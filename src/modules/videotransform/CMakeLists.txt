
set(VIDEOTRANSFORM_SRC
    videotransformmodule.h
    videotransformmodule.cpp
    vtransformctldialog.h
    vtransformctldialog.cpp
    videotransform.h
    videotransform.cpp
    vtransformlistmodel.h
    vtransformlistmodel.cpp
)

qt5_wrap_ui(VIDEOTRANSFORM_UI_SRC vtransformctldialog.ui)

add_library(mod_videotransform STATIC
    ${VIDEOTRANSFORM_SRC}
    ${VIDEOTRANSFORM_UI_SRC}
)

target_link_libraries(mod_videotransform
    ${SY_BASE_LIBS}
    ${OpenCV_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_videotransform PARENT_SCOPE)
