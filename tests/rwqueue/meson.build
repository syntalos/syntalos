
test_rwqueue_stab_exe = executable('test-rwqueue-stab',
    ['stabtest.cpp',
     'common/simplethread.h',
     'common/simplethread.cpp'],
    dependencies: [syntalos_fabric_dep,
                   qt_test_dep]
)

test_rwqueue_exe = executable('test-rwqueue',
    ['unittests.cpp',
     'minitest.h',
     'common/simplethread.h',
     'common/simplethread.cpp'],
    dependencies: [syntalos_fabric_dep,
                   qt_test_dep]
)
test('sy-test-rwqueue',
    test_rwqueue_exe,
    args: ['--disable-prompt'],
    timeout: 120,
    is_parallel: false
)
