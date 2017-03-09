
set(MAZEAMAZE_SRC
    main.cpp
    ma-private.h
    utils.h
    utils.cpp
    mainwindow.h
    mainwindow.cpp
    mazeio.h
    mazeio.cpp
    mazescript.h
    mazescript.cpp
    statuswidget.h
    statuswidget.cpp
    testsubjectlistmodel.h
    testsubjectlistmodel.cpp
)

set(MAZEAMAZE_UI
    mainwindow.ui
)

qt5_wrap_ui(MAZEAMAZE_UI_SRC ${MAZEAMAZE_UI})

set(FIRMATA_SRC
    firmata/backend.cpp
    firmata/backend.h
    firmata/serialinfo.cpp
    firmata/serialinfo.h
    firmata/serialport.cpp
    firmata/serialport.h
    firmata/fmutils.h
)

set(INTANREC_SRC
    intanrec/auxdigoutconfigdialog.cpp
    intanrec/auxdigoutconfigdialog.h
    intanrec/bandwidthdialog.cpp
    intanrec/bandwidthdialog.h
    intanrec/cabledelaydialog.cpp
    intanrec/cabledelaydialog.h
    intanrec/globalconstants.h
    intanrec/helpdialogchipfilters.cpp
    intanrec/helpdialogchipfilters.h
    intanrec/helpdialogcomparators.cpp
    intanrec/helpdialogcomparators.h
    intanrec/helpdialogdacs.cpp
    intanrec/helpdialogdacs.h
    intanrec/helpdialogfastsettle.cpp
    intanrec/helpdialogfastsettle.h
    intanrec/helpdialoghighpassfilter.cpp
    intanrec/helpdialoghighpassfilter.h
    intanrec/helpdialognotchfilter.cpp
    intanrec/helpdialognotchfilter.h
    intanrec/impedancefreqdialog.cpp
    intanrec/impedancefreqdialog.h
    intanrec/intanui.cpp
    intanrec/intanui.h
    intanrec/keyboardshortcutdialog.cpp
    intanrec/keyboardshortcutdialog.h
    intanrec/okFrontPanel.dll
    intanrec/okFrontPanelDLL.cpp
    intanrec/okFrontPanelDLL.h
    intanrec/qtincludes.h
    intanrec/randomnumber.cpp
    intanrec/randomnumber.h
    intanrec/renamechanneldialog.cpp
    intanrec/renamechanneldialog.h
    intanrec/rhd2000datablock.cpp
    intanrec/rhd2000datablock.h
    intanrec/rhd2000evalboard.cpp
    intanrec/rhd2000evalboard.h
    intanrec/rhd2000registers.cpp
    intanrec/rhd2000registers.h
    intanrec/setsaveformatdialog.cpp
    intanrec/setsaveformatdialog.h
    intanrec/signalchannel.cpp
    intanrec/signalchannel.h
    intanrec/signalgroup.cpp
    intanrec/signalgroup.h
    intanrec/signalprocessor.cpp
    intanrec/signalprocessor.h
    intanrec/signalsources.cpp
    intanrec/signalsources.h
    intanrec/spikeplot.cpp
    intanrec/spikeplot.h
    intanrec/spikescopedialog.cpp
    intanrec/spikescopedialog.h
    intanrec/triggerrecorddialog.cpp
    intanrec/triggerrecorddialog.h
    intanrec/waveplot.cpp
    intanrec/waveplot.h
)

set(VIDEO_SRC
    video/simplevprobe.h
    video/simplevprobe.cpp
    video/genericcamera.h
    video/genericcamera.cpp
    video/videotracker.h
    video/videotracker.cpp
    video/videoviewwidget.h
    video/videoviewwidget.cpp
)

set(UYEYE_LIB "")
set(VIDEO_UEYE_SRC "")
if (UEYE_CAMERA)
    set(UYEYE_LIB "ueye_api")
    set(VIDEO_UEYE_SRC
        video/ueyecamera.h
        video/ueyecamera.cpp
    )
endif ()

set(MAZEAMAZE_RES
    ../data/resources.qrc
)

qt5_add_resources(MAZEAMAZE_RES_SRC ${MAZEAMAZE_RES})

add_executable(mazeamaze
    ${MAZEAMAZE_SRC}
    ${MAZEAMAZE_RES_SRC}
    ${MAZEAMAZE_UI_SRC}
    ${INTANREC_SRC}
    ${FIRMATA_SRC}
    ${VIDEO_SRC}
    ${VIDEO_UEYE_SRC}
)

target_link_libraries(mazeamaze
    ${CMAKE_DL_LIBS}
    Qt5::Widgets
    Qt5::SerialPort
    Qt5::Script
    Qt5::OpenGL
    KF5::DBusAddons
    KF5::Archive
    KF5::TextEditor
    Qt5::ScriptTools
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    ${OpenCV_LIBS}
    ${OPENGL_LIBRARIES}
    ${UYEYE_LIB}
)

install(TARGETS mazeamaze DESTINATION ${CMAKE_INSTALL_BINDIR})
