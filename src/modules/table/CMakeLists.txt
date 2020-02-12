
set(TABLEMOD_SRC
    tablemodule.h
    tablemodule.cpp
    recordedtable.h
    recordedtable.cpp
)

add_library(mod_table STATIC
    ${TABLEMOD_SRC}
)

target_link_libraries(mod_table
    Qt5::Widgets
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_table PARENT_SCOPE)
