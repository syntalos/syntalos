#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0+
"""Remove other Syntalos library versions after a Meson installation."""

import argparse
import os
import re
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('libdir')
    parser.add_argument('version')
    parser.add_argument('soversion')
    args = parser.parse_args()

    libdir = Path(args.libdir)
    if libdir.is_absolute():
        # Absolute libdir options are outside the prefix, but still inside DESTDIR.
        libdir = Path(os.environ.get('DESTDIR') or '/') / libdir.relative_to(libdir.anchor)
    else:
        libdir = Path(os.environ['MESON_INSTALL_DESTDIR_PREFIX']) / libdir

    for name in ('libsyntalos-mlink.so', 'libsyntalos-datactl.so'):
        current = libdir / f'{name}.{args.version}'
        if not current.is_file() or current.is_symlink():
            # A partial install must not delete the only available library.
            continue

        keep = {current.name, f'{name}.{args.soversion}'}
        versioned_name = re.compile(rf'{re.escape(name)}\.[0-9]+(?:\.[0-9]+)*(?:[-+][\w.+-]+)?')
        for entry in sorted(libdir.iterdir()):
            if entry.name in keep or not versioned_name.fullmatch(entry.name):
                continue
            if not entry.is_symlink() and not entry.is_file():
                continue
            if not os.environ.get('MESON_INSTALL_QUIET'):
                print(f'Removing obsolete Syntalos library: {entry}')
            entry.unlink()


if __name__ == '__main__':
    main()
