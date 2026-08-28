# Build definitions for the camera-arv module unit tests

arv_test_env = environment()
arv_test_env.set('MIMALLOC_SHOW_ERRORS', '1')

#
# Frame decoder tests
#
# The decoders are compiled into the test directly instead of linking the module,
# so we can instantiate them without going through the static plugin registry.
#
test_arvdecoders_moc_src = ['test-arvdecoders.cpp']
test_arvdecoders_moc = qt.compile_moc(
    headers: ['../qarv/decoders/mono12packed.h'],
    sources: test_arvdecoders_moc_src,
    dependencies: [qt_gui_dep, aravis_dep],
    extra_args: ['--no-notes'],
)
test_arvdecoders_exe = executable('test-arvdecoders',
    [test_arvdecoders_moc_src, test_arvdecoders_moc,
     '../qarv/decoders/mono12packed.cpp',
     '../qarv/decoders/swscaledecoder.cpp'],
    include_directories: include_directories('..'),
    # matches the module build: the decoder plugins are registered statically
    cpp_args: ['-DQT_STATICPLUGIN', '-Wno-missing-declarations'],
    dependencies: [syntalos_fabric_dep,
                   qt_test_dep,
                   qt_gui_dep,
                   aravis_dep,
                   avutil_dep,
                   swscale_dep,
                   opencv_dep]
)
test('sy-test-arvdecoders',
    test_arvdecoders_exe,
    env: arv_test_env,
    is_parallel: true,
)
