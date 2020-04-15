
pkg_check_modules(TCAM REQUIRED tcam)

set(TIS_CAMERA_SRC
    tiscameramodule.h
    tiscameramodule.cpp
)

add_library(mod_tis_camera STATIC
    ${TIS_CAMERA_SRC}
)

target_link_libraries(mod_tis_camera
    ${SY_BASE_LIBS}
    ${OpenCV_LIBS}
    ${GSTREAMER_LIBRARIES}
    ${TCAM_LIBRARIES}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${GSTREAMER_INCLUDE_DIRS}
    ${TCAM_INCLUDE_DIRS}
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_tis_camera PARENT_SCOPE)
