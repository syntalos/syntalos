#!/usr/bin/env python3
#
# Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
Validator for the ONNX Inference image test.

The ONNX Inference module runs a model that outputs the mean of each colour channel,
decoded as class scores. The Video Recorder stores the same source frames. We check:
 * the table header and that enough rows arrived,
 * that the scores are plausible and the label belongs to the best score,
 * that every row carries the timestamp of the frame it was computed from, i.e. one of
   the frame timestamps the Video Recorder stored, and not the time the result was ready,
 * that the run record holds the recipe and the model's checksum.
"""

import glob
import hashlib
import json
import os
import sys
import tomllib

try:
    import edlio
    import numpy as np
    import zstandard as zstd
except ImportError as e:
    print(f"Required Python package missing: {e}", file=sys.stderr)
    sys.exit(1)

EXPECTED_COLUMNS = [
    "time_usec",
    "frame_index",
    "step",
    "argmax",
    "label",
    "score_red",
    "score_green",
    "score_blue",
]
LABELS = ["red", "green", "blue"]

# The DataSource emits 30 frames/s and the run lasts several seconds. The model is tiny,
# so almost no frame should be skipped; a generous minimum avoids startup flakiness.
MIN_ROWS = 40


def find_collection_root(export_dir: str) -> str:
    if os.path.exists(os.path.join(export_dir, "manifest.toml")):
        return export_dir
    for entry in sorted(os.listdir(export_dir)):
        sub = os.path.join(export_dir, entry)
        if os.path.isdir(sub) and os.path.exists(os.path.join(sub, "manifest.toml")):
            return sub
    return export_dir


def load_rows(root: str):
    files = glob.glob(os.path.join(root, "onnx-rows", "*.json.zst"))
    if len(files) != 1:
        raise RuntimeError(f"Expected one JSON file in onnx-rows, found: {files}")
    with open(files[0], "rb") as f:
        doc = json.loads(zstd.ZstdDecompressor().stream_reader(f).read())
    if "columns" not in doc or "data" not in doc:
        raise RuntimeError(f"Unexpected JSON layout, keys: {list(doc.keys())}")
    return doc["columns"], doc["data"]


def load_frame_times_usec(root: str) -> np.ndarray:
    collection = edlio.load(root)
    videos = collection.group_by_name("videos")
    if videos is None:
        raise RuntimeError("No 'videos' group in the collection")
    dset = videos.dataset_by_name("source-frames")
    if dset is None:
        raise RuntimeError("No 'videos/source-frames' dataset in the collection")
    tsf = next(dset.read_aux_data("tsync"))
    labels = list(tsf.time_labels)
    if "master-time" not in labels:
        raise RuntimeError(f"Tsync file has no master-time column: {labels}")
    col = labels.index("master-time")
    unit = str(tsf.time_units[col])
    times = tsf.times[:, col].astype(np.int64)
    if "milli" in unit:
        times *= 1000
    elif "micro" not in unit:
        raise RuntimeError(f"Unexpected master-time unit: {unit}")
    return times


def check_rows(columns, data, frame_times: np.ndarray) -> None:
    if columns != EXPECTED_COLUMNS:
        raise RuntimeError(f"Unexpected columns: {columns}")
    if len(data) < MIN_ROWS:
        raise RuntimeError(f"Only {len(data)} rows, expected at least {MIN_ROWS}")

    known_times = set(int(t) for t in frame_times)
    last_time = -1
    last_index = -1
    for i, row in enumerate(data):
        values = dict(zip(columns, row))
        time_usec = int(values["time_usec"])
        frame_index = int(values["frame_index"])
        scores = [float(values[f"score_{name}"]) for name in LABELS]

        if time_usec not in known_times:
            raise RuntimeError(
                f"Row {i}: time {time_usec} µs is not the timestamp of any recorded frame"
            )
        if time_usec <= last_time or frame_index <= last_index:
            raise RuntimeError(f"Row {i}: time or frame index does not increase")
        last_time = time_usec
        last_index = frame_index

        if int(values["step"]) != 0:
            raise RuntimeError(f"Row {i}: unexpected step {values['step']}")
        if not all(0.0 <= s <= 1.0 for s in scores):
            raise RuntimeError(f"Row {i}: scores outside of 0…1: {scores}")
        best = int(values["argmax"])
        if scores[best] != max(scores):
            raise RuntimeError(f"Row {i}: argmax {best} is not the best score of {scores}")
        if values["label"] != LABELS[best]:
            raise RuntimeError(f"Row {i}: label {values['label']} does not belong to argmax {best}")

    print(
        f"  OK: {len(data)} rows for {len(frame_times)} recorded frames, all with frame timestamps"
    )


def check_run_record(root: str) -> None:
    recipes = glob.glob(os.path.join(root, "**", "recipe.toml"), recursive=True)
    if len(recipes) != 1:
        raise RuntimeError(f"Expected one recipe.toml run record, found: {recipes}")
    with open(recipes[0], "rb") as f:
        recipe = tomllib.load(f)

    if recipe["input"]["adapter"] != "image" or recipe["output"]["decoder"] != "scores":
        raise RuntimeError(f"Unexpected recipe units: {recipe['input']}, {recipe['output']}")
    if recipe["output"]["labels"] != LABELS:
        raise RuntimeError(f"Unexpected labels in recipe: {recipe['output']['labels']}")

    model_file = recipe["model"]["file"]
    with open(model_file, "rb") as f:
        sha = hashlib.sha256(f.read()).hexdigest()
    if recipe["model"]["sha256"] != sha:
        raise RuntimeError(f"Model checksum {recipe['model']['sha256']} does not match {sha}")

    with open(os.path.join(os.path.dirname(recipes[0]), "attributes.toml"), "rb") as f:
        attrs = tomllib.load(f)
    for key in ("model_sha256", "ort_version", "execution_provider", "input_shape"):
        if key not in attrs:
            raise RuntimeError(f"Run record attribute '{key}' is missing: {attrs}")
    if attrs["input_shape"] != [1, 3, 48, 64]:
        raise RuntimeError(f"Unexpected input shape {attrs['input_shape']}")

    print(f"  OK: run record with recipe, model checksum and ONNX Runtime {attrs['ort_version']}")


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: onnx_inference_validator.py EXPORT_DIR", file=sys.stderr)
        return 2

    root = find_collection_root(sys.argv[1])
    try:
        columns, data = load_rows(root)
        check_rows(columns, data, load_frame_times_usec(root))
        check_run_record(root)
    except Exception as e:
        print(f"Validation failed: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
