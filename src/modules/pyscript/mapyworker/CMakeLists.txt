# MaPyWorker helper process

set(MAPYWORKER_SRC
    main.cpp
    zmqclient.h
    zmqclient.cpp
    ../rpc-shared-info.h
    pycontroller.h
    pycontroller.cpp
    maio.h
    maio.cpp
)

add_executable(mapyworker ${MAPYWORKER_SRC})

target_link_libraries(mapyworker
    Qt5::Core
    ${CZMQ_LIBRARIES}
    ${PYTHON_LIBRARIES}
)

include_directories(
    ${CZMQ_INCLUDE_DIRS}
    ${PYTHON_INCLUDE_DIRS}
)
