
add_executable(test-rwqueue-stab
    stabtest.cpp
    common/simplethread.h
    common/simplethread.cpp
)

target_link_libraries(test-rwqueue-stab
    ${CMAKE_THREAD_LIBS_INIT}
    Qt5::Test
)

add_executable(test-rwqueue
    unittests.cpp
    minitest.h
    common/simplethread.h
    common/simplethread.cpp
)

add_test(rwqueue test-rwqueue --disable-prompt)

target_link_libraries(test-rwqueue
    ${CMAKE_THREAD_LIBS_INIT}
    Qt5::Test
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src/
)
