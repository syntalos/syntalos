# Build definition for the Syntalos System Benchmark

sybench_core_hdr = [
    'dimension.h',
    'ladder.h',
    'projectgen.h',
    'runner.h',
]

sybench_core_src = [
    'dimension.cpp',
    'dimensions-camera.cpp',
    'ladder.cpp',
    'projectgen.cpp',
    'runner.cpp',
]

sybench_core_lib = static_library('sybench-core',
    [sybench_core_hdr, sybench_core_src],
    dependencies: [
        syntalos_fabric_dep,
        kfarchive_dep,
        toml_dep,
    ],
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

syntalos_bench_exe = executable('syntalos-benchmark',
    ['main.cpp'],
    dependencies: [sybench_core_dep],
    install: true,
    install_rpath: sy_libdir
)

#
# Unit test for the project generator and ladder logic
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
