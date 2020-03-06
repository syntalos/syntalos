
set(UEYE_CAMERA_SRC
    ueyecameramodule.h
    ueyecameramodule.cpp
    ueyecamera.h
    ueyecamera.cpp
    ueyecamerasettingsdialog.h
    ueyecamerasettingsdialog.cpp
)

set(UEYE_CAMERA_UI
    ueyecamerasettingsdialog.ui
)

qt5_wrap_ui(UEYE_CAMERA_UI_SRC ${UEYE_CAMERA_UI})

add_library(mod_ueye_camera STATIC
    ${UEYE_CAMERA_SRC}
    ${UEYE_CAMERA_UI_SRC}
)

target_link_libraries(mod_ueye_camera
    ${SY_BASE_LIBS}
    ${OpenCV_LIBS}
    ${UEYE_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    "/usr/include/" # uEye
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_ueye_camera PARENT_SCOPE)
