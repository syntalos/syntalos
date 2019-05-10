
set(FIRMATAIO_SRC
    firmataiomodule.h
    firmataiomodule.cpp
)

add_library(mod_firmataio STATIC
    ${FIRMATAIO_SRC}
)

target_link_libraries(mod_firmataio
    Qt5::Widgets
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_firmataio PARENT_SCOPE)
