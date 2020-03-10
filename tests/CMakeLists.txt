 
remove_definitions("-Wextra")
add_subdirectory(rwqueue)

include_directories(
    ${CMAKE_SOURCE_DIR}/src/
)

#
# Basic Stream Performance Test
#
add_executable(test-streamperf
    test-streamperf.cpp
    testbarrier.h
    ${CMAKE_SOURCE_DIR}/src/streams/stream.cpp
    ${CMAKE_SOURCE_DIR}/src/syclock.cpp
)
target_link_libraries(test-streamperf
    ${SY_BASE_LIBS}
    Qt5::Test
    ${OpenCV_LIBS}
)
add_test(streamperf test-streamperf)
set_tests_properties(streamperf PROPERTIES RUN_SERIAL TRUE)

#
# Basic Timer/HRClock Test
#
add_executable(test-timer
    test-timer.cpp
    ${CMAKE_SOURCE_DIR}/src/syclock.cpp
)
target_link_libraries(test-timer
    ${SY_BASE_LIBS}
    Qt5::Test
)
add_test(timer test-timer)
set_tests_properties(timer PROPERTIES RUN_SERIAL TRUE)
