cmake_minimum_required(VERSION 3.2)
project(Syntalos
        VERSION 1.0
)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/contrib/cmake/")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

#
# Options
#
option(MAINTAINER "Enable maintainer mode" OFF)
option(MOD_CAMERA_UEYE "Enable uEye camera module" OFF)
option(MOD_CAMERA_FLIR "Enable FLIR camera module using the Spinnaker API" OFF)
option(MOD_MINISCOPE   "Enable Miniscope module" ON)


#
# Compiler flags
#
add_definitions("-Wall" "-Wextra")
if (MAINTAINER)
    add_definitions("-Werror")
    add_definitions("-Wzero-as-null-pointer-constant")
    add_definitions(
        "-Wsuggest-override"
        "-Wsuggest-final-methods"
        # "-Wsuggest-final-types"
    )
endif()

# we can't link with -rdynamic, because otherwise dlopen'ing the proprietary Opal library fails
if (";${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS};" MATCHES ";-rdynamic;")
    add_definitions("-g")
endif()
string(REPLACE "-rdynamic" "" CMAKE_SHARED_LIBRARY_LINK_C_FLAGS ${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS})
string(REPLACE "-rdynamic" "" CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS ${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS})


#
# Dependencies
#
set(OpenGL_GL_PREFERENCE GLVND)
include(GNUInstallDirs)
find_package(PkgConfig)
find_package(Threads)
find_library(RT_LIB rt)
find_package(Qt5Core REQUIRED)
find_package(Qt5Test REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5SerialPort REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5Charts REQUIRED)
find_package(Qt5Svg REQUIRED)
find_package(Qt5RemoteObjects)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(FFmpeg 58.35 REQUIRED COMPONENTS avcodec avutil avformat swscale)
pkg_check_modules(GLIB glib-2.0>=2.58 REQUIRED)
pkg_check_modules(TOML tomlplusplus>=1.0 REQUIRED)

find_package(PythonLibs 3.5 REQUIRED)
find_package(KF5Archive REQUIRED)
find_package(KF5TextEditor NO_MODULE REQUIRED)

find_package(KF5DBusAddons REQUIRED)

find_package(OpenCV REQUIRED)
find_package(OpenGL REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

# essential dependencies definitely required by
# anything using the Syntalos API
# NOTE: QtWidgets is required since modules expose QAction items
set(SY_BASE_LIBS ${CMAKE_THREAD_LIBS_INIT}
                 Qt5::Core
                 Qt5::Widgets
                 Eigen3::Eigen
)

# ensure our Qt version is sufficient
if (Qt5Core_VERSION VERSION_LESS 5.12)
    message(FATAL_ERROR "We need at least Qt 5.12 to build." ${Qt5_DIR} ${QT_QMAKE_EXECUTABLE})
endif()

#
# Config file
#
if(MOD_CAMERA_TIS)
    set(HAVE_TIS_CAMERA ON)
endif()
if(MOD_CAMERA_FLIR)
    set(HAVE_FLIR_CAMERA ON)
endif()
if(MOD_CAMERA_UEYE)
    set(HAVE_UEYE_CAMERA ON)
endif()
if(MOD_MINISCOPE)
    set(HAVE_MINISCOPE ON)
endif()

set(MA_ARCH_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/syntalos")
set(OKLIB_FULL_PATH "${MA_ARCH_INSTALL_DIR}/libokFrontPanel.so")
configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h)
include_directories(${CMAKE_BINARY_DIR})


#
# Subdirectories
#
add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(data)
add_subdirectory(tests)
add_subdirectory(contrib)
