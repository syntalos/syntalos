#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0+
"""
build-iox2.py – fetch, configure, build and install iceoryx2.

We use this in the Meson build script as fallback to make
iox2 available in case it is missing.

Usage (called by meson.build):
    python3 build-iox2.py --source-dir DIR --build-dir DIR
                          --prefix DIR --version VER --cmake PATH
"""

import argparse
import os
import shutil
import subprocess
import sys


def run(cmd: list, description: str) -> None:
    """Run *cmd*, printing *description* first; abort on non-zero exit."""
    print(description, flush=True)
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f'FAILED: {description}', file=sys.stderr, flush=True)
        sys.exit(result.returncode)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-dir', required=True, help='iceoryx2 source directory')
    parser.add_argument('--build-dir',  required=True, help='CMake build directory')
    parser.add_argument('--prefix',     required=True, help='CMake install prefix')
    parser.add_argument('--version',    required=True, help='iceoryx2 version tag to clone')
    parser.add_argument('--cmake',      required=True, help='Path to the cmake executable')
    parser.add_argument('--cmake-build-type', default='RelWithDebInfo',
                        help='CMake build type to configure for iceoryx2')
    args = parser.parse_args()

    if not os.path.isdir(args.source_dir):
        git = shutil.which('git')
        if not git:
            print('ERROR: git is required to fetch iceoryx2 but was not found on PATH.',
                  file=sys.stderr)
            sys.exit(1)

        run(
            [git, 'clone', '--depth=1', '-b', f'v{args.version}',
             'https://github.com/eclipse-iceoryx/iceoryx2.git',
             args.source_dir],
            f'Fetching iceoryx2 v{args.version} source via git...',
        )

    stamp = os.path.join(args.prefix, 'lib', 'cmake', 'iceoryx2-cxx')
    build_stamp = os.path.join(args.prefix, '.syntalos-build-iox2')
    build_stamp_data = (
        'stamp-version=1\n'
        f'cmake-build-type={args.cmake_build_type}\n'
        f'iceoryx2-version={args.version}\n'
    )
    if os.path.isdir(stamp) and os.path.isfile(build_stamp):
        with open(build_stamp, encoding='utf-8') as fh:
            if fh.read() == build_stamp_data:
                print('Using previously built iceoryx2')
                return

    if os.path.isdir(stamp):
        print('Removing stale iceoryx2 build artifacts')
        shutil.rmtree(args.build_dir, ignore_errors=True)
        shutil.rmtree(args.prefix, ignore_errors=True)

    run(
        [args.cmake,
         '-S', args.source_dir,
         '-B', args.build_dir,
         '-G', 'Ninja',
         f'-DCMAKE_BUILD_TYPE={args.cmake_build_type}',
         '-DBUILD_CXX=ON',
         '-DBUILD_TESTING=OFF',
         '-DBUILD_EXAMPLES=OFF',
         '-DIOX2_FEATURE_LIBC_PLATFORM=ON',
         f'-DCMAKE_INSTALL_PREFIX={args.prefix}'],
        'Configuring iceoryx2...',
    )

    run([args.cmake, '--build', args.build_dir], 'Compiling iceoryx2...')

    run([args.cmake, '--install', args.build_dir], 'Installing iceoryx2...')

    os.makedirs(args.prefix, exist_ok=True)
    with open(build_stamp, 'w', encoding='utf-8') as fh:
        fh.write(build_stamp_data)


if __name__ == '__main__':
    main()
