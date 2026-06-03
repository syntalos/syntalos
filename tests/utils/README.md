# EDL signals viewer

`view-signals.py` is a small example program that loads an EDL recording and
displays all signal data in can find in the dataset as scrolling traces using
[ephyviewer](https://ephyviewer.readthedocs.io/).

Panels are auto-scaled with ephyviewer's stacked per-channel mode; pulse-like
channels (e.g. TTL/sync recorded on an ADC, which have zero median deviation)
fall back to absolute min/max scaling so they stay visible.

## Running

```bash
# directory dialog
python view-signals.py

# or point it straight at a recording
python view-signals.py /path/to/edl/recording

# list the suitable datasets found, without opening a window (no display needed)
python view-signals.py --list /path/to/edl/recording
```

## Dependencies

- `edlio`
- `ephyviewer`
- **A Qt binding** - ephyviewer cannot open a window without one. This example
  targets PySide6:

  ```bash
  pip install PySide6
  ```

## A note on sample rate and timestamps

ephyviewer plots traces on a single, constant sample rate per view - it has no
source that accepts an explicit per-sample timestamp. For zarr datasets the
example resolves the rate and the sample positions as follows:

- **Sample rate.** If the `data` (or `timestamps`) array has a `sample_rate`
  attribute it is used directly. Otherwise, if the timestamps carry a physical
  `time_unit` (microseconds, milliseconds, …) the rate is derived from their
  total span (`(N - 1) / (t_last - t_first)`) — not the per-sample spacing,
  since Syntalos timestamps are not strictly equidistant (they can arrive in
  near-duplicate pairs), which would make a median-spacing estimate wildly
  wrong. If neither is available (index timestamps with no rate) the rate is
  recovered from the collection's `recording_length_msec` (`samples / duration`),
  falling back to 30 kHz with a warning.
- **Index timestamps.** When `time_unit` is `"index"` (sample counters, as
  written by Syntalos' Zarr writer) the values give each row's true position on
  the sample grid. The example honours the **start offset** (`t_start =
  first_index / sample_rate`) and, because Syntalos' time-sync may leave
  **non-equidistant** indices, re-grids the rows onto the full integer-index grid
  - forward-filling any gaps - so that a constant sample rate maps each row to
  its correct time. Very large gaps are left uncorrected (with a warning) to
  avoid exploding memory. For the common gap-free recording this only sets
  `t_start`.

The same logic is applied to the displayed amplitudes: a zarr dataset's
`data_scale` / `data_offset` attributes are applied so traces show physical
units (e.g. µV) rather than raw ADC counts.
