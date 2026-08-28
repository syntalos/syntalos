# Build definitions for video recorder unit tests

videorecorder_test_moc_src = ['test-videorecorder.cpp']
videorecorder_test_moc = qt.compile_moc(
    sources: videorecorder_test_moc_src,
    dependencies: [qt_test_dep],
    extra_args: ['--no-notes'],
)
videorecorder_test_exe = executable('test-videorecorder',
    [
        videorecorder_test_moc_src,
        videorecorder_test_moc,
        '../videowriter.cpp',
        '../encodehelper/videoreader.cpp',
    ],
    include_directories: [root_include_dir, include_directories('..', '../encodehelper')],
    dependencies: [syntalos_fabric_dep,
                   qt_test_dep,
                   libsystemd_dep,
                   opencv_dep,
                   avcodec_dep,
                   avformat_dep,
                   avutil_dep,
                   swscale_dep],
)

videorecorder_test_env = environment()
videorecorder_test_env.set('MIMALLOC_SHOW_ERRORS', '1')
test('sy-test-videorecorder',
    videorecorder_test_exe,
    env: videorecorder_test_env,
    is_parallel: true,
)
