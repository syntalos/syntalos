
add_subdirectory(mapyworker)

set(PYSCRIPT_SRC
    pyscriptmodule.h
    pyscriptmodule.cpp
    zmqserver.h
    zmqserver.cpp
    rpc-shared-info.h
    mafuncrelay.h
    mafuncrelay.cpp
    eventtable.h
    eventtable.cpp
)

add_library(mod_pyscript STATIC
    ${PYSCRIPT_SRC}
)

target_link_libraries(mod_pyscript
    Qt5::Widgets
    KF5::TextEditor
    ${CZMQ_LIBRARIES}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CZMQ_INCLUDE_DIRS}
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_pyscript PARENT_SCOPE)
