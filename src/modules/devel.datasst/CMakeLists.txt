
set(DEVEL_DATASST_SRC
    datasstmodule.h
    datasstmodule.cpp
)

add_library(mod_devel_datasst STATIC
    ${DEVEL_DATASST_SRC}
)

target_link_libraries(mod_devel_datasst
    Qt5::Widgets
    ${OpenCV_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_devel_datasst PARENT_SCOPE)
