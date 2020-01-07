 
remove_definitions("-Wextra")
add_subdirectory(rwqueue)

include_directories(
    ${CMAKE_SOURCE_DIR}/src/
)

add_executable(test-streamperf
    test-streamperf.cpp
    ${CMAKE_SOURCE_DIR}/src/streams/stream.cpp
    testbarrier.h
)
target_link_libraries(test-streamperf
    ${CMAKE_THREAD_LIBS_INIT}
    Qt5::Test
    ${OpenCV_LIBS}
)
add_test(streamperf test-streamperf)
