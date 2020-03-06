
set(PYSCRIPT_SRC
    pyscriptmodule.h
    pyscriptmodule.cpp
    porteditordialog.h
    porteditordialog.cpp
)

set(PYSCRIPT_UI
    porteditordialog.ui
)

qt5_wrap_ui(PYSCRIPT_UI_SRC ${PYSCRIPT_UI})

add_library(mod_pyscript STATIC
    ${PYSCRIPT_SRC}
    ${PYSCRIPT_UI_SRC}
)

target_link_libraries(mod_pyscript
    ${SY_BASE_LIBS}
    KF5::TextEditor
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_pyscript PARENT_SCOPE)
