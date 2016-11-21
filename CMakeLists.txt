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
find_package(Qt5Widgets REQUIRED)
find_package(Qt5SerialPort REQUIRED)
find_package(Qt5Script REQUIRED)
find_package(Qt5ScriptTools REQUIRED)
find_package(Qt5OpenGL REQUIRED)

find_package(KF5Archive REQUIRED)
find_package(KF5TextEditor NO_MODULE REQUIRED)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
find_package(OpenGL REQUIRED)

add_subdirectory(src)
