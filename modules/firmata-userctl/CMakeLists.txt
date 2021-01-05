
set(FIRMATA_USERCTL_SRC
    firmatauserctlmod.h
    firmatauserctlmod.cpp
    firmatactldialog.h
    firmatactldialog.cpp
)

qt5_wrap_ui(FIRMATA_USERCTL_UI_SRC firmatactldialog.ui)

add_library(mod_firmata_userctl STATIC
    ${FIRMATA_USERCTL_SRC}
    ${FIRMATA_USERCTL_UI_SRC}
)

target_link_libraries(mod_firmata_userctl
    ${SY_BASE_LIBS}
    ${OpenCV_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_firmata_userctl PARENT_SCOPE)
