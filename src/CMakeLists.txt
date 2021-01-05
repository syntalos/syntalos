# CMakeLists for Syntalos main

add_subdirectory(oop)
add_subdirectory(pyworker)

set(SYNTALOS_SHARED_SRC
    edlstorage.h
    edlstorage.cpp
    eigenaux.h
    moduleapi.h
    moduleapi.cpp
    optionalwaitcondition.h
    optionalwaitcondition.cpp
    rangeslider.h
    rangeslider.cpp
    subscriptionwatcher.h
    subscriptionwatcher.cpp
    syclock.h
    syclock.cpp
    timesync.h
    timesync.cpp
    tsyncfile.h
    tsyncfile.cpp

    utils/misc.h
    utils/misc.cpp
    utils/tomlutils.h
    utils/tomlutils.cpp

    streams/atomicops.h
    streams/datatypes.h
    streams/datatypes.cpp
    streams/frametype.h
    streams/readerwriterqueue.h
    streams/stream.h
    streams/stream.cpp
)
set(SYNTALOS_SHARED_SRC ${SYNTALOS_SHARED_SRC} PARENT_SCOPE)

set(SYNTALOS_SRC
    aboutdialog.h
    aboutdialog.cpp
    cpuaffinity.h
    cpuaffinity.cpp
    elidedlabel.h
    elidedlabel.cpp
    entitylistmodels.h
    entitylistmodels.cpp
    engine.h
    engine.cpp
    flowgraphview.h
    flowgraphview.cpp
    globalconfig.h
    globalconfig.cpp
    globalconfigdialog.h
    globalconfigdialog.cpp
    main.cpp
    mainwindow.h
    mainwindow.cpp
    meminfo.h
    meminfo.cpp
    moduleeventthread.h
    moduleeventthread.cpp
    modulegraphform.h
    modulegraphform.cpp
    modulelibrary.h
    modulelibrary.cpp
    moduleselectdialog.h
    moduleselectdialog.cpp
    rtkit.h
    rtkit.cpp
    sysinfo.h
    sysinfo.cpp
    sysinfodialog.h
    sysinfodialog.cpp
    timingsdialog.h
    timingsdialog.cpp
)

set(SYNTALOS_UI
    aboutdialog.ui
    globalconfigdialog.ui
    mainwindow.ui
    modulegraphform.ui
    moduleselectdialog.ui
    sysinfodialog.ui
    timingsdialog.ui
)

qt5_wrap_ui(SYNTALOS_UI_SRC ${SYNTALOS_UI})

set(SYNTALOS_RES
    ../data/resources.qrc
)

qt5_add_resources(SYNTALOS_RES_SRC ${SYNTALOS_RES})

add_library(syntalos_shared STATIC
    ${SYNTALOS_SHARED_SRC}
)

target_link_libraries(syntalos_shared
    ${SY_BASE_LIBS}
    ${OpenCV_LIBS}
    ${OPENGL_LIBRARIES}
)

add_executable(syntalos
    ${SYNTALOS_SRC}
    ${SYNTALOS_RES_SRC}
    ${SYNTALOS_UI_SRC}
)

target_link_libraries(syntalos
    syntalos_shared
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    ${RT_LIB}
    Qt5::Widgets
    Qt5::SerialPort
    Qt5::OpenGL
    Qt5::Svg
    Qt5::DBus
    KF5::DBusAddons
    KF5::Archive
    KF5::TextEditor
    Qt5::Charts
    Qt5::RemoteObjects
    Eigen3::Eigen
    ${GLIB_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${OpenCV_LIBS}
    ${FFMPEG_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${MA_EXTRA_LIBS}

    ${MA_MODULE_LIBS}
    ${MA_OOP_LIB} # this has to come *after* modules to ensure link order
)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${XXHASH_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${TOML_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
)

install(TARGETS syntalos DESTINATION ${CMAKE_INSTALL_BINDIR})
