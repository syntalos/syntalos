
set(DEVEL_DATASINK_SRC
    datasinkmodule.h
    datasinkmodule.cpp
)

add_library(mod_devel_datasink STATIC
    ${DEVEL_DATASINK_SRC}
)

target_link_libraries(mod_devel_datasink
    Qt5::Widgets
    ${OpenCV_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_devel_datasink PARENT_SCOPE)
