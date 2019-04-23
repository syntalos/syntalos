project(MazeAmaze)

cmake_minimum_required(VERSION 3.2)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

#
# Compiler flags
#
add_definitions("-Wall" "-W" "-Wextra" "-Werror")

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
include(GNUInstallDirs)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5SerialPort REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5Multimedia REQUIRED)
find_package(Qt5MultimediaWidgets REQUIRED)
find_package(Qt5Charts REQUIRED)

find_package(PythonLibs 3.5 REQUIRED)
find_package(KF5Archive REQUIRED)
find_package(KF5TextEditor NO_MODULE REQUIRED)

find_package(KF5DBusAddons REQUIRED)

find_package(OpenCV REQUIRED)
find_package(OpenGL REQUIRED)

#
# Compile options
#
option(UEYE_CAMERA "Use uEye camera instead of generic driver")

#
# Config file
#
if (UEYE_CAMERA)
    set(USE_UEYE_CAMERA ON)
endif ()
set(OKLIB_FULL_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/mazeamaze/libokFrontPanel.so")
configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h)
include_directories(${CMAKE_BINARY_DIR})

#
# Subdirectories
#
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(contrib)
