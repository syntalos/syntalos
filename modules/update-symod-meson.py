#!/usr/bin/env python3
#
# Copyright (C) 2020-2024 Matthias Klumpp <mak@debian.org>
#
# SPDX-License-Identifier: LGPL-3.0+
#
# Update common Meson build definitions for modules, since
# Meson lacks the required macro functionality.
#

import os
import sys
from pathlib import Path
from glob import glob


# these modules should have their build definitions
# updated manually and shouldn't use the updater script
MANUAL_MODULES = set(
    [
        'intan-rhx',
        'camera-flir',
    ]
)


def update_meson_build_file(meson_fname, new_symod_setup_code):
    mod_name = Path(meson_fname).joinpath('..').resolve().name

    if mod_name in MANUAL_MODULES:
        print(f'Ignored: {mod_name}')
        return

    with open(meson_fname, 'r', encoding='utf-8') as f:
        content = f.readlines()

    # find sentinel comment block
    sentinel = ["#\n", "# Generic module setup\n", "#\n"]
    index = -1
    for i in range(len(content) - len(sentinel) + 1):
        if content[i : i + len(sentinel)] == sentinel:
            index = i + len(sentinel)
            break

    if index == -1:
        return

    # replace everything after the found index with the new code
    content = content[:index] + [new_symod_setup_code]

    # save result
    with open(meson_fname, 'w', encoding='utf-8') as f:
        f.writelines(content)

    print(f'Updated: {mod_name}')


def run(current_dir, args):
    # collect Meson build definitions
    meson_mod_def_fnames = []
    for mod_dir in glob(current_dir + '/*', recursive=False):
        meson_fname = os.path.join(mod_dir, 'meson.build')
        if os.path.isfile(meson_fname):
            meson_mod_def_fnames.append(meson_fname)

    meson_symod_setup_code = Path(os.path.join(current_dir, 'modsetup.meson')).read_text()
    for meson_fname in meson_mod_def_fnames:
        update_meson_build_file(meson_fname, meson_symod_setup_code)

    return 0


if __name__ == '__main__':
    thisfile = __file__
    if not os.path.isabs(thisfile):
        thisfile = os.path.normpath(os.path.join(os.getcwd(), thisfile))
    thisdir = os.path.normpath(os.path.join(os.path.dirname(thisfile)))
    os.chdir(thisdir)

    sys.exit(run(thisdir, sys.argv[1:]))
