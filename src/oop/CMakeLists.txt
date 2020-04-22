# Out-of-process module glue code

qt5_generate_repc(REP_WORKER_R_SRC interface.rep REPLICA)

set(OOP_SHARED_SRC
    sharedmemory.h
    sharedmemory.cpp
    ipcmarshal.h
    ipcmarshal.cpp
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
    ${SY_BASE_LIBS}
    Qt5::RemoteObjects
    ${OpenCV_LIBS}
)
set_target_properties(lslice_oopshared PROPERTIES COMPILE_FLAGS "-DNO_TID_PORTCONSTRUCTORS")

set(MA_OOPSHARED_LIB lslice_oopshared PARENT_SCOPE)

target_link_libraries(lslice_oop
    lslice_oopshared
    ${CMAKE_THREAD_LIBS_INIT}
    ${RT_LIB}
    ${SY_BASE_LIBS}
    Qt5::Widgets
    Qt5::RemoteObjects
    ${OpenCV_LIBS}
)
set_target_properties(lslice_oop PROPERTIES COMPILE_FLAGS "-Wno-zero-as-null-pointer-constant")

set(MA_OOP_LIB lslice_oop PARENT_SCOPE)
