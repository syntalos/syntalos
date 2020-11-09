
set(DEVEL_CLOCK_SRC
    clockmodule.h
    clockmodule.cpp
    clocksettingsdialog.h
    clocksettingsdialog.cpp
)

qt5_wrap_ui(DEVEL_CLOCK_UI_SRC clocksettingsdialog.ui)

add_library(mod_devel_clock STATIC
    ${DEVEL_CLOCK_SRC}
    ${DEVEL_CLOCK_UI_SRC}
)

target_link_libraries(mod_devel_clock
    ${SY_BASE_LIBS}
    ${GLIB_LIBRARIES}
)

include_directories(
    ${GLIB_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_devel_clock PARENT_SCOPE)
