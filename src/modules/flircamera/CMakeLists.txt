
find_path(SPINNAKER_INCLUDE_DIRS
    NAMES Spinnaker.h
    PATH_SUFFIXES spinnaker
)
if(NOT SPINNAKER_INCLUDE_DIRS)
    message(FATAL_ERROR "Unable to find Spinnaker SDK include directories! Please install the SDK or disable FLIR camera support.")
endif()

find_library(SPINNAKER_LIBRARY
    NAMES Spinnaker
)
if(NOT SPINNAKER_LIBRARY)
    message(FATAL_ERROR "Unable to find Spinnaker SDK library! Please install the SDK or disable FLIR camera support.")
endif()

set(FLIR_CAMERA_SRC
    flircameramod.h
    flircameramod.cpp
    flircamera.h
    flircamera.cpp
    flircamsettingsdialog.h
    flircamsettingsdialog.cpp
)

set(FLIR_CAMERA_UI
    flircamsettingsdialog.ui
)

qt5_wrap_ui(FLIR_CAMERA_UI_SRC ${FLIR_CAMERA_UI})

add_library(mod_flircamera STATIC
    ${FLIR_CAMERA_SRC}
    ${FLIR_CAMERA_UI_SRC}
)

target_link_libraries(mod_flircamera
    ${SY_BASE_LIBS}
    ${SPINNAKER_LIBRARY}
    ${OpenCV_LIBS}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)
include_directories(SYSTEM
    ${SPINNAKER_INCLUDE_DIRS}
)

set(MA_EXTRA_LIBS ${MA_EXTRA_LIBS} ${SPINNAKER_LIBRARY} PARENT_SCOPE)
set(MA_MODULE_LIBS ${MA_MODULE_LIBS} mod_flircamera PARENT_SCOPE)
