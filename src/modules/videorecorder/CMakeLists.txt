
set(VIDEO_RECORDER_SRC
    videorecordmodule.h
    videorecordmodule.cpp
    videowriter.h
    videowriter.cpp
    recordersettingsdialog.h
    recordersettingsdialog.cpp
)

set(VIDEO_RECORDER_UI
    recordersettingsdialog.ui
)

qt5_wrap_ui(VIDEO_RECORDER_UI_SRC ${VIDEO_RECORDER_UI})

add_library(mod_videorecorder STATIC
    ${VIDEO_RECORDER_SRC}
    ${VIDEO_RECORDER_UI_SRC}
)

target_link_libraries(mod_videorecorder
    syntalos_shared
    ${SY_BASE_LIBS}
    ${OpenCV_LIBS}
    ${FFMPEG_LIBRARIES}
)

include_directories(
    ${FFMPEG_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_videorecorder PARENT_SCOPE)
