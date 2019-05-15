# MaPyWorker helper process

set(MAPYWORKER_SRC
    main.cpp
    zmqclient.h
    zmqclient.cpp
    pycontroller.h
    pycontroller.cpp
)

add_executable(mapyworker ${MAPYWORKER_SRC})

target_link_libraries(mapyworker
    Qt5::Core
    ${CZMQ_LIBRARIES}
)

include_directories(
    ${CZMQ_INCLUDE_DIRS}
)
