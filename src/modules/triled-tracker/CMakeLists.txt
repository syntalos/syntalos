
set(TRILED_TRACKER_SRC
    triledtrackermodule.h
    triledtrackermodule.cpp
    ledtrackersettingsdialog.h
    ledtrackersettingsdialog.cpp
    tracker.h
    tracker.cpp
)

set(TRILED_TRACKER_UI
    ledtrackersettingsdialog.ui
)

qt5_wrap_ui(TRILED_TRACKER_UI_SRC ${TRILED_TRACKER_UI})

add_library(mod_triled_tracker STATIC
    ${TRILED_TRACKER_SRC}
    ${TRILED_TRACKER_UI_SRC}
)

target_link_libraries(mod_triled_tracker
    Qt5::Widgets
    ${OpenCV_LIBS}
    ${Boost_LIBRARIES}
)

include_directories(
    ${Boost_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_triled_tracker PARENT_SCOPE)
