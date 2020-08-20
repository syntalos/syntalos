
set(VIDEOTRANSFORM_SRC
    videotransformmodule.h
    videotransformmodule.cpp
)

add_library(mod_videotransform STATIC
    ${VIDEOTRANSFORM_SRC}
)

target_link_libraries(mod_videotransform
    ${SY_BASE_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_videotransform PARENT_SCOPE)
