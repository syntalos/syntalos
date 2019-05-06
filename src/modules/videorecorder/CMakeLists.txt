
set(VIDEO_RECORDER_SRC
    videorecordmodule.h
    videorecordmodule.cpp
)

add_library(mod_videorecorder STATIC
    ${VIDEO_RECORDER_SRC}
)

target_link_libraries(mod_videorecorder
    Qt5::Widgets
    ${OpenCV_LIBS}
    ${Boost_LIBRARIES}
    ${FFMPEG_LIBRARIES}
)

include_directories(
    ${Boost_INCLUDE_DIR}
    ${FFMPEG_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)
