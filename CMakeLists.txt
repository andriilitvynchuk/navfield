cmake_minimum_required(VERSION 3.20)
project(navfield VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(FetchContent)

# Dependencies:
# - argparse       header-only CLI parsing     (fetched automatically)
# - quill          structured async logging    (system: brew install quill)
# - nlohmann_json  JSON config loading         (fetched automatically)
# - googletest     unit tests                  (fetched if BUILD_TESTS=ON)

# ---- argparse ----
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse
  GIT_TAG        v3.1
)
FetchContent_MakeAvailable(argparse)

# ---- quill (brew install quill) ----
find_package(quill REQUIRED)

# ---- nlohmann/json ----
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ---- Main executable ----
add_executable(navfield src/main.cpp)
target_include_directories(navfield PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(navfield PRIVATE
  argparse
  quill::quill
  nlohmann_json::nlohmann_json
)
target_compile_options(navfield PRIVATE -Wall -Wextra -Wpedantic)

# ---- Tests (cmake -B build -DBUILD_TESTS=OFF to skip) ----
option(BUILD_TESTS "Build test targets" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
