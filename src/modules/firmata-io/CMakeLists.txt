
set(FIRMATA_SRC
    firmata/backend.cpp
    firmata/backend.h
    firmata/serialinfo.cpp
    firmata/serialinfo.h
    firmata/serialport.cpp
    firmata/serialport.h
    firmata/fmutils.h
)

set(FIRMATAIO_SRC
    firmataiomodule.h
    firmataiomodule.cpp
    firmatasettingsdialog.h
    firmatasettingsdialog.cpp
)

set(FIRMATAIO_UI
    firmatasettingsdialog.ui
)

qt5_wrap_ui(FIRMATAIO_UI_SRC ${FIRMATAIO_UI})

add_library(mod_firmataio STATIC
    ${FIRMATAIO_SRC}
    ${FIRMATA_SRC}
    ${FIRMATAIO_UI_SRC}
)

target_link_libraries(mod_firmataio
    Qt5::Widgets
    Qt5::SerialPort
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_firmataio PARENT_SCOPE)
