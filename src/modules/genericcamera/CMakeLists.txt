
set(GENERIC_CAMERA_SRC
    genericcameramodule.h
    genericcameramodule.cpp
    simplevprobe.h
    simplevprobe.cpp
    genericcamera.h
    genericcamera.cpp
)

add_library(mod_generic_camera STATIC
    ${GENERIC_CAMERA_SRC}
)

target_link_libraries(mod_generic_camera
    Qt5::Widgets
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    ${OpenCV_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)
