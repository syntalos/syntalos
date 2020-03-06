
add_executable(test-rwqueue-stab
    stabtest.cpp
    common/simplethread.h
    common/simplethread.cpp
    ${CMAKE_SOURCE_DIR}/src/streams/stream.cpp
)

target_link_libraries(test-rwqueue-stab
    ${SY_BASE_LIBS}
    Qt5::Test
)

add_executable(test-rwqueue
    unittests.cpp
    minitest.h
    common/simplethread.h
    common/simplethread.cpp
    ${CMAKE_SOURCE_DIR}/src/streams/stream.cpp
)

add_test(rwqueue test-rwqueue --disable-prompt)

target_link_libraries(test-rwqueue
    ${SY_BASE_LIBS}
    Qt5::Test
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src/
)
