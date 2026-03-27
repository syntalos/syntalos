
subdir('rwqueue')

test_env = environment()
test_env.set('MIMALLOC_SHOW_ERRORS', '1')
test_env.set('QT_QPA_PLATFORM', 'offscreen')

#
# Core Utilities Test
#
test_basic_moc_src = ['test-basic.cpp']
test_basic_moc = qt.compile_moc(sources: test_basic_moc_src)
test_basic_exe = executable('test-basic',
                            [test_basic_moc_src, test_basic_moc],
                            dependencies: [syntalos_fabric_dep,
                                           qt_test_dep]
)
test('sy-test-basic',
     test_basic_exe,
     env: test_env,
     is_parallel: true,
)

#
# Basic Stream Performance Test
#
test_streamperf_moc_src = ['test-streamperf.cpp']
test_streamperf_moc = qt.compile_moc(sources: test_streamperf_moc_src)
test_streamperf_exe = executable('test-streamperf',
    [test_streamperf_moc_src, test_streamperf_moc,
     'testbarrier.h'],
    dependencies: [
        syntalos_fabric_dep,
        qt_test_dep,
        opencv_dep
    ]
)
test('sy-test-streamperf',
    test_streamperf_exe,
    env: test_env,
    timeout: 120,
    is_parallel: false,
)


#
# Basic Timer/HRClock Test
#
test_timer_moc_src = ['test-timer.cpp']
test_timer_moc = qt.compile_moc(sources: test_timer_moc_src)
test_timer_exe = executable('test-timer',
    [test_timer_moc_src, test_timer_moc],
    dependencies: [syntalos_fabric_dep,
                   qt_test_dep]
)
test('sy-test-timer',
    test_timer_exe,
    env: test_env,
    is_parallel: false,
)

#
# Experiment Directory Layout Test
#
test_edlstorage_moc_src = ['test-edlstorage.cpp']
test_edlstorage_moc = qt.compile_moc(sources: test_edlstorage_moc_src)
test_edlstorage_exe = executable('test-edlstorage',
    [test_edlstorage_moc_src, test_edlstorage_moc],
    dependencies: [syntalos_fabric_dep,
                   qt_test_dep]
)
test('sy-test-edlstorage',
    test_edlstorage_exe,
    env: test_env,
)

#
# TSync file verification
#
test_tsyncfile_moc_src = ['test-tsyncfile.cpp']
test_tsyncfile_moc = qt.compile_moc(sources: test_tsyncfile_moc_src)
test_tsyncfile_exe = executable('test-tsyncfile',
    [test_tsyncfile_moc_src, test_tsyncfile_moc],
    dependencies: [syntalos_fabric_dep, qt_test_dep]
)
test('sy-test-tsyncfile',
    test_tsyncfile_exe,
    env: test_env,
)

#
# Sample Python GUI Project Tests
#
gui_project_test_runner = find_program('run-project-test.py')

test_gui_env = environment()
test_gui_env.set('MIMALLOC_SHOW_ERRORS', '1')

test_manifests = [
    'scenarios/jsonwriter-test.toml',
    'scenarios/plotter-smoketest.toml',
    'scenarios/video-transform-crop.toml',
    'scenarios/ipc-test-simple.toml',
    'scenarios/ipc-test-pycpp.toml',
]

if get_option('gui-tests')
    foreach manifest : test_manifests
        test_name = 'sy-test-project-@0@'.format(manifest.split('/')[-1].split('.')[0])
        test(test_name,
            gui_project_test_runner,
            args: ['--syntalos-bin', syntalos_exe, '--manifest', files(manifest)],
            env: test_gui_env,
            timeout: 600,
            is_parallel: false,
            suite: 'gui',
        )
    endforeach
endif
