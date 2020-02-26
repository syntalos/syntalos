# CMakeLists for Syntalos main

add_subdirectory(modules)
add_subdirectory(oop)
add_subdirectory(pyworker)

set(SYNTALOS_SRC
    main.cpp
    engine.h
    engine.cpp
    aboutdialog.h
    aboutdialog.cpp
    optionalwaitcondition.h
    optionalwaitcondition.cpp
    utils.h
    utils.cpp
    hrclock.h
    hrclock.cpp
    mainwindow.h
    mainwindow.cpp
    testsubjectlistmodel.h
    testsubjectlistmodel.cpp
    moduleapi.h
    moduleapi.cpp
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

    streams/datatypes.h
    streams/datatypes.cpp
    streams/frametype.h
    streams/stream.h
    streams/stream.cpp
    streams/atomicops.h
    streams/readerwriterqueue.h
)

set(SYNTALOS_UI
    mainwindow.ui
    moduleselectdialog.ui
    aboutdialog.ui
    modulegraphform.ui
)

qt5_wrap_ui(SYNTALOS_UI_SRC ${SYNTALOS_UI})

set(SYNTALOS_RES
    ../data/resources.qrc
)

qt5_add_resources(SYNTALOS_RES_SRC ${SYNTALOS_RES})

add_executable(syntalos
    ${SYNTALOS_SRC}
    ${SYNTALOS_RES_SRC}
    ${SYNTALOS_UI_SRC}
)

target_link_libraries(syntalos
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
    ${PYTHON_LIBRARIES}
    ${OpenCV_LIBS}
    ${OPENGL_LIBRARIES}
    ${Boost_LIBRARIES}
    ${UEYE_LIBS}
    ${MA_EXTRA_LIBS}

    ${MA_MODULE_LIBS}
    ${MA_OOP_LIB} # this has to come *after* modules to ensure link order
)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${PYTHON_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIR}
)

install(TARGETS syntalos DESTINATION ${CMAKE_INSTALL_BINDIR})
