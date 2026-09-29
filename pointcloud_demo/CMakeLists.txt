cmake_minimum_required(VERSION 3.16)
project(pointcloud_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
add_executable(pointcloud_demo
  src/main.cpp
  src/mainwindow.cpp
  src/mainwindow.h
  src/cloudview.cpp
  src/cloudview.h
  src/pointclouddata.cpp
  src/pointclouddata.h
)

target_include_directories(pointcloud_demo PRIVATE src)
target_link_libraries(pointcloud_demo PRIVATE
  Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(pointcloud_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
