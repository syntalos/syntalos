
set(GENERIC_CAMERA_SRC
    genericcameramodule.h
    genericcameramodule.cpp
    camera.h
    camera.cpp
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
    ${SY_BASE_LIBS}
    ${OpenCV_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_generic_camera PARENT_SCOPE)
