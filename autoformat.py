#!/usr/bin/env python3
#
# Copyright (C) 2015-2022 Matthias Klumpp <mak@debian.org>
#
# SPDX-License-Identifier: LGPL-3.0+
#
# Format all Syntalos source code in-place.
#

import os
import re
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
    '*/contrib/subprojects/*',
    '*/modules/intan-rhx/*',  # we want to keep Intan RHX close to upstream, for easy merging of changes
    '*/modules/camera-arv/qarv/*',
    '*/modules/open-ephys-acq/devices/oni/rhythm-api/*',
    '*/tests/rwqueue/*',
]

MIN_CLANG_FORMAT_VERSION = 22
CLANG_FORMAT_FALLBACKS = ['clang-format-24', 'clang-format-23', 'clang-format-22']


def get_clang_format_version(executable):
    """Return the major version of the given clang-format executable, or None on failure."""

    try:
        result = subprocess.run(
            [executable, '--version'], capture_output=True, text=True, check=True
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None
    match = re.search(r'version\s+(\d+)', result.stdout)
    if not match:
        return None
    return int(match.group(1))


def find_clang_format():
    """Find a suitable clang-format executable."""

    candidates = []
    if shutil.which('clang-format'):
        candidates.append('clang-format')
    for name in CLANG_FORMAT_FALLBACKS:
        if shutil.which(name):
            candidates.append(name)

    for exe in candidates:
        version = get_clang_format_version(exe)
        if version is not None and version >= MIN_CLANG_FORMAT_VERSION:
            return exe, version

    return None, None


def format_cpp_sources(executable, sources):
    """Format C/C++ sources with clang-format."""

    command = [executable, '-i']
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
    clang_format_exe, clang_format_version = find_clang_format()
    if not clang_format_exe:
        if shutil.which('clang-format'):
            current_version = get_clang_format_version('clang-format')
            print(
                'Found `clang-format` version {}, but version {} or newer is required. '
                'Please install clang-format >= {}.'.format(
                    current_version, MIN_CLANG_FORMAT_VERSION, MIN_CLANG_FORMAT_VERSION
                ),
                file=sys.stderr,
            )
        else:
            print(
                'The `clang-format` formatter (version {} or newer) is not installed. '
                'Please install it to continue!'.format(MIN_CLANG_FORMAT_VERSION),
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
    format_cpp_sources(clang_format_exe, cpp_sources)

    return 0


if __name__ == '__main__':
    thisfile = __file__
    if not os.path.isabs(thisfile):
        thisfile = os.path.normpath(os.path.join(os.getcwd(), thisfile))
    thisdir = os.path.normpath(os.path.join(os.path.dirname(thisfile)))
    os.chdir(thisdir)

    sys.exit(run(thisdir, sys.argv[1:]))
