
set(DEVEL_PYOOPTEST_SRC
    pyooptestmodule.h
    pyooptestmodule.cpp
)

add_library(mod_devel_pyooptest STATIC
    ${DEVEL_PYOOPTEST_SRC}
)

target_link_libraries(mod_devel_pyooptest
    Qt5::Widgets
    ${OpenCV_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/oop
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_devel_pyooptest PARENT_SCOPE)
