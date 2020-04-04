# CMakeLists for Syntalos main

add_subdirectory(oop)
add_subdirectory(pyworker)
add_subdirectory(modules)

set(SYNTALOS_SHARED_SRC
    optionalwaitcondition.h
    optionalwaitcondition.cpp
    utils.h
    utils.cpp
    eigenaux.h
    syclock.h
    syclock.cpp
    timesync.h
    timesync.cpp
    edlstorage.h
    edlstorage.cpp
    moduleapi.h
    moduleapi.cpp

    streams/datatypes.h
    streams/datatypes.cpp
    streams/frametype.h
    streams/stream.h
    streams/stream.cpp
    streams/atomicops.h
    streams/readerwriterqueue.h
)
set(SYNTALOS_SHARED_SRC ${SYNTALOS_SHARED_SRC} PARENT_SCOPE)

set(SYNTALOS_SRC
    main.cpp
    engine.h
    engine.cpp
    aboutdialog.h
    aboutdialog.cpp
    mainwindow.h
    mainwindow.cpp
    testsubjectlistmodel.h
    testsubjectlistmodel.cpp
    timingsdialog.h
    timingsdialog.cpp
    modulelibrary.h
    modulelibrary.cpp
    moduleselectdialog.h
    moduleselectdialog.cpp
    elidedlabel.h
    elidedlabel.cpp
    flowgraphview.h
    flowgraphview.cpp
    modulegraphform.h
    modulegraphform.cpp
)

set(SYNTALOS_UI
    mainwindow.ui
    moduleselectdialog.ui
    aboutdialog.ui
    modulegraphform.ui
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
    KF5::DBusAddons
    KF5::Archive
    KF5::TextEditor
    Qt5::Charts
    Qt5::RemoteObjects
    Eigen3::Eigen
    ${OpenCV_LIBS}
    ${OPENGL_LIBRARIES}
    ${UEYE_LIBS}
    ${MA_EXTRA_LIBS}

    ${MA_MODULE_LIBS}
    ${MA_OOP_LIB} # this has to come *after* modules to ensure link order
)

include_directories(
    ${TOML_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

install(TARGETS syntalos DESTINATION ${CMAKE_INSTALL_BINDIR})
