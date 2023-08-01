#!/bin/env python3
#
# Format all Syntalos source code in-place.
#

import os
import sys
import shutil
import fnmatch
import subprocess
from glob import glob

EXCLUDE_MATCH = ['*/contrib/*', '*/build/*', '*/venv/*', '*/modules/intan-rhx/*']


def format_cpp_sources(sources):
    pass


def format_python_sources(sources):
    """Format all Python code."""
    command = ['black', '-S', '-l', '100', '-t', 'py311']
    command.extend(sources)
    subprocess.run(command, check=True)


def run(current_dir, args):
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

    cpp_sources = []
    py_sources = []

    for filename in glob(current_dir + '/**/*', recursive=True):
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

    format_python_sources(py_sources)

    return 0


if __name__ == '__main__':
    thisfile = __file__
    if not os.path.isabs(thisfile):
        thisfile = os.path.normpath(os.path.join(os.getcwd(), thisfile))
    thisdir = os.path.normpath(os.path.join(os.path.dirname(thisfile)))
    os.chdir(thisdir)

    sys.exit(run(thisdir, sys.argv[1:]))
