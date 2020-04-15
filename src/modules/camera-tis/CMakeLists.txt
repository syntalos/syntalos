
# those are okay in most cases, since a lot of the code is C-style
# and having missing initializers is fine e.g. for padding fields
remove_definitions("-Wmissing-field-initializers")
add_definitions("-Wno-missing-field-initializers")

pkg_check_modules(TCAM REQUIRED tcam)

set(TIS_CAMERA_SRC
    tiscameramodule.h
    tiscameramodule.cpp
    tcamcamera.h
    tcamcamera.cpp
    cpropertiesdialog.h
    cpropertiesdialog.cpp
    cdeviceselectiondlg.h
    cdeviceselectiondlg.cpp
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
