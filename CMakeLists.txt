cmake_minimum_required(VERSION 3.20)

include(FetchContent)

project(navfield VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# depthai's CMake install is missing find_dependency() calls — bootstrap handles it.
# See cmake/depthai_bootstrap.cmake for details and the upstream issue to track.
include(cmake/depthai_bootstrap.cmake)

find_package(quill   REQUIRED)
find_package(OpenCV  REQUIRED)

option(WITH_ROS2 "Enable ROS2 rclcpp integration" OFF)
if(WITH_ROS2)
  find_package(rclcpp REQUIRED)
endif()

FetchContent_Declare(argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse
  GIT_TAG        v3.1)
FetchContent_MakeAvailable(argparse)

# ---- navfield (CLI entry point, no camera) --------------------------------
add_executable(navfield src/main.cpp)
target_include_directories(navfield PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(navfield PRIVATE argparse quill::quill)
target_compile_options(navfield PRIVATE -Wall -Wextra -Wpedantic)
if(WITH_ROS2)
  target_compile_definitions(navfield PRIVATE WITH_ROS2)
  target_link_libraries(navfield PRIVATE rclcpp::rclcpp)
endif()

# ---- Camera viewer targets ------------------------------------------------
set(CAMERA_DEPS argparse quill::quill depthai::core ${OpenCV_LIBS})

add_executable(view_rgb src/view_rgb.cpp)
target_include_directories(view_rgb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(view_rgb PRIVATE ${CAMERA_DEPS})
target_compile_options(view_rgb PRIVATE -Wall -Wextra -Wpedantic)

add_executable(view_stereo src/view_stereo.cpp)
target_include_directories(view_stereo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(view_stereo PRIVATE ${CAMERA_DEPS})
target_compile_options(view_stereo PRIVATE -Wall -Wextra -Wpedantic)

# ---- Tests ----------------------------------------------------------------
option(BUILD_TESTS "Build test targets" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
