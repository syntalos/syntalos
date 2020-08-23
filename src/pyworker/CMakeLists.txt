# PyWorker helper process

find_package(pybind11 REQUIRED)
find_package(NumPy REQUIRED)

# QRO replica compiler generates code with a bunch of these, for compatibility
# (so we can't fail hard on these style issues)
remove_definitions("-Wzero-as-null-pointer-constant")

qt5_generate_repc(REP_WORKER_S_SRC ../oop/interface.rep SOURCE)

set(MAPYWORKER_SRC
    main.cpp
    worker.h
    worker.cpp
    syio.h
    syio.cpp
    qstringtopy.h
    ../rtkit.h
    ../rtkit.cpp
    ../cpuaffinity.h
    ../cpuaffinity.cpp
    ../syclock.h
    ../syclock.cpp
    ../streams/datatypes.h
    ../streams/datatypes.cpp
    cvmatndsliceconvert.h
    cvmatndsliceconvert.cpp
    pyipcmarshal.h
    pyipcmarshal.cpp
)

add_executable(pyworker
    ${MAPYWORKER_SRC}
    ${REP_WORKER_S_SRC}
)

target_link_libraries(pyworker
    ${CMAKE_THREAD_LIBS_INIT}
    ${RT_LIB}
    ${MA_OOPSHARED_LIB}
    ${SY_BASE_LIBS}
    pybind11::module
    Qt5::RemoteObjects
    Qt5::DBus
    ${PYTHON_LIBRARIES}
    ${OpenCV_LIBS}
)
set_target_properties(pyworker PROPERTIES COMPILE_FLAGS "-DNO_TID_PORTCONSTRUCTORS")

include_directories(
    ..
    ../oop/
    ${PYTHON_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${NUMPY_INCLUDE_DIRS}
)

install(TARGETS pyworker DESTINATION ${MA_ARCH_INSTALL_DIR})

# Generate some documentation files for the embedded scripting interface,
# if we can

find_program(PDOC_EXE pdoc)
if (PDOC_EXE)
    # the pdoc Python module is probably there, so we can try to refresh the documentation
    set(syio_doc_fname ${CMAKE_SOURCE_DIR}/docs/modules/pyapi_syio.rst)
    add_custom_command(
      DEPENDS pyworker
      COMMAND pyworker --doc ${syio_doc_fname}
      COMMAND ${CMAKE_COMMAND} -E touch syio_doc_gen.stamp
      OUTPUT syio_doc_gen.stamp
      COMMENT "Extracting embedded Python documentation"
      VERBATIM
    )

    add_custom_target(
      sysio-pdoc ALL
      DEPENDS syio_doc_gen.stamp
    )
endif()
