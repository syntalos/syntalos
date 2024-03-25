
subdir('rwqueue')

#
# Basic Stream Performance Test
#
test_streamperf_moc_src = ['test-streamperf.cpp']
test_streamperf_moc = qt.preprocess(moc_sources: test_streamperf_moc_src)
test_streamperf_exe = executable('test-streamperf',
    [test_streamperf_moc_src, test_streamperf_moc,
     'testbarrier.h'],
    dependencies: [syntalos_fabric_dep,
                   qt_test_dep,
                   opencv_dep]
)
test('sy-test-streamperf',
    test_streamperf_exe,
    timeout: 120,
    is_parallel: false
)


#
# Basic Timer/HRClock Test
#
test_timer_moc_src = ['test-timer.cpp']
test_timer_moc = qt.preprocess(moc_sources: test_timer_moc_src)
test_timer_exe = executable('test-timer',
    [test_timer_moc_src, test_timer_moc],
    dependencies: [syntalos_fabric_dep,
                   qt_test_dep]
)
test('sy-test-timer',
    test_timer_exe,
    is_parallel: false
)

#
# Experiment Directory Layout Test
#
test_edlstorage_moc_src = ['test-edlstorage.cpp']
test_edlstorage_moc = qt.preprocess(moc_sources: test_edlstorage_moc_src)
test_edlstorage_exe = executable('test-edlstorage',
    [test_edlstorage_moc_src, test_edlstorage_moc],
    dependencies: [syntalos_fabric_dep,
                   qt_test_dep]
)
test('sy-test-edlstorage',
    test_edlstorage_exe
)

#
# TSync file verification
#
test_tsyncfile_moc_src = ['test-tsyncfile.cpp']
test_tsyncfile_moc = qt.preprocess(moc_sources: test_tsyncfile_moc_src)
test_tsyncfile_exe = executable('test-tsyncfile',
    [test_tsyncfile_moc_src, test_tsyncfile_moc],
    dependencies: [syntalos_fabric_dep,
                   qt_test_dep]
)
test('sy-test-tsyncfile',
    test_tsyncfile_exe
)
