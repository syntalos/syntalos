#!/usr/bin/env python3
#
# Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
Validator for the Signal Filter end-to-end test.

signalfilter-test.syct feeds a deterministic
two-tone signal (low + high) into two filter chains and also records the raw
input, producing three Zarr datasets:

  raw_input            unfiltered source (the reference input)
  filtered_butter_hp   Butterworth high-pass, order 4, cutoff 100 Hz
  filtered_custom_lp   Custom-SOS low-pass = scipy.butter(4, 100, 'low', fs)

Because signalfilter passes the source timestamps through unchanged, raw and
filtered samples can be aligned exactly by timestamp. We then prove correctness
two ways:

  * Reference match: filter the recorded raw with the *same* design in SciPy and
    compare sample-for-sample (tight for the Custom-SOS chain, which uses the
    identical coefficients; looser for the module's own Butterworth design).
  * Spectral: the high-pass must suppress the low tone and keep the high tone;
    the low-pass must do the inverse.
"""

import sys
import os
import glob

try:
    import numpy as np
    import zarr
    from scipy.signal import butter, sosfilt
except ImportError as e:
    print(f"Required Python package missing: {e}", file=sys.stderr)
    print("Install with: pip install zarr numpy scipy", file=sys.stderr)
    sys.exit(1)


# --- shared contract ---
FS = 2000.0
CUTOFF_HZ = 100.0
ORDER = 4
FREQ_LOW = 10.0
FREQ_HIGH = 300.0

# Minimum aligned samples we expect after an ~8 s run (effective rate ~2 kHz).
MIN_SAMPLES = 2000
# Samples to drop at the head before comparing: lets the IIR transient (and any
# tiny start-of-recording offset between the raw and filtered writers) decay.
SETTLE = int(0.5 * FS)


def find_store(export_dir: str, dataset: str) -> str:
    """Locate the .zarr store for a named dataset under the export dir."""
    candidates = [
        c
        for c in glob.glob(os.path.join(export_dir, "**", "*.zarr"), recursive=True)
        if (os.sep + dataset + os.sep) in (c + os.sep)
    ]
    if not candidates:
        raise RuntimeError(f"No .zarr store found for dataset '{dataset}' under {export_dir}")
    return candidates[0]


def load_store(export_dir: str, dataset: str):
    """Return (data[float64, N x C], timestamps[uint64, N]) for a dataset."""
    path = find_store(export_dir, dataset)
    root = zarr.open_group(path, mode="r")
    if "timestamps" not in root or "data" not in root:
        raise RuntimeError(f"Store '{path}' is missing 'timestamps' or 'data'")

    ts = root["timestamps"][:]
    data = root["data"][:]

    if ts.ndim != 1:
        raise RuntimeError(f"{dataset}: timestamps must be 1-D, got {ts.shape}")
    if data.ndim != 2:
        raise RuntimeError(f"{dataset}: data must be 2-D, got {data.shape}")
    if ts.dtype != np.dtype("uint64"):
        raise RuntimeError(f"{dataset}: timestamps dtype must be uint64, got {ts.dtype}")
    if data.dtype != np.dtype("float32"):
        raise RuntimeError(f"{dataset}: data dtype must be float32, got {data.dtype}")
    if ts.shape[0] != data.shape[0]:
        raise RuntimeError(f"{dataset}: row mismatch timestamps={ts.shape[0]} data={data.shape[0]}")
    if np.any(np.diff(ts.astype(np.int64)) < 0):
        raise RuntimeError(f"{dataset}: timestamps are not monotonically non-decreasing")

    print(f"    loaded {dataset}: {data.shape[0]} samples x {data.shape[1]} ch")
    return data.astype(np.float64), ts


def align(raw_data, raw_ts, filt_data, filt_ts):
    """Align raw and filtered samples by shared timestamps."""
    _, idx_raw, idx_filt = np.intersect1d(raw_ts, filt_ts, return_indices=True)
    if idx_raw.size < MIN_SAMPLES:
        raise RuntimeError(
            f"Too few aligned samples: {idx_raw.size} (need >= {MIN_SAMPLES}). "
            "Raw and filtered timestamps may not correspond."
        )
    return raw_data[idx_raw], filt_data[idx_filt]


def tone_mag(sig: np.ndarray, freq: float) -> float:
    """Magnitude of a tone at `freq` in a 1-D signal (Hann-windowed FFT bin)."""
    n = sig.size
    win = np.hanning(n)
    spec = np.fft.rfft((sig - sig.mean()) * win)
    freqs = np.fft.rfftfreq(n, d=1.0 / FS)
    k = int(np.argmin(np.abs(freqs - freq)))
    return float(np.abs(spec[k]))


def db(ratio: float) -> float:
    return 20.0 * np.log10(max(ratio, 1e-12))


def check_reference(name, raw_a, filt_a, sos, atol, errors):
    """Compare the recorded filtered output against an independent SciPy run."""
    ref = sosfilt(sos, raw_a, axis=0)
    diff = np.abs(filt_a[SETTLE:] - ref[SETTLE:])
    max_diff = float(diff.max())
    print(f"    {name}: max|recorded - scipy reference| (post-settle) = {max_diff:.3e}")
    if max_diff > atol:
        errors.append(
            f"{name}: output deviates from SciPy reference by {max_diff:.3e} > {atol:.1e}"
        )


def check_spectral(name, raw_a, filt_a, attenuated_freq, preserved_freq, errors, ch=2):
    """Assert one tone is suppressed and the other preserved on a channel."""
    raw = raw_a[SETTLE:, ch]
    filt = filt_a[SETTLE:, ch]

    att = db(tone_mag(raw, attenuated_freq) / tone_mag(filt, attenuated_freq))
    keep = abs(db(tone_mag(raw, preserved_freq) / tone_mag(filt, preserved_freq)))
    print(
        f"    {name}: {attenuated_freq:g} Hz attenuated by {att:.1f} dB, "
        f"{preserved_freq:g} Hz changed by {keep:.1f} dB"
    )
    if att < 20.0:
        errors.append(
            f"{name}: expected >=20 dB attenuation at {attenuated_freq:g} Hz, got {att:.1f} dB"
        )
    if keep > 3.0:
        errors.append(f"{name}: expected <=3 dB change at {preserved_freq:g} Hz, got {keep:.1f} dB")


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: signalfilter_validator.py <export_dir>", file=sys.stderr)
        return 1
    export_dir = sys.argv[1]
    if not os.path.isdir(export_dir):
        print(f"Export directory not found: {export_dir}", file=sys.stderr)
        return 1

    errors: list[str] = []
    try:
        raw_data, raw_ts = load_store(export_dir, "raw_input")
        hp_data, hp_ts = load_store(export_dir, "filtered_butter_hp")
        lp_data, lp_ts = load_store(export_dir, "filtered_custom_lp")
    except RuntimeError as e:
        print(f"  [!] {e}", file=sys.stderr)
        return 1

    # Reference designs (identical to those used in the project).
    sos_lp = butter(ORDER, CUTOFF_HZ, btype="low", fs=FS, output="sos")
    sos_hp = butter(ORDER, CUTOFF_HZ, btype="high", fs=FS, output="sos")

    # --- Custom-SOS low-pass: uses the exact same coefficients -> tight match.
    raw_a, lp_a = align(raw_data, raw_ts, lp_data, lp_ts)
    check_reference("custom_lp", raw_a, lp_a, sos_lp, atol=5e-3, errors=errors)
    check_spectral("custom_lp", raw_a, lp_a, FREQ_HIGH, FREQ_LOW, errors)

    # --- Butterworth high-pass: module's own design -> looser sample match,
    #     spectral attenuation is the authoritative gate.
    raw_b, hp_a = align(raw_data, raw_ts, hp_data, hp_ts)
    check_reference("butter_hp", raw_b, hp_a, sos_hp, atol=5e-2, errors=errors)
    check_spectral("butter_hp", raw_b, hp_a, FREQ_LOW, FREQ_HIGH, errors)

    if errors:
        for err in errors:
            print(f"  [!] {err}", file=sys.stderr)
        return 1

    print("  Signal filter output validated successfully.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
