project(MazeAmaze)

cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/contrib/cmake/")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

#
# Options
#
option(MAINTAINER "Enable maintainer mode" OFF)


#
# Compiler flags
#
add_definitions("-Wall" "-Wextra")
if (MAINTAINER)
    add_definitions("-Werror")
endif()

add_definitions("-Wno-unused-but-set-variable")

# we can't link with -rdynamic, because otherwise dlopen'ing the proprietary Opal library fails
if (";${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS};" MATCHES ";-rdynamic;")
    add_definitions("-g")
endif()
string( REPLACE "-rdynamic" "" CMAKE_SHARED_LIBRARY_LINK_C_FLAGS ${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS})
string( REPLACE "-rdynamic" "" CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS ${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS})


#
# Dependencies
#
set(OpenGL_GL_PREFERENCE GLVND)
include(GNUInstallDirs)
find_package(Threads)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5SerialPort REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5Charts REQUIRED)
find_package(Boost 1.60 REQUIRED COMPONENTS container)
find_package(FFmpeg 58.35 REQUIRED COMPONENTS avcodec avutil avformat swscale)

find_package(PythonLibs 3.5 REQUIRED)
find_package(KF5Archive REQUIRED)
find_package(KF5TextEditor NO_MODULE REQUIRED)

find_package(KF5DBusAddons REQUIRED)

find_package(OpenCV REQUIRED)
find_package(OpenGL REQUIRED)


#
# Compile options
#
option(MOD_CAMERA_UEYE "Enable uEye camera module" ON)


if(MOD_CAMERA_UEYE)
    set(UYEYE_LIBS "ueye_api")
endif()

#
# Config file
#
if(MOD_CAMERA_UEYE)
    set(HAVE_UEYE_CAMERA ON)
endif()
set(OKLIB_FULL_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/mazeamaze/libokFrontPanel.so")
configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h)
include_directories(${CMAKE_BINARY_DIR})


#
# Subdirectories
#
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(contrib)
