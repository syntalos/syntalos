
set(SCRIPT_SRC
    pyscript/maio.h
    pyscript/maio.cpp
    pyscript/pycontroller.h
    pyscript/pycontroller.cpp
    pyscript/mazescript.h
    pyscript/mazescript.cpp
)

set(PYSCRIPT_SRC
    pyscriptmodule.h
    pyscriptmodule.cpp
)

add_library(mod_pyscript STATIC
    ${PYSCRIPT_SRC}
)

target_link_libraries(mod_pyscript
    Qt5::Widgets
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_pyscript PARENT_SCOPE)
