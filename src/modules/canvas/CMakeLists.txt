
set(CANVASMOD_SRC
    canvasmodule.h
    canvasmodule.cpp
    imageviewwidget.h
    imageviewwidget.cpp
    canvaswindow.h
    canvaswindow.cpp
)

add_library(mod_canvas STATIC
    ${CANVASMOD_SRC}
)

target_link_libraries(mod_canvas
    ${SY_BASE_LIBS}
    ${OpenCV_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_canvas PARENT_SCOPE)
