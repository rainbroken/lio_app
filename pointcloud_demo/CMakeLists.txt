cmake_minimum_required(VERSION 3.16)
project(pointcloud_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Concurrent)
find_package(Eigen3 REQUIRED)
find_path(NANOFLANN_INCLUDE_DIR nanoflann.hpp)
if(NOT NANOFLANN_INCLUDE_DIR)
  message(FATAL_ERROR "nanoflann.hpp is required (install libnanoflann-dev)")
endif()
add_executable(pointcloud_demo
  src/main.cpp
  src/mainwindow.cpp
  src/mainwindow.h
  src/cloudview.cpp
  src/cloudview.h
  src/transformdialog.cpp
  src/transformdialog.h
  src/pointclouddata.cpp
  src/pointclouddata.h
  src/pointcloudprocessing.cpp
  src/pointcloudprocessing.h
)

target_include_directories(pointcloud_demo PRIVATE src ${NANOFLANN_INCLUDE_DIR})
target_link_libraries(pointcloud_demo PRIVATE
  Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent Eigen3::Eigen
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(pointcloud_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(pointclouddata_test tests/pointclouddata_test.cpp src/pointclouddata.cpp)
  target_include_directories(pointclouddata_test PRIVATE src)
  target_link_libraries(pointclouddata_test PRIVATE Qt6::Widgets)
  add_test(NAME pointclouddata_roundtrip COMMAND pointclouddata_test)
  add_executable(processing_history_test tests/processing_history_test.cpp
    src/mainwindow.cpp src/mainwindow.h src/cloudview.cpp src/cloudview.h
    src/transformdialog.cpp src/transformdialog.h src/pointclouddata.cpp src/pointclouddata.h
    src/pointcloudprocessing.cpp src/pointcloudprocessing.h)
  target_include_directories(processing_history_test PRIVATE src ${NANOFLANN_INCLUDE_DIR})
  target_link_libraries(processing_history_test PRIVATE Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent Eigen3::Eigen)
  add_test(NAME processing_history COMMAND processing_history_test)
  set_tests_properties(processing_history PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  add_executable(pointcloudprocessing_test tests/pointcloudprocessing_test.cpp src/pointcloudprocessing.cpp src/pointclouddata.cpp)
  target_include_directories(pointcloudprocessing_test PRIVATE src ${NANOFLANN_INCLUDE_DIR})
  target_link_libraries(pointcloudprocessing_test PRIVATE Qt6::Widgets Eigen3::Eigen)
  add_test(NAME pointcloud_processing COMMAND pointcloudprocessing_test)
endif()
