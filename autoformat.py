#!/usr/bin/env python3
#
# Copyright (C) 2015-2022 Matthias Klumpp <mak@debian.org>
#
# SPDX-License-Identifier: LGPL-3.0+
#
# Format all Syntalos source code in-place.
#

import os
import sys
import shutil
import fnmatch
import subprocess
from glob import glob

INCLUDE_LOCATIONS = ['autoformat.py', 'contrib', 'docs', 'modules', 'src', 'tests', 'tools']

EXCLUDE_MATCH = [
    '*/contrib/firmware/*',
    '*/venv/*',
    '*/contrib/vendor/*',
    '*/modules/intan-rhx/*',  # we want to keep Intan RHX close to upstream, for easy merging of changes
    '*/modules/camera-arv/qarv/*',
    '*/tests/rwqueue/*',
]


def format_cpp_sources(sources):
    """Format C/C++ sources with clang-format."""

    command = ['clang-format', '-i']
    command.extend(sources)
    subprocess.run(command, check=True)


def format_python_sources(sources):
    """Format Python sources with Black."""

    command = [
        'black',
        '-S',  # no string normalization
        '-l',
        '100',  # line length
        '-t',
        'py311',  # minimum Python target
    ]
    command.extend(sources)
    subprocess.run(command, check=True)


def run(current_dir, args):
    # check for tools
    if not shutil.which('clang-format'):
        print(
            'The `clang-format` formatter is not installed. Please install it to continue!',
            file=sys.stderr,
        )
        return 1
    if not shutil.which('black'):
        print(
            'The `black` formatter is not installed. Please install it to continue!',
            file=sys.stderr,
        )
        return 1

    # if no include directories are explicitly specified, we read all locations
    if not INCLUDE_LOCATIONS:
        INCLUDE_LOCATIONS.append('.')

    # collect sources
    cpp_sources = []
    py_sources = []
    for il_path_base in INCLUDE_LOCATIONS:
        il_path = os.path.join(current_dir, il_path_base)
        if os.path.isfile(il_path):
            candidates = [il_path]
        else:
            candidates = glob(il_path + '/**/*', recursive=True)

        for filename in candidates:
            skip = False
            for exclude in EXCLUDE_MATCH:
                if fnmatch.fnmatch(filename, exclude):
                    skip = True
                    break
            if skip:
                continue
            if filename.endswith(('.c', '.cpp', '.h', '.hpp')):
                cpp_sources.append(filename)
            elif filename.endswith('.py'):
                py_sources.append(filename)

    # format
    format_python_sources(py_sources)
    format_cpp_sources(cpp_sources)

    return 0


if __name__ == '__main__':
    thisfile = __file__
    if not os.path.isabs(thisfile):
        thisfile = os.path.normpath(os.path.join(os.getcwd(), thisfile))
    thisdir = os.path.normpath(os.path.join(os.path.dirname(thisfile)))
    os.chdir(thisdir)

    sys.exit(run(thisdir, sys.argv[1:]))
