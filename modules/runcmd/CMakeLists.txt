
set(RUNCMD_SRC
    runcmdmodule.h
    runcmdmodule.cpp
    runcmdsettingsdlg.h
    runcmdsettingsdlg.cpp
)

qt5_wrap_ui(RUNCMD_UI_SRC runcmdsettingsdlg.ui)

add_library(mod_runcmd STATIC
    ${RUNCMD_SRC}
    ${RUNCMD_UI_SRC}
)

target_link_libraries(mod_runcmd
    ${SY_BASE_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_runcmd PARENT_SCOPE)
