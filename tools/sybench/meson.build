# Build definition for the Syntalos Benchmark Tool

sybench_core_hdr = [
    'dimension.h',
    'dimensions-common.h',
    'health.h',
    'ladder.h',
    'projectgen.h',
    'runner.h',
]

sybench_core_src = [
    'dimension.cpp',
    'dimensions-camera.cpp',
    'dimensions-common.cpp',
    'dimensions-encoding.cpp',
    'dimensions-oop.cpp',
    'dimensions-signal.cpp',
    'health.cpp',
    'ladder.cpp',
    'projectgen.cpp',
    'runner.cpp',
    'score.cpp',
]

sybench_core_lib = static_library('sybench-core',
    [sybench_core_hdr, sybench_core_src],
    dependencies: [
        syntalos_fabric_dep,
        kfarchive_dep,
        toml_dep,
    ],
    include_directories: [include_directories('../../src')],
)

sybench_core_dep = declare_dependency(
    link_with: sybench_core_lib,
    dependencies: [
        syntalos_fabric_dep,
        kfarchive_dep,
        toml_dep,
    ],
    include_directories: include_directories('.'),
)

sybench_moc_hdr = [
    'benchsession.h',
    'benchwindow.h',
]
sybench_src = [
    'main.cpp',
    'benchsession.cpp',
    'benchwindow.cpp',
    'report.cpp',
    '../../src/appstyle.cpp',
]
sybench_ui = ['benchwindow.ui']

sybench_moc = qt.compile_moc(
    headers: sybench_moc_hdr,
    dependencies: [qt_core_dep, qt_gui_dep],
)
sybench_moc += qt.compile_ui(sources: sybench_ui)

syntalos_bench_exe = executable('syntalos-benchmark',
    [sybench_moc_hdr, sybench_src, sybench_moc],
    dependencies: [
        sybench_core_dep,
        qt_gui_dep,
        kfconfigwidgets_dep,
    ],
    include_directories: [include_directories('../../src')],
    install: true,
    install_rpath: sy_libdir
)

#
# Unit tests
#
test_sybench_moc_src = ['test-sybench.cpp']
test_sybench_moc = qt.compile_moc(sources: test_sybench_moc_src)
test_sybench_exe = executable('test-sybench',
    [test_sybench_moc_src, test_sybench_moc],
    dependencies: [sybench_core_dep,
                   qt_test_dep]
)
test('sy-test-sybench',
     test_sybench_exe,
     env: test_env,
     is_parallel: true,
)
