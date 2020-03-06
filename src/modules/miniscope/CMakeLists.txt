
pkg_check_modules(LIBMINISCOPE miniscope>=0.1 REQUIRED)

set(MINISCOPE_SRC
    miniscopemodule.h
    miniscopemodule.cpp
    miniscopesettingsdialog.h
    miniscopesettingsdialog.cpp
)

set(MINISCOPE_UI
    miniscopesettingsdialog.ui
)

qt5_wrap_ui(MINISCOPE_UI_SRC ${MINISCOPE_UI})

add_library(mod_miniscope STATIC
    ${MINISCOPE_SRC}
    ${MINISCOPE_UI_SRC}
)

target_link_libraries(mod_miniscope
    ${SY_BASE_LIBS}
    ${OpenCV_LIBS}
    ${LIBMINISCOPE_LIBRARIES}
)

include_directories(
    ${Boost_INCLUDE_DIR}
    ${LIBMINISCOPE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_EXTRA_LIBS ${MA_EXTRA_LIBS} ${LIBMINISCOPE_LIBRARIES} PARENT_SCOPE)
set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_miniscope PARENT_SCOPE)
