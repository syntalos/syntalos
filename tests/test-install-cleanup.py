#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0+
"""Exercise install cleanup without touching the system installation."""

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / 'contrib' / 'cleanup-installed-libs.py'
LIBRARIES = ('libsyntalos-mlink.so', 'libsyntalos-datactl.so')


class InstallCleanupTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='syntalos-install-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.prefix = self.root / 'staging' / 'usr' / 'local'
        self.libdir = self.prefix / 'lib' / 'multiarch'
        self.libdir.mkdir(parents=True)

    def install_version(self, version, soversion='0'):
        for name in LIBRARIES:
            (self.libdir / f'{name}.{version}').write_text(version)
            for link_name, target in (
                (name, f'{name}.{soversion}'),
                (f'{name}.{soversion}', f'{name}.{version}'),
            ):
                link = self.libdir / link_name
                link.unlink(missing_ok=True)
                link.symlink_to(target)

    def cleanup(self, version='3.2.1', soversion='0', libdir='lib/multiarch', **extra_env):
        env = os.environ.copy()
        env.pop('DESTDIR', None)
        env.pop('MESON_INSTALL_QUIET', None)
        env['MESON_INSTALL_DESTDIR_PREFIX'] = str(self.prefix)
        env.update(extra_env)
        return subprocess.run(
            [sys.executable, str(SCRIPT), libdir, version, soversion],
            env=env,
            check=True,
            capture_output=True,
            text=True,
        )

    def assert_version(self, version, soversion='0'):
        for name in LIBRARIES:
            self.assertEqual((self.libdir / name).read_text(), version)
            self.assertEqual(
                (self.libdir / f'{name}.{soversion}').readlink(), Path(f'{name}.{version}')
            )

    def test_upgrade_downgrade_and_reinstall(self):
        for previous, current in (
            ('3.2.1', '3.2.2'),
            ('3.2.2', '3.2.1'),
            ('3.2.1', '3.2.1'),
        ):
            with self.subTest(previous=previous, current=current):
                self.install_version(previous)
                self.install_version(current)
                self.cleanup(current)
                self.cleanup(current)
                self.assert_version(current)
                for name in LIBRARIES:
                    self.assertEqual(
                        sorted(p.name for p in self.libdir.glob(f'{name}*')),
                        sorted([name, f'{name}.0', f'{name}.{current}']),
                    )

    def test_preserves_unrelated_files_and_symlink_targets(self):
        self.install_version('3.2.1')
        outside = self.root / 'outside.so'
        outside.write_text('external library')
        for name in LIBRARIES:
            (self.libdir / f'{name}.3.2.2').symlink_to(outside)
            (self.libdir / f'{name}.2.1.2').symlink_to('missing-library')
            (self.libdir / f'{name}.3.2.1.backup').write_text('backup')
            (self.libdir / f'{name}.9.0.0').mkdir()
        (self.libdir / 'libother.so.3.2.2').write_text('unrelated')
        self.cleanup()
        self.assert_version('3.2.1')
        self.assertEqual(outside.read_text(), 'external library')
        self.assertEqual((self.libdir / 'libother.so.3.2.2').read_text(), 'unrelated')
        for name in LIBRARIES:
            self.assertFalse((self.libdir / f'{name}.3.2.2').is_symlink())
            self.assertFalse((self.libdir / f'{name}.2.1.2').is_symlink())
            self.assertEqual((self.libdir / f'{name}.3.2.1.backup').read_text(), 'backup')
            self.assertTrue((self.libdir / f'{name}.9.0.0').is_dir())

    def test_partial_install_preserves_missing_family(self):
        self.install_version('3.2.2')
        (self.libdir / 'libsyntalos-mlink.so.3.2.1').write_text('3.2.1')
        self.cleanup()
        self.assertFalse((self.libdir / 'libsyntalos-mlink.so.3.2.2').exists())
        self.assertEqual((self.libdir / 'libsyntalos-datactl.so.3.2.2').read_text(), '3.2.2')

    def test_missing_current_library_preserves_installation(self):
        self.install_version('3.2.2')
        self.cleanup()
        self.assert_version('3.2.2')
        self.cleanup(libdir='does-not-exist')

    def test_removes_previous_soname_and_prerelease(self):
        self.install_version('3.2.2', soversion='1')
        self.install_version('3.2.1-rc1')
        self.install_version('3.2.1')
        self.cleanup()
        self.assert_version('3.2.1')
        for name in LIBRARIES:
            self.assertFalse((self.libdir / f'{name}.1').is_symlink())
            self.assertFalse((self.libdir / f'{name}.3.2.1-rc1').exists())

    def test_absolute_libdir_honors_destdir(self):
        self.install_version('3.2.2')
        self.install_version('3.2.1')
        # A different configured prefix must not change an absolute libdir.
        self.prefix = self.root / 'staging' / 'opt' / 'syntalos'
        self.cleanup(libdir='/usr/local/lib/multiarch', DESTDIR=str(self.root / 'staging'))
        self.assert_version('3.2.1')
        for name in LIBRARIES:
            self.assertFalse((self.libdir / f'{name}.3.2.2').exists())

    def test_unstaged_prefix_and_quiet_install(self):
        self.install_version('3.2.2')
        self.install_version('3.2.1')
        result = self.cleanup(libdir=str(self.libdir), MESON_INSTALL_QUIET='1')
        self.assertEqual(result.stdout, '')
        self.assert_version('3.2.1')
        for name in LIBRARIES:
            self.assertFalse((self.libdir / f'{name}.3.2.2').exists())


if __name__ == '__main__':
    unittest.main()
