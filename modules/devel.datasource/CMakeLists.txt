
set(DEVEL_DATASOURCE_SRC
    datasourcemodule.h
    datasourcemodule.cpp
)

add_library(mod_devel_datasource STATIC
    ${DEVEL_DATASOURCE_SRC}
)

target_link_libraries(mod_devel_datasource
    ${SY_BASE_LIBS}
    ${OpenCV_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_devel_datasource PARENT_SCOPE)
