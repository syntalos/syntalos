# Out-of-process module glue code

qt5_generate_repc(REP_WORKER_R_SRC interface.rep REPLICA)

set(OOP_SHARED_SRC
    sharedmemory.h
    sharedmemory.cpp
    cvmatshm.h
    cvmatshm.cpp
    ipctypes.h
)

set(MA_OOP_SRC
    ${REP_WORKER_R_SRC}
    oopmodule.h
    oopmodule.cpp
    oopworkerconnector.h
    oopworkerconnector.cpp
)

add_library(lslice_oopshared STATIC
    ${OOP_SHARED_SRC}
)

add_library(lslice_oop STATIC
    ${MA_OOP_SRC}
)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ..
)

target_link_libraries(lslice_oopshared
    ${CMAKE_THREAD_LIBS_INIT}
    ${RT_LIB}
    Qt5::Core
    Qt5::RemoteObjects
    ${OpenCV_LIBS}
)

set(MA_OOPSHARED_LIB lslice_oopshared PARENT_SCOPE)

target_link_libraries(lslice_oop
    lslice_oopshared
    ${CMAKE_THREAD_LIBS_INIT}
    ${RT_LIB}
    Qt5::Core
    Qt5::Widgets
    Qt5::RemoteObjects
    ${OpenCV_LIBS}
)

set(MA_OOP_LIB lslice_oop PARENT_SCOPE)
