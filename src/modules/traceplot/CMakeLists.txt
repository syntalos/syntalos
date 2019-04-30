
set(TRACEPLOT_SRC
    traceplot.h
    traceplot.cpp
    traceview.h
    traceview.cpp
    traceplotproxy.h
    traceplotproxy.cpp
    traceplotmodule.h
    traceplotmodule.cpp
    tracedisplay.h
    tracedisplay.cpp
)

set(TRACEPLOT_UI
    tracedisplay.ui
)

qt5_wrap_ui(TRACEPLOT_UI_SRC ${TRACEPLOT_UI})

add_library(mod_traceplot STATIC
    ${TRACEPLOT_SRC}
    ${TRACEPLOT_UI}
)

target_link_libraries(mod_traceplot
    Qt5::Widgets
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)
