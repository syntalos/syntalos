
set(INTAN_RHD2000_SRC
    rhd2000module.h
    rhd2000module.cpp
    auxdigoutconfigdialog.cpp
    auxdigoutconfigdialog.h
    bandwidthdialog.cpp
    bandwidthdialog.h
    cabledelaydialog.cpp
    cabledelaydialog.h
    globalconstants.h
    helpdialogchipfilters.cpp
    helpdialogchipfilters.h
    helpdialogcomparators.cpp
    helpdialogcomparators.h
    helpdialogdacs.cpp
    helpdialogdacs.h
    helpdialogfastsettle.cpp
    helpdialogfastsettle.h
    helpdialoghighpassfilter.cpp
    helpdialoghighpassfilter.h
    helpdialognotchfilter.cpp
    helpdialognotchfilter.h
    impedancefreqdialog.cpp
    impedancefreqdialog.h
    intanui.cpp
    intanui.h
    keyboardshortcutdialog.cpp
    keyboardshortcutdialog.h
    okFrontPanelDLL.cpp
    okFrontPanelDLL.h
    qtincludes.h
    randomnumber.cpp
    randomnumber.h
    renamechanneldialog.cpp
    renamechanneldialog.h
    rhd2000datablock.cpp
    rhd2000datablock.h
    rhd2000evalboard.cpp
    rhd2000evalboard.h
    rhd2000registers.cpp
    rhd2000registers.h
    setsaveformatdialog.cpp
    setsaveformatdialog.h
    signalchannel.cpp
    signalchannel.h
    signalgroup.cpp
    signalgroup.h
    signalprocessor.cpp
    signalprocessor.h
    signalsources.cpp
    signalsources.h
    spikeplot.cpp
    spikeplot.h
    spikescopedialog.cpp
    spikescopedialog.h
    triggerrecorddialog.cpp
    triggerrecorddialog.h
    waveplot.cpp
    waveplot.h
)

add_library(mod_intan_rhd2000 STATIC
    ${INTAN_RHD2000_SRC}
)

target_link_libraries(mod_intan_rhd2000
    ${CMAKE_DL_LIBS}
    Qt5::Widgets
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_intan_rhd2000 PARENT_SCOPE)
