#!/usr/bin/env python3
#
# Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0+

"""Delete old Debian package versions from a Cloudsmith repository.

Cloudsmith's own retention rules can only group packages by name, format and type,
so a new upload for one architecture or distribution suite may delete the packages
of another one. This script groups packages by everything that identifies them
except for their version (name, suite, component, architecture and package type)
and only keeps the newest versions of each group.

Without --delete, it only shows what would be deleted.
"""

import os
import sys
import json
import argparse
import functools
import subprocess
from collections import defaultdict

try:
    import apt_pkg

    apt_pkg.init_system()
except ImportError:
    apt_pkg = None


@functools.cache
def version_compare(a: str, b: str) -> int:
    """Compare two Debian versions, returning <0, 0 or >0."""
    if apt_pkg:
        return apt_pkg.version_compare(a, b)

    # python3-apt is not always available, ask dpkg instead
    for op, result in (('lt', -1), ('gt', 1)):
        proc = subprocess.run(['dpkg', '--compare-versions', a, op, b])
        if proc.returncode == 0:
            return result
        if proc.returncode != 1:
            raise RuntimeError(f'Unable to compare versions "{a}" and "{b}" with dpkg.')
    return 0


def deb_version(pkg: dict) -> str:
    """Return the full Debian version of a package, including its epoch."""
    version = pkg['version']
    if pkg.get('epoch') and ':' not in version:
        version = f'{pkg["epoch"]}:{version}'
    return version


def group_label(pkg: dict) -> str:
    ids = pkg['identifiers']
    label = (
        f'{ids.get("name")} ({ids.get("distro_version")}/{ids.get("deb_component")}, '
        f'{ids.get("architecture")}'
    )
    # all regular binary packages have type 1, only mention it if it is something else
    if ids.get('package_type', '1') != '1':
        label += f', type {ids["package_type"]}'
    return label + ')'


def warn(message: str):
    if os.environ.get('GITHUB_ACTIONS') == 'true':
        print(f'::warning::{message}')
    else:
        print(f'W: {message}')


def list_packages(repo: str) -> list[dict]:
    proc = subprocess.run(
        ['cloudsmith', 'ls', 'pkgs', repo, '-F', 'json', '--page-all'],
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    return json.loads(proc.stdout)['data']


def find_stale_arch_all(newest: dict[tuple, dict]) -> list[tuple]:
    """Find groups superseded by a build for a different kind of architecture.

    When a package changes from arch "all" to arch-specific builds (or the other way around),
    its old group never receives a new version and thus is never pruned.
    """
    families = defaultdict(list)
    for key, pkg in newest.items():
        families[tuple(item for item in key if item[0] != 'architecture')].append(key)

    stale = []
    for keys in families.values():
        for key in keys:
            is_all = newest[key]['identifiers']['architecture'] == 'all'
            for other in keys:
                if (newest[other]['identifiers']['architecture'] == 'all') == is_all:
                    continue
                if version_compare(deb_version(newest[key]), deb_version(newest[other])) < 0:
                    stale.append((key, newest[other]))
                    break
    return stale


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Delete all but the newest versions of each Debian package in a Cloudsmith '
        'repository, counted separately per name, suite, component, architecture and type.'
    )
    parser.add_argument('repo', metavar='OWNER/REPO', help='Cloudsmith repository to prune')
    parser.add_argument(
        '--keep',
        type=int,
        default=3,
        help='Number of newest versions to keep for each package (default: %(default)s)',
    )
    parser.add_argument(
        '--delete',
        action='store_true',
        help='Actually delete packages, instead of only showing what would be deleted',
    )
    args = parser.parse_args()
    if args.keep < 1:
        parser.error('--keep must be at least 1')

    packages = list_packages(args.repo)

    # group packages by everything that identifies them, except for their version
    groups = defaultdict(list)
    n_foreign = 0
    for pkg in packages:
        if pkg['format'] != 'deb':
            n_foreign += 1
            continue
        if pkg['is_quarantined'] or not pkg['is_sync_completed']:
            # never count or delete packages that are being processed, failed or quarantined
            state = 'quarantined' if pkg['is_quarantined'] else pkg['stage_str']
            print(f'Ignoring {pkg["filename"]}: {state}')
            continue
        ids = pkg['identifiers']
        groups[tuple(sorted((k, v) for k, v in ids.items() if k != 'version'))].append(pkg)
    if n_foreign:
        print(f'Ignoring {n_foreign} non-Debian package(s)')

    newest_first = functools.cmp_to_key(
        lambda a, b: version_compare(deb_version(b), deb_version(a))
    )
    newest = {}
    to_delete = []
    for key in sorted(groups, key=lambda k: group_label(groups[k][0])):
        pkgs = sorted(groups[key], key=newest_first)
        newest[key] = pkgs[0]

        # keep all packages of the newest N distinct versions
        n_versions = 0
        prev_version = None
        drop = []
        for pkg in pkgs:
            version = deb_version(pkg)
            if prev_version is None or version_compare(version, prev_version) != 0:
                n_versions += 1
                prev_version = version
            if n_versions > args.keep:
                drop.append(pkg)
        if not drop:
            continue

        print(f'\n{group_label(pkgs[0])}:')
        for pkg in pkgs:
            action = 'delete' if pkg in drop else 'keep'
            print(f'  {action:<6}  {deb_version(pkg):<24}  (uploaded {pkg["uploaded_at"][:10]})')
        to_delete.extend(drop)

    for key, superseding in find_stale_arch_all(newest):
        versions = ', '.join(
            f'{deb_version(pkg)} [{pkg["slug_perm"]}]'
            for pkg in sorted(groups[key], key=newest_first)
        )
        warn(
            f'{group_label(newest[key])} is older than the '
            f'{superseding["identifiers"]["architecture"]} build {deb_version(superseding)} '
            f'and will never be pruned automatically. Consider deleting it manually: {versions}'
        )

    print()
    if not to_delete:
        print(f'Nothing to delete, all {len(packages)} package(s) are kept.')
        return 0
    if not args.delete:
        print(
            f'Would delete {len(to_delete)} of {len(packages)} package(s). '
            'Run with --delete to remove them.'
        )
        return 0

    n_failed = 0
    for pkg in to_delete:
        proc = subprocess.run(['cloudsmith', 'delete', '-y', f'{args.repo}/{pkg["slug_perm"]}'])
        if proc.returncode != 0:
            n_failed += 1
    if n_failed:
        print(f'Failed to delete {n_failed} of {len(to_delete)} package(s)!', file=sys.stderr)
        return 1
    print(f'Deleted {len(to_delete)} package(s).')
    return 0


if __name__ == '__main__':
    # keep our output in order with the output of the cloudsmith CLI in CI logs
    sys.stdout.reconfigure(line_buffering=True)
    sys.exit(main())
