#!/usr/bin/env python3
#
# Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
Validator for the Zarr Writer module test.

Checks that each generated *.zarr store is a valid Zarr v3 group containing
readable 'timestamps' (uint64, 1-D) and 'data' (float64 or int32, 2-D) arrays
with consistent row counts and at least some samples.

Usage: zarr_writer_validator.py <export_dir>
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


def validate_zarr_store(store_path: str) -> None:
    """Open a zarr store and validate timestamps + data arrays."""
    print(f"  Checking store: {store_path}")

    try:
        root = zarr.open_group(store_path, mode="r")
    except Exception as e:
        raise RuntimeError(f"Cannot open Zarr store at '{store_path}': {e}") from e

    # Must have both sub-arrays
    if "timestamps" not in root:
        raise RuntimeError(f"Store '{store_path}' is missing the 'timestamps' array")
    if "data" not in root:
        raise RuntimeError(f"Store '{store_path}' is missing the 'data' array")

    ts = root["timestamps"]
    data = root["data"]

    # Basic shape checks
    if ts.ndim != 1:
        raise RuntimeError(f"timestamps array must be 1-D, got shape {ts.shape}")
    if data.ndim not in (1, 2):
        raise RuntimeError(f"data array must be 1-D or 2-D, got shape {data.shape}")

    n_samples = ts.shape[0]
    if n_samples == 0:
        raise RuntimeError("timestamps array is empty - no data was written")
    if data.shape[0] != n_samples:
        raise RuntimeError(
            f"Row count mismatch: timestamps has {n_samples} rows, "
            f"data has {data.shape[0]} rows"
        )

    # dtype checks
    if ts.dtype != np.dtype("uint64"):
        raise RuntimeError(f"timestamps dtype should be uint64, got {ts.dtype}")
    if data.dtype not in (np.dtype("float64"), np.dtype("int32")):
        raise RuntimeError(f"data dtype should be float64 or int32, got {data.dtype}")

    # Actually read the data to verify decompression works
    ts_arr = ts[:]
    data_arr = data[:]

    if ts_arr.shape[0] == 0:
        raise RuntimeError("timestamps array read back as empty")
    if data_arr.shape[0] == 0:
        raise RuntimeError("data array read back as empty")

    # Timestamps must be monotonically non-decreasing
    if np.any(np.diff(ts_arr.astype(np.int64)) < 0):
        raise RuntimeError("timestamps are not monotonically non-decreasing")

    n_channels = data.shape[1] if data.ndim == 2 else 1
    print(
        f"    OK: {n_samples} samples, {n_channels} channel(s), "
        f"dtype={data.dtype}, ts_dtype={ts.dtype}"
    )


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: zarr_writer_validator.py <export_dir>", file=sys.stderr)
        return 1

    export_dir = sys.argv[1]
    if not os.path.isdir(export_dir):
        print(f"Export directory not found: {export_dir}", file=sys.stderr)
        return 1

    # Find all *.zarr directories anywhere under the export tree
    zarr_stores = glob.glob(os.path.join(export_dir, "**", "*.zarr"), recursive=True)

    if not zarr_stores:
        print(f"No *.zarr stores found under {export_dir}", file=sys.stderr)
        return 1

    errors = []
    for store in sorted(zarr_stores):
        try:
            validate_zarr_store(store)
        except RuntimeError as e:
            errors.append(str(e))

    if errors:
        for err in errors:
            print(f"  [!] {err}", file=sys.stderr)
        return 1

    print(f"  Validated {len(zarr_stores)} Zarr store(s) successfully.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
