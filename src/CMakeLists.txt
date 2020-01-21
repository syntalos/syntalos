
add_subdirectory(modules)

set(MAZEAMAZE_SRC
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
    videoviewwidget.h
    videoviewwidget.cpp
    elidedlabel.h
    elidedlabel.cpp
    flowgraphview.h
    flowgraphview.cpp
    modulegraphform.h
    modulegraphform.cpp

    streams/datatypes.h
    streams/frametype.h
    streams/stream.h
    streams/stream.cpp
    streams/atomicops.h
    streams/readerwriterqueue.h

    oop/oopmodule.h
    oop/oopmodule.cpp
    oop/oopworkerconnector.h
    oop/oopworkerconnector.cpp
    oop/sharedmemory.h
    oop/sharedmemory.cpp
    oop/cvmatshm.h
    oop/cvmatshm.cpp
)

set(MAZEAMAZE_UI
    mainwindow.ui
    moduleselectdialog.ui
    aboutdialog.ui
    modulegraphform.ui
)

qt5_wrap_ui(MAZEAMAZE_UI_SRC ${MAZEAMAZE_UI})

set(MAZEAMAZE_RES
    ../data/resources.qrc
)

qt5_add_resources(MAZEAMAZE_RES_SRC ${MAZEAMAZE_RES})
qt5_generate_repc(REP_WORKER_R_SRC oop/interface.rep REPLICA)

add_executable(mazeamaze
    ${MAZEAMAZE_SRC}
    ${MAZEAMAZE_RES_SRC}
    ${REP_WORKER_R_SRC}
    ${MAZEAMAZE_UI_SRC}
)

target_link_libraries(mazeamaze
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    ${LIB_RT}
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
    ${CZMQ_LIBRARIES}
    ${UEYE_LIBS}
    ${MA_EXTRA_LIBS}

    ${MA_MODULE_LIBS}
    miniscope
)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${PYTHON_INCLUDE_DIRS}
    ${CZMQ_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIR}
)

install(TARGETS mazeamaze DESTINATION ${CMAKE_INSTALL_BINDIR})
