#!/usr/bin/env python3
"""Render CSV samples to:
   - inline Mermaid xychart-beta blocks on stdout (for $GITHUB_STEP_SUMMARY)
   - a full-resolution matplotlib PNG written to disk (for artifact upload).

Per-metric colors and phase markers (drawn as a narrow vertical bar overlay)
are kept consistent between the Mermaid and matplotlib outputs.
"""

import csv
import subprocess
import sys
from pathlib import Path

import matplotlib
import psutil

matplotlib.use('Agg')
import matplotlib.pyplot as plt

MAX_POINTS = 3000  # safety cap; ~1500-sample runs render full-detail
MARKER_COLOR = '#ff4040'

# (label, mermaid color, matplotlib color)
COLORS = {
    'cpu': '#ff8800',
    'mem': '#2ca02c',
    'dread': '#1f77b4',
    'dwrite': '#9467bd',
}

if len(sys.argv) < 2:
    print('usage: ci-resource-plot.py <csv> [markers] [label] [png_out]', file=sys.stderr)
    sys.exit(2)

csv_path = Path(sys.argv[1])
markers_path = Path(sys.argv[2]) if len(sys.argv) > 2 else None
job_label = sys.argv[3] if len(sys.argv) > 3 else 'CI job'
png_out = Path(sys.argv[4]) if len(sys.argv) > 4 else Path('/tmp/ci-resources.png')

rows = list(csv.DictReader(csv_path.open()))
if not rows:
    print('_(no samples collected)_')
    sys.exit(0)

t0 = float(rows[0]['ts_epoch'])
t_min = [(float(r['ts_epoch']) - t0) / 60 for r in rows]
tmax = t_min[-1] if t_min else 0.0

cpu = [float(r['cpu_pct']) for r in rows]
mem_gb = [float(r['mem_used_mb']) / 1024 for r in rows]
dr = [float(r['disk_read_mb_s']) for r in rows]
dw = [float(r['disk_write_mb_s']) for r in rows]

all_markers = []
if markers_path and markers_path.is_file():
    for line in markers_path.read_text().splitlines():
        if ',' not in line:
            continue
        label, epoch = line.split(',', 1)
        try:
            all_markers.append((label.strip(), (float(epoch) - t0) / 60))
        except ValueError:
            pass

# Inner markers = exclude the implicit start/end (build-start at ~0, end at ~tmax)
inner_markers = [
    (lbl, x)
    for lbl, x in all_markers
    if 0.05 < x < (tmax - 0.05) and lbl not in ('build-start', 'end')
]

# --- Full-resolution PNG (artifact) ---
fig, axes = plt.subplots(4, 1, figsize=(11, 11), sharex=True)
axes[0].plot(t_min, cpu, color=COLORS['cpu'])
axes[0].set_ylabel('CPU %')
axes[0].set_ylim(0, 100)
axes[1].plot(t_min, mem_gb, color=COLORS['mem'])
axes[1].set_ylabel('Mem used (GB)')
axes[2].plot(t_min, dr, color=COLORS['dread'])
axes[2].set_ylabel('Disk read (MB/s)')
axes[3].plot(t_min, dw, color=COLORS['dwrite'])
axes[3].set_ylabel('Disk write (MB/s)')
axes[-1].set_xlabel('Time since sampler start (min)')
fig.suptitle(f'Resource usage — {job_label}')

for label, x in all_markers:
    for ax in axes:
        ax.axvline(x, color=MARKER_COLOR, linestyle='--', alpha=0.55, linewidth=1)
    axes[0].text(x, 95, ' ' + label, fontsize=8, color=MARKER_COLOR, rotation=90, va='top')

fig.tight_layout()
fig.savefig(png_out, format='png', dpi=110)


# --- Mermaid xychart-beta inline charts ---
def downsample(values):
    if len(values) <= MAX_POINTS:
        return list(values)
    step = len(values) / MAX_POINTS
    out = []
    for i in range(MAX_POINTS):
        lo = int(i * step)
        hi = max(lo + 1, int((i + 1) * step))
        chunk = values[lo:hi]
        out.append(sum(chunk) / len(chunk))
    return out


def fmt(vals, decimals=1):
    return '[' + ', '.join(f'{v:.{decimals}f}' for v in vals) + ']'


def title_with_markers(base):
    """Append inner phase markers to the chart title (Mermaid xychart-beta has
    no native annotation primitive that doesn't render a baseline strip)."""
    if not inner_markers:
        return base
    suffix = '  —  ' + ', '.join(f'{lbl} @ {x:.1f}min' for lbl, x in inner_markers)
    return base + suffix


CHART_WIDTH = 1400
CHART_HEIGHT = 320


def xychart(title, ylabel, ymin, ymax, colors, series_list, decimals=1):
    """colors and series_list are parallel lists; pass [(label, color), ...]
    for the legend hint in the title when more than one series is plotted."""
    palette = ', '.join(c for _, c in colors)
    init = (
        '%%{init: {'
        '"themeVariables": {"xyChart": {"plotColorPalette": "' + palette + '"}}, '
        '"xyChart": {"width": ' + str(CHART_WIDTH) + ', "height": ' + str(CHART_HEIGHT) + '}'
        '}}%%'
    )
    full_title = title_with_markers(title)
    if len(colors) > 1:
        full_title += '  (' + ', '.join(lbl for lbl, _ in colors) + ')'
    block = (
        '```mermaid\n'
        f'{init}\n'
        'xychart-beta\n'
        f'    title "{full_title}"\n'
        f'    x-axis "min" 0 --> {tmax:.1f}\n'
        f'    y-axis "{ylabel}" {ymin} --> {ymax}\n'
    )
    for series in series_list:
        block += f'    line {fmt(downsample(series), decimals)}\n'
    block += '```\n'
    return block


peak_mem = max(mem_gb)
avg_cpu = sum(cpu) / len(cpu)


def integrate(series):
    """MB/s series -> total GB, using per-row Δt from the timestamps."""
    total = 0.0
    for i in range(1, len(series)):
        dt = float(rows[i]['ts_epoch']) - float(rows[i - 1]['ts_epoch'])
        total += series[i] * dt
    return total / 1024


total_r = integrate(dr)
total_w = integrate(dw)

print(f'## Resource usage — {job_label}\n')
print('| Metric | Value |')
print('|---|---|')
print(f'| Wall duration | {tmax:.1f} min |')
print(f'| Peak memory | {peak_mem:.2f} GB |')
print(f'| Average CPU | {avg_cpu:.1f} % |')
print(f'| Total disk read | {total_r:.2f} GB |')
print(f'| Total disk write | {total_w:.2f} GB |')
print()
if all_markers:
    print(
        '**Phase markers** (minutes from sampler start): '
        + ', '.join(f'`{lbl} @ {x:.1f}`' for lbl, x in all_markers)
        + '\n'
    )

print(xychart('CPU %', 'percent', 0, 100, [('cpu', COLORS['cpu'])], [cpu]))
print(
    xychart(
        'Memory used (GB)',
        'GB',
        0,
        max(8, int(peak_mem) + 1),
        [('mem', COLORS['mem'])],
        [mem_gb],
        2,
    )
)
print(
    xychart(
        'Disk read (MB/s)',
        'MB/s',
        0,
        max(1, int(max(dr)) + 1),
        [('read', COLORS['dread'])],
        [dr],
        2,
    )
)
print(
    xychart(
        'Disk write (MB/s)',
        'MB/s',
        0,
        max(1, int(max(dw)) + 1),
        [('write', COLORS['dwrite'])],
        [dw],
        2,
    )
)


def cpu_model():
    try:
        with open('/proc/cpuinfo') as f:
            for line in f:
                if line.startswith('model name'):
                    return line.split(':', 1)[1].strip()
    except OSError:
        pass
    # arm64 /proc/cpuinfo lacks "model name" - fall back to lscpu
    try:
        out = subprocess.check_output(['lscpu'], text=True)
        for line in out.splitlines():
            if line.startswith('Model name:'):
                return line.split(':', 1)[1].strip()
    except (OSError, subprocess.CalledProcessError):
        pass
    return 'unknown'


vm = psutil.virtual_memory()
sw = psutil.swap_memory()
print('### Runner environment\n')
print('| | |')
print('|---|---|')
print(f'| CPU | {cpu_model()} |')
print(
    f'| Cores | {psutil.cpu_count(logical=False) or "?"} physical / '
    f'{psutil.cpu_count(logical=True) or "?"} logical |'
)
print(f'| Total RAM | {vm.total / 1024 / 1024 / 1024:.1f} GB |')
print(f'| Swap | {sw.total / 1024 / 1024 / 1024:.1f} GB |')

print('\n> Full-resolution chart available in the **resource-samples** ' 'artifact for this job.\n')
