#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Example: browse the electrophysiology data of an EDL recording.
#
# The program opens a directory-selection dialog to pick an EDL collection,
# group or dataset, recursively scans it for ephys datasets (Intan ``rhd``,
# ``zarr`` arrays or numeric ``json`` tables) and displays each one as scrolling
# traces in its own panel, using ``ephyviewer``.
#
# Run it with no arguments to get the directory dialog, or pass an EDL path
# directly to skip it::
#
#     python view_ephys.py [EDL_DIRECTORY]
#
# Pass ``--list`` to only print the ephys datasets that were found (no GUI),
# which is handy for testing without a display::
#
#     python view_ephys.py --list tests/samples/zarrtest1
#
# See README.md for the required dependencies (notably a Qt binding such as
# PySide6, which ephyviewer needs to open a window).

from __future__ import annotations

import os
import sys
import typing as T

import numpy as np

import edlio
from edlio.group import EDLGroup
from edlio.dataset import EDLDataset

# how many seconds of time-unit map onto one second, keyed by the ``time_unit``
# attribute that edlio writes into zarr stores and json attributes. A timestamp
# vector whose unit is *not* in here (e.g. ``"index"``) holds sample indices
# rather than physical time, so its sample rate cannot be read off the values.
_TIME_UNIT_TO_SECONDS = {
    'nanoseconds': 1e-9,
    'microseconds': 1e-6,
    'milliseconds': 1e-3,
    'seconds': 1.0,
}

# last-resort sample rate (Hz) when timestamps are sample indices and the
# recording length is unknown; 30 kHz is typical for Intan/Open Ephys rigs
_FALLBACK_SAMPLE_RATE = 30000.0


class EphysDataset(T.NamedTuple):
    """A discovered ephys dataset and how to address it for the user."""

    dataset: EDLDataset
    kind: str  # 'rhd' | 'zarr' | 'json'
    label: str  # human-readable, e.g. "videos/intan-signals  [rhd]"


def iter_datasets(unit: T.Any, prefix: str = '') -> T.Iterator[tuple[str, EDLDataset]]:
    """Recursively yield ``(path, dataset)`` pairs below a loaded EDL unit.

    ``unit`` may be an :class:`EDLCollection`, :class:`EDLGroup` or a bare
    :class:`EDLDataset` (``edlio.load`` can return any of these).
    """
    if isinstance(unit, EDLDataset):
        yield (unit.name or '', unit)
        return
    if isinstance(unit, EDLGroup):
        for dset in unit.datasets:
            name = dset.name or ''
            yield (prefix + name, dset)
        for group in unit.groups:
            sub_prefix = prefix + (group.name or '') + '/'
            yield from iter_datasets(group, sub_prefix)


def classify(dataset: EDLDataset) -> str | None:
    """Return 'rhd', 'zarr' or 'json' for a dataset, or None if not ephys-like.

    The reasoning mirrors edlio's own loader dispatch in
    ``EDLDataFile.read`` (edlio/dataset.py).
    """
    file_type = (dataset.data.file_type or '').lower()
    media_type = (dataset.data.media_type or '').lower()

    if file_type == 'rhd':
        return 'rhd'
    if file_type == 'zarr':
        return 'zarr'
    if file_type == 'json' or file_type == 'json.zst' or 'json' in media_type:
        return 'json'
    return None


def _sample_rate_from_timestamps(timestamps: np.ndarray, time_unit: str) -> tuple[float, float]:
    """Derive a uniform sample rate (Hz) and start time (s) from a *physical* timestamp vector.

    The rate is taken from the total span (``(N - 1) / (t_last - t_first)``)
    rather than the per-sample spacing: Syntalos timestamps are not strictly
    equidistant (they can arrive in near-duplicate pairs or with jitter), which
    would make a ``median(diff)`` estimate wildly wrong. The span-based estimate
    also yields the correct overall duration under ephyviewer's uniform model.
    """
    factor = _TIME_UNIT_TO_SECONDS[time_unit]
    ts_s = timestamps.astype(np.float64) * factor
    if ts_s.size < 2:
        return 1.0, float(ts_s[0]) if ts_s.size else 0.0
    duration = float(ts_s[-1] - ts_s[0])
    sample_rate = (ts_s.size - 1) / duration if duration > 0 else 1.0
    return sample_rate, float(ts_s[0])


# gap-filling onto the index grid is capped at this many times the recorded
# sample count, so a huge pause in the data can't blow up memory
_MAX_INDEX_EXPANSION = 8


def _rate_from_indices(n_samples: int, recording_length_msec: float | None) -> float:
    """Best-effort sample rate (Hz) when no explicit ``sample_rate`` is stored.

    Index timestamps are sample counters, so the rate is not in their values.
    Recover it from the collection's recorded duration, falling back to a
    typical ephys rate.
    """
    if recording_length_msec and recording_length_msec > 0 and n_samples > 1:
        return n_samples / (float(recording_length_msec) / 1000.0)
    print(
        'Warning: timestamps are sample indices and no sample_rate or recording '
        'length is available; assuming {:.0f} Hz.'.format(_FALLBACK_SAMPLE_RATE),
        file=sys.stderr,
    )
    return _FALLBACK_SAMPLE_RATE


def _regrid_by_index(
    data: np.ndarray, timestamps: np.ndarray, sample_rate: float
) -> tuple[np.ndarray, float]:
    """Place index-timestamped rows onto a uniform grid and return (data, t_start).

    With ``time_unit == "index"`` each row's true position is its sample index
    (at ``sample_rate``). Syntalos' time-sync can leave a start offset and, in
    principle, non-equidistant indices (gaps). ephyviewer assumes a constant
    rate, so we expand the data onto the full integer-index grid (gaps
    forward-filled with the preceding sample) and set ``t_start`` from the first
    index. For the common gap-free recording this is a no-op apart from
    ``t_start``.
    """
    idx = timestamps.astype(np.int64) - int(timestamps[0])
    n = data.shape[0]
    t_start = float(timestamps[0]) / sample_rate
    if idx.size == 0:
        return data, 0.0

    span = int(idx[-1]) + 1
    if span == n:
        # already gap-free and equidistant
        return data, t_start
    if span > _MAX_INDEX_EXPANSION * n:
        print(
            'Warning: large gaps in sample indices ({} recorded vs {} index span); '
            'showing data without gap correction.'.format(n, span),
            file=sys.stderr,
        )
        return data, t_start

    # for every grid point, take the most recent recorded sample (forward fill)
    src = np.searchsorted(idx, np.arange(span), side='right') - 1
    np.clip(src, 0, n - 1, out=src)
    return data[src], t_start


def _load_rhd(dataset: EDLDataset) -> tuple[np.ndarray, float, float, list[str]]:
    """Read an Intan RHD dataset into a (samples, channels) float array."""
    # we only need a uniform sample rate here, so skip the (slower) time-sync
    readers = list(dataset.read_data(do_timesync=False))
    if not readers:
        raise ValueError('RHD dataset contains no data parts.')

    chunks = []
    sample_rate = None
    channel_names: list[str] = []
    for reader in readers:
        reader.parse_header()
        raw = reader.get_analogsignal_chunk(stream_index=0)
        chunks.append(reader.rescale_signal_raw_to_float(raw, stream_index=0))
        if sample_rate is None:
            sample_rate = float(reader.get_signal_sampling_rate(stream_index=0))
            sig_channels = reader.header['signal_channels']
            stream_id = reader.header['signal_streams'][0]['id']
            mask = sig_channels['stream_id'] == stream_id
            channel_names = [str(name) for name in sig_channels[mask]['name']]

    signals = np.concatenate(chunks, axis=0)
    return signals, float(sample_rate), 0.0, channel_names


def _resolve_timebase(
    data: np.ndarray,
    timestamps: np.ndarray | None,
    time_unit: str,
    explicit_rate: T.Any,
    data_scale: T.Any,
    data_offset: T.Any,
    recording_length_msec: float | None,
) -> tuple[np.ndarray, float, float]:
    """Scale a signal block and resolve its (signals, sample_rate, t_start).

    Shared by the zarr and JSON loaders, which both may carry an explicit
    ``sample_rate``, an affine ``data_scale``/``data_offset`` transform, and
    timestamps that are either physical (``time_unit`` of microseconds, …) or
    sample indices (``time_unit == "index"``).
    """
    # recover physical units (e.g. µV) when an affine transform is given
    if data_scale is not None:
        data = data * float(data_scale)
    if data_offset is not None:
        data = data + float(data_offset)

    if timestamps is not None and time_unit in _TIME_UNIT_TO_SECONDS:
        # physical timestamps: take t_start from them; trust an explicit rate
        # over the (noisier) value derived from sample spacing
        rate_from_ts, t_start = _sample_rate_from_timestamps(timestamps, time_unit)
        sample_rate = float(explicit_rate) if explicit_rate else rate_from_ts
    else:
        # index-based (or missing) timestamps: the rows are sample counters, so
        # use the stored rate (or recover it) and re-grid onto the index grid to
        # honour the start offset and any gaps
        if explicit_rate is not None:
            sample_rate = float(explicit_rate)
        else:
            sample_rate = _rate_from_indices(data.shape[0], recording_length_msec)
        if timestamps is not None:
            data, t_start = _regrid_by_index(data, timestamps, sample_rate)
        else:
            t_start = 0.0

    return data, sample_rate, t_start


def _load_zarr(
    dataset: EDLDataset, recording_length_msec: float | None
) -> tuple[np.ndarray, float, float, list[str]]:
    """Read a zarr signal store into a (samples, channels) float array."""
    stores = list(dataset.read_data())
    if not stores:
        raise ValueError('Zarr dataset contains no data parts.')
    root = stores[0]

    data = np.asarray(root['data'][:], dtype=np.float64)
    if data.ndim == 1:
        data = data.reshape(-1, 1)

    attrs = dict(root['data'].attrs)
    channel_names = [str(n) for n in attrs.get('signal_names', [])]
    if len(channel_names) != data.shape[1]:
        channel_names = ['ch{}'.format(i) for i in range(data.shape[1])]

    # the time unit / sample rate live on the timestamps array (real Syntalos
    # data) or on the data array; prefer the timestamps array
    timestamps = None
    time_unit = str(attrs.get('time_unit', 'microseconds'))
    explicit_rate = attrs.get('sample_rate')
    if 'timestamps' in root:
        timestamps = np.asarray(root['timestamps'][:])
        ts_attrs = dict(root['timestamps'].attrs)
        if 'time_unit' in ts_attrs:
            time_unit = str(ts_attrs['time_unit'])
        if explicit_rate is None:
            explicit_rate = ts_attrs.get('sample_rate')

    data, sample_rate, t_start = _resolve_timebase(
        data,
        timestamps,
        time_unit,
        explicit_rate,
        attrs.get('data_scale'),
        attrs.get('data_offset'),
        recording_length_msec,
    )
    return data, sample_rate, t_start, channel_names


def _load_json(
    dataset: EDLDataset, recording_length_msec: float | None
) -> tuple[np.ndarray, float, float, list[str]] | None:
    """Read a numeric JSON table into a (samples, channels) float array.

    Returns None for tables that hold no numeric signal columns (e.g. string
    event tables), as those are not ephys traces.
    """
    import pandas as pd

    frames = list(dataset.read_data())
    if not frames:
        return None
    df = pd.concat(frames, ignore_index=True)

    # a 'timestamp_*' (or 'time') column, if present, carries the time/indices
    time_col = None
    for col in df.columns:
        if str(col).lower().startswith('timestamp') or str(col).lower() == 'time':
            time_col = col
            break

    signal_cols = [c for c in df.columns if c != time_col and pd.api.types.is_numeric_dtype(df[c])]
    if not signal_cols:
        return None

    data = df[signal_cols].to_numpy(dtype=np.float64)

    # newer recordings store the same time_unit + sample_rate as zarr; older
    # ones only a physical json_time_unit describing the timestamp column
    attrs = dataset.attributes
    time_unit = str(attrs.get('time_unit') or 'milliseconds')
    timestamps = df[time_col].to_numpy() if time_col is not None else None
    data, sample_rate, t_start = _resolve_timebase(
        data,
        timestamps,
        time_unit,
        attrs.get('sample_rate'),
        attrs.get('data_scale'),
        attrs.get('data_offset'),
        recording_length_msec,
    )
    return data, sample_rate, t_start, [str(c) for c in signal_cols]


def load_signals(
    dataset: EDLDataset, kind: str, recording_length_msec: float | None = None
) -> tuple[np.ndarray, float, float, list[str]] | None:
    """Convert a dataset to ``(signals, sample_rate, t_start, channel_names)``.

    ``signals`` is shaped ``(nb_sample, nb_channel)`` as expected by
    ephyviewer's :class:`InMemoryAnalogSignalSource`. Returns None when the
    dataset turns out to hold no numeric signals. ``recording_length_msec`` (the
    collection's recorded duration) is used to recover the sample rate of
    datasets whose timestamps are sample indices rather than physical time.
    """
    if kind == 'rhd':
        return _load_rhd(dataset)
    if kind == 'zarr':
        return _load_zarr(dataset, recording_length_msec)
    if kind == 'json':
        return _load_json(dataset, recording_length_msec)
    raise ValueError('Unknown ephys dataset kind: {}'.format(kind))


def scan(path: str) -> tuple[list[EphysDataset], float | None]:
    """Load an EDL path and return its ephys datasets and recorded duration.

    The second element is the collection's ``recording_length_msec`` attribute
    (or None), needed to recover the sample rate of index-timestamped datasets.
    """
    root = edlio.load(path)
    recording_length_msec = None
    attrs = getattr(root, 'attributes', None)
    if attrs:
        recording_length_msec = attrs.get('recording_length_msec')

    found: list[EphysDataset] = []
    for dpath, dset in iter_datasets(root):
        kind = classify(dset)
        if kind is None:
            continue
        found.append(EphysDataset(dset, kind, '{}  [{}]'.format(dpath, kind)))
    return found, recording_length_msec


def _run_list_mode(path: str) -> int:
    """Print discovered ephys datasets and their signal shapes; no GUI."""
    datasets, rec_len_msec = scan(path)
    if not datasets:
        print('No ephys datasets (rhd/zarr/json) found in: {}'.format(path))
        return 1

    print('Found {} ephys dataset(s) in {}:'.format(len(datasets), path))
    for entry in datasets:
        try:
            loaded = load_signals(entry.dataset, entry.kind, rec_len_msec)
        except Exception as e:  # noqa: BLE001  (diagnostic listing only)
            print('  - {}: ERROR: {}'.format(entry.label, e))
            continue
        if loaded is None:
            print('  - {}: no numeric signals (skipped)'.format(entry.label))
            continue
        signals, sample_rate, t_start, channel_names = loaded
        print(
            '  - {}: {} samples x {} channel(s), {:.2f} Hz, t_start={:.4f} s, channels={}'.format(
                entry.label,
                signals.shape[0],
                signals.shape[1],
                sample_rate,
                t_start,
                channel_names,
            )
        )
    return 0


def _apply_progressive_colors(view: T.Any) -> None:
    """Give a TraceViewer's channels progressive hues."""
    import pyqtgraph as pg

    n = view.source.nb_channel
    for c in range(n):
        color = pg.intColor(c, hues=max(n, 1)).name()
        view.by_channel_params['ch{}'.format(c), 'color'] = color


def _auto_scale(view: T.Any) -> None:
    """Auto-scale a TraceViewer, robustly handling degenerate signals.

    The default ``same_for_all`` mode scales by the median absolute deviation,
    which is zero for pulse-like channels (e.g. TTL/sync recorded on an ADC:
    mostly 0 with occasional pulses). That divides by zero and leaves the panel
    blank, so for such views we fall back to ``real_scale`` (min/max based).
    """
    with np.errstate(divide='ignore', invalid='ignore'):
        view.auto_scale()
        gains = getattr(view.params_controller, 'gains', None)
        if gains is not None and not np.all(np.isfinite(gains)):
            view.params['scale_mode'] = 'real_scale'
            view.auto_scale()


def main() -> int:
    args = [a for a in sys.argv[1:] if a != '--list']
    list_mode = '--list' in sys.argv[1:]
    path = args[0] if args else None

    if list_mode:
        if not path:
            print('Usage: view_ephys.py --list <EDL_DIRECTORY>', file=sys.stderr)
            return 2
        return _run_list_mode(path)

    try:
        from ephyviewer import (
            mkQApp,
            MainViewer,
            TraceViewer,
            InMemoryAnalogSignalSource,
        )
        from ephyviewer.myqt import QT
    except ImportError as e:
        print(
            'Failed to import ephyviewer with a working Qt binding.\n'
            'Install a binding, e.g.:  pip install PySide6\n'
            'Original error: {}'.format(e),
            file=sys.stderr,
        )
        return 1

    # the Qt application must exist before any dialog is constructed
    app = mkQApp()

    if not path:
        path = QT.QFileDialog.getExistingDirectory(None, 'Select an EDL recording directory')
        if not path:
            return 0  # user cancelled

    datasets, rec_len_msec = scan(path)
    if not datasets:
        QT.QMessageBox.information(
            None,
            'No ephys data',
            'No ephys datasets (rhd/zarr/json) were found in:\n{}'.format(path),
        )
        return 1

    # build one trace panel per dataset, stacked in a single window; the
    # navigation toolbar keeps a shared, synchronised time axis across them
    win = MainViewer(show_auto_scale=True)
    views = []
    prev_name = None
    for entry in datasets:
        try:
            loaded = load_signals(entry.dataset, entry.kind, rec_len_msec)
        except Exception as e:  # noqa: BLE001 (skip a bad dataset, keep the rest)
            print('Skipping "{}": {}'.format(entry.label, e), file=sys.stderr)
            continue
        if loaded is None:
            # e.g. a JSON table with no numeric signal columns
            continue
        signals, sample_rate, t_start, channel_names = loaded

        source = InMemoryAnalogSignalSource(
            signals, sample_rate, t_start, channel_names=channel_names
        )
        view = TraceViewer(source=source, name=entry.label)
        # 'same_for_all' stacks channels by their median-absolute-deviation, but
        # that is zero for pulse-like data (TTL/sync on an ADC: mostly 0 with
        # occasional pulses); use absolute min/max scaling for those instead
        med = np.median(signals, axis=0)
        mad = np.median(np.abs(signals - med), axis=0)
        view.params['scale_mode'] = 'same_for_all' if np.nanmax(mad) > 0 else 'real_scale'
        _apply_progressive_colors(view)

        if prev_name is None:
            win.add_view(view)
        else:
            # stack each subsequent panel below the previous one
            win.add_view(view, split_with=prev_name, orientation='vertical')
        prev_name = entry.label
        views.append(view)

    if not views:
        QT.QMessageBox.information(
            None,
            'No signals',
            'None of the datasets in:\n{}\ncontained numeric signal traces.'.format(path),
        )
        return 1

    # auto-scale only after every panel is in place, so the shared time range is
    # set and each view's initial window actually overlaps its data
    for view in views:
        _auto_scale(view)

    win.setWindowTitle('edlio ephys viewer — {}'.format(os.path.basename(path.rstrip('/'))))
    win.show()
    return app.exec()


if __name__ == '__main__':
    sys.exit(main())
