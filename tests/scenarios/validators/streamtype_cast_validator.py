#!/usr/bin/env python3
#
# Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
Validator for the Stream-Type Conversions test.
"""

import sys
import os
import glob

try:
    import zarr
    import numpy as np
except ImportError as e:
    print(f"Required Python package missing: {e}", file=sys.stderr)
    print("Install with: pip install zarr numpy", file=sys.stderr)
    sys.exit(1)


# Minimum samples we expect after the configured run duration. The DataSource is
# set to 200 Hz; we run for several seconds. A handful of samples comfortably
# avoids flakiness from startup/teardown latency while still catching the
# silent-no-data failure mode.
MIN_SAMPLES = 8


def validate_converted_zarr(store_path: str, expected_dtype: str, conversion_desc: str) -> None:
    print(f"  Checking store: {store_path}")

    try:
        root = zarr.open_group(store_path, mode="r")
    except Exception as e:
        raise RuntimeError(f"Cannot open Zarr store at '{store_path}': {e}") from e

    if "timestamps" not in root:
        raise RuntimeError(f"Store '{store_path}' is missing the 'timestamps' array")
    if "data" not in root:
        raise RuntimeError(f"Store '{store_path}' is missing the 'data' array")

    ts = root["timestamps"]
    data = root["data"]

    if ts.ndim != 1:
        raise RuntimeError(f"timestamps must be 1-D, got shape {ts.shape}")
    # The Zarr writer stores single-channel signals as a 1-D array and
    # multi-channel signals as 2-D [samples, channels]
    if data.ndim not in (1, 2):
        raise RuntimeError(f"data must be 1-D or 2-D, got shape {data.shape}")
    n_channels = 1 if data.ndim == 1 else data.shape[1]

    if ts.dtype != np.dtype("uint64"):
        raise RuntimeError(f"timestamps dtype must be uint64, got {ts.dtype}")
    if data.dtype != np.dtype(expected_dtype):
        raise RuntimeError(
            f"data dtype must be {expected_dtype} (proves {conversion_desc} conversion ran), "
            f"got {data.dtype}"
        )

    n_samples = ts.shape[0]
    if n_samples != data.shape[0]:
        raise RuntimeError(f"Row count mismatch: timestamps={n_samples}, data={data.shape[0]}")
    if n_samples < MIN_SAMPLES:
        raise RuntimeError(
            f"Too few samples in '{store_path}': got {n_samples}, expected >= {MIN_SAMPLES}"
        )

    ts_arr = ts[:]
    if np.any(np.diff(ts_arr.astype(np.int64)) < 0):
        raise RuntimeError(f"Timestamps in '{store_path}' are not monotonically non-decreasing")

    print(f"    OK: {n_samples} samples × {n_channels} channel(s), dtype={data.dtype}")


# Map between the dataset names in the project and the zarr files they should
# produce, along with the data dtype each conversion is expected to yield.
EXPECTED_STORES = {
    "u16-to-i32-direct": ("u16-to-i32-direct.zarr", "int32", "U16→I32"),
    "u16-to-i32-mlinkpy": ("u16-to-i32-mlinkpy.zarr", "int32", "U16→I32"),
    "i32-to-f32-direct": ("i32-to-f32-direct.zarr", "float32", "I32→F32"),
}


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: streamtype_cast_validator.py <export_dir>", file=sys.stderr)
        return 1

    export_dir = sys.argv[1]
    if not os.path.isdir(export_dir):
        print(f"Export directory not found: {export_dir}", file=sys.stderr)
        return 1

    errors = []
    for dataset_name, (store_basename, expected_dtype, conversion_desc) in EXPECTED_STORES.items():
        matches = glob.glob(
            os.path.join(export_dir, "**", dataset_name, store_basename),
            recursive=True,
        )
        if not matches:
            errors.append(
                f"Expected store '{dataset_name}/{store_basename}' not found under {export_dir}"
            )
            continue
        for store in matches:
            try:
                validate_converted_zarr(store, expected_dtype, conversion_desc)
            except RuntimeError as e:
                errors.append(str(e))

    if errors:
        for err in errors:
            print(f"  [!] {err}", file=sys.stderr)
        return 1

    print(f"  Validated {len(EXPECTED_STORES)} converted Zarr store(s) successfully.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
