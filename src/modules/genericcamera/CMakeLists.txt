
set(GENERIC_CAMERA_SRC
    genericcameramodule.h
    genericcameramodule.cpp
    simplevprobe.h
    simplevprobe.cpp
    genericcamera.h
    genericcamera.cpp
    genericcamerasettingsdialog.h
    genericcamerasettingsdialog.cpp
)

set(GENERIC_CAMERA_UI
    genericcamerasettingsdialog.ui
)

qt5_wrap_ui(GENERIC_CAMERA_UI_SRC ${GENERIC_CAMERA_UI})

add_library(mod_generic_camera STATIC
    ${GENERIC_CAMERA_SRC}
    ${GENERIC_CAMERA_UI_SRC}
)

target_link_libraries(mod_generic_camera
    ${CMAKE_THREAD_LIBS_INIT}
    Qt5::Widgets
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    ${OpenCV_LIBS}
    ${Boost_LIBRARIES}
)

include_directories(
    ${Boost_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
