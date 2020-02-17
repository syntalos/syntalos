
set(PYSCRIPT_SRC
    pyscriptmodule.h
    pyscriptmodule.cpp
)

add_library(mod_pyscript STATIC
    ${PYSCRIPT_SRC}
)

target_link_libraries(mod_pyscript
    Qt5::Widgets
    KF5::TextEditor
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_pyscript PARENT_SCOPE)
