
add_subdirectory(modules)

set(MAZEAMAZE_SRC
    main.cpp
    ma-private.h
    barrier.h
    utils.h
    utils.cpp
    mainwindow.h
    mainwindow.cpp
    statuswidget.h
    statuswidget.cpp
    testsubjectlistmodel.h
    testsubjectlistmodel.cpp
    moduleindicator.h
    moduleindicator.cpp
    abstractmodule.h
    abstractmodule.cpp
    modulemanager.h
    modulemanager.cpp
    moduleselectdialog.h
    moduleselectdialog.cpp
    imagesourcemodule.h
    imagesourcemodule.cpp
)

set(SCRIPT_SRC
    pyscript/maio.h
    pyscript/maio.cpp
    pyscript/pycontroller.h
    pyscript/pycontroller.cpp
    pyscript/mazescript.h
    pyscript/mazescript.cpp
)

set(MAZEAMAZE_UI
    mainwindow.ui
    moduleindicator.ui
    moduleselectdialog.ui
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

set(VIDEO_SRC
    video/mazevideo.h
    video/mazevideo.cpp
    video/tracker.h
    video/tracker.cpp
    video/simplevprobe.h
    video/simplevprobe.cpp
    video/genericcamera.h
    video/genericcamera.cpp
    video/videoviewwidget.h
    video/videoviewwidget.cpp
)

set(PLOT_SRC
    traceplot/traceplot.h
    traceplot/traceplot.cpp
    traceplot/traceview.h
    traceplot/traceview.cpp
    traceplot/traceplotproxy.h
    traceplot/traceplotproxy.cpp
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
    ${SCRIPT_SRC}
    ${FIRMATA_SRC}
    ${VIDEO_SRC}
    ${VIDEO_UEYE_SRC}
)

target_link_libraries(mazeamaze
    ${CMAKE_DL_LIBS}
    Qt5::Widgets
    Qt5::SerialPort
    Qt5::OpenGL
    KF5::DBusAddons
    KF5::Archive
    KF5::TextEditor
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    Qt5::Charts
    ${PYTHON_LIBRARIES}
    ${OpenCV_LIBS}
    ${OPENGL_LIBRARIES}
    ${UYEYE_LIB}

    mod_intan_rhd2000
    mod_traceplot
)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${PYTHON_INCLUDE_DIRS}
)

install(TARGETS mazeamaze DESTINATION ${CMAKE_INSTALL_BINDIR})
