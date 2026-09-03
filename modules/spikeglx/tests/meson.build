# Unit tests for Syntalos' SpikeGLX module

test_sglxutils_moc = qt.compile_moc(
    sources: ['test-spikeglx-utils.cpp'],
    dependencies: [qt_test_dep],
)

test_sglxutils_exe = executable('test-spikeglx-utils',
    ['test-spikeglx-utils.cpp',
        '../sglxutils.cpp',
        test_sglxutils_moc],
    include_directories: include_directories('..'),
    dependencies: [syntalos_fabric_dep, qt_test_dep]
)

test('sy-test-spikeglx-utils',
    test_sglxutils_exe,
)
