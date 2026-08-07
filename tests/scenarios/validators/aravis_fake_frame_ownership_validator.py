#!/usr/bin/env python3
#
# Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Check that queued Aravis frames retain their original pixel contents."""

import os
import sys

import edlio
import numpy as np


def find_collection_root(export_dir):
    if os.path.isfile(os.path.join(export_dir, "manifest.toml")):
        return export_dir
    for entry in sorted(os.listdir(export_dir)):
        candidate = os.path.join(export_dir, entry)
        if os.path.isfile(os.path.join(candidate, "manifest.toml")):
            return candidate
    raise RuntimeError(f"No EDL collection found below {export_dir}")


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} EXPORT_DIR", file=sys.stderr)
        return 2

    collection = edlio.load(find_collection_root(sys.argv[1]))
    videos = collection.group_by_name("videos")
    if videos is None:
        print("Missing EDL group: videos", file=sys.stderr)
        return 1

    dataset = videos.dataset_by_name("fake-camera")
    if dataset is None:
        print("Missing EDL dataset: videos/fake-camera", file=sys.stderr)
        return 1

    previous = None
    frame_count = 0
    duplicate_pairs = []
    duplicate_run_start = None
    duplicate_runs = []

    for frame in dataset.read_data():
        is_duplicate = previous is not None and np.array_equal(previous, frame.mat)
        if is_duplicate:
            duplicate_pairs.append((frame_count - 1, frame_count))
            if duplicate_run_start is None:
                duplicate_run_start = frame_count - 1
        elif duplicate_run_start is not None:
            duplicate_runs.append((duplicate_run_start, frame_count - 1))
            duplicate_run_start = None

        previous = frame.mat.copy()
        frame_count += 1

    if duplicate_run_start is not None:
        duplicate_runs.append((duplicate_run_start, frame_count - 1))

    if duplicate_pairs:
        longest_run = max(duplicate_runs, key=lambda run: run[1] - run[0])
        print(
            f"Found {len(duplicate_pairs)} exact consecutive duplicate pair(s) "
            f"in {frame_count} losslessly decoded frames; longest identical run: "
            f"frames {longest_run[0]}-{longest_run[1]}.",
            file=sys.stderr,
        )
        return 1

    print(f"No exact consecutive duplicates in {frame_count} losslessly decoded frames.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
