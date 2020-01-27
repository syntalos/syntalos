# PyWorker helper process

find_package(Boost 1.60 REQUIRED COMPONENTS python3)
find_package(NumPy REQUIRED)

# Boost.Python has a few of these in templates
remove_definitions("-Wzero-as-null-pointer-constant")

qt5_generate_repc(REP_WORKER_S_SRC ../oop/interface.rep SOURCE)

set(MAPYWORKER_SRC
    main.cpp
    worker.h
    worker.cpp
    maio.h
    maio.cpp
    ../hrclock.h
    ../hrclock.cpp
    cvmatndsliceconvert.h
    cvmatndsliceconvert.cpp
)

add_executable(pyworker
    ${MAPYWORKER_SRC}
    ${REP_WORKER_S_SRC}
)

target_link_libraries(pyworker
    ${CMAKE_THREAD_LIBS_INIT}
    ${RT_LIB}
    ${MA_OOPSHARED_LIB}
    Qt5::Core
    Qt5::RemoteObjects
    ${PYTHON_LIBRARIES}
    ${Boost_LIBRARIES}
    ${OpenCV_LIBS}
)

include_directories(
    ..
    ../oop/
    ${PYTHON_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${NUMPY_INCLUDE_DIRS}
)

install(TARGETS pyworker DESTINATION ${MA_ARCH_INSTALL_DIR})
