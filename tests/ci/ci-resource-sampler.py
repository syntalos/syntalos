#!/usr/bin/env python3
"""Background sampler: write host /proc-derived metrics to CSV until SIGTERM."""

import csv
import signal
import sys
import time

import psutil

INTERVAL = 1.0

if len(sys.argv) < 2:
    print('usage: ci-resource-sampler.py <out.csv>', file=sys.stderr)
    sys.exit(2)

out_path = sys.argv[1]
running = True


def stop(*_):
    global running
    running = False


signal.signal(signal.SIGTERM, stop)
signal.signal(signal.SIGINT, stop)

with open(out_path, 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(
        [
            'ts_epoch',
            'cpu_pct',
            'mem_used_mb',
            'disk_read_mb_s',
            'disk_write_mb_s',
        ]
    )
    psutil.cpu_percent(None)
    prev_d = psutil.disk_io_counters()
    prev_t = time.time()
    while running:
        time.sleep(INTERVAL)
        now = time.time()
        dt = now - prev_t or INTERVAL
        d = psutil.disk_io_counters()
        m = psutil.virtual_memory()
        w.writerow(
            [
                f'{now:.2f}',
                f'{psutil.cpu_percent(None):.1f}',
                f'{(m.total - m.available) / 1024 / 1024:.1f}',
                f'{(d.read_bytes - prev_d.read_bytes) / dt / 1024 / 1024:.2f}',
                f'{(d.write_bytes - prev_d.write_bytes) / dt / 1024 / 1024:.2f}',
            ]
        )
        f.flush()
        prev_d, prev_t = d, now
