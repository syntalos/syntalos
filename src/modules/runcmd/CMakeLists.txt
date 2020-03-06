
set(RUNCMD_SRC
    runcmdmodule.h
    runcmdmodule.cpp
)

add_library(mod_runcmd STATIC
    ${RUNCMD_SRC}
)

target_link_libraries(mod_runcmd
    ${SY_BASE_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_runcmd PARENT_SCOPE)
