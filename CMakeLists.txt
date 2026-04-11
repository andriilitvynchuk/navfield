cmake_minimum_required(VERSION 3.20)

# Dependencies:
# - argparse     header-only CLI parsing  (fetched automatically)
# - quill        async logging            (system: brew install quill)
# - depthai      OAK-D Lite C++ SDK      (system install — see README)
# - OpenCV       image display            (system: brew install opencv)
# - googletest   unit tests               (fetched if BUILD_TESTS=ON)

include(FetchContent)

# ---------------------------------------------------------------
project(navfield VERSION 1.0.0 LANGUAGES CXX)
# ---------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- depthai transitive deps ----
# depthaiTargets.cmake embeds these as INTERFACE_LINK_LIBRARIES but does NOT
# call find_dependency for them — must all be pre-found before find_package(depthai).
#
# System (/usr/local) — installed alongside depthai:
find_package(fmt         CONFIG REQUIRED)
find_package(protobuf    CONFIG REQUIRED)
find_package(spdlog      CONFIG REQUIRED)
find_package(websocketpp CONFIG REQUIRED)
find_package(OpenSSL           REQUIRED)
find_package(ZLIB              REQUIRED)
find_package(BZip2             REQUIRED)
find_package(Threads           REQUIRED)
# vcpkg-only — needs CMAKE_PREFIX_PATH=~/navbrain/depthai-core/build/vcpkg_installed/x64-osx
find_package(CURL        CONFIG REQUIRED)
find_package(Eigen3      CONFIG REQUIRED)
find_package(LibArchive        REQUIRED)
find_package(cpr         CONFIG REQUIRED)
find_package(lz4         CONFIG REQUIRED)
find_package(liblzma     CONFIG REQUIRED)
find_package(httplib     CONFIG REQUIRED)
find_package(semver      CONFIG REQUIRED)
find_package(Backward    CONFIG REQUIRED)
find_package(magic_enum  CONFIG REQUIRED)
find_package(mp4v2       CONFIG REQUIRED)
find_package(apriltag    CONFIG REQUIRED)
find_package(yaml-cpp    CONFIG REQUIRED)

# ---- Stubs for transitive deps that the installed depthai config omits ----
# dynamic_calibration: referenced in depthaiTargets.cmake but not created by depthaiConfig.cmake
find_library(DYNAMIC_CALIBRATION_LIB dynamic_calibration REQUIRED)
add_library(dynamic_calibration_imported SHARED IMPORTED)
set_target_properties(dynamic_calibration_imported PROPERTIES
  IMPORTED_LOCATION "${DYNAMIC_CALIBRATION_LIB}"
)
# usb-1.0: XLink target uses it as a bare name; newer macOS linker requires an explicit path
find_library(USB_LIB usb-1.0 REQUIRED)
add_library(usb-1.0 SHARED IMPORTED)
set_target_properties(usb-1.0 PROPERTIES
  IMPORTED_LOCATION "${USB_LIB}"
)

# ---- depthai-core ----
find_package(depthai CONFIG REQUIRED)

# ---- argparse ----
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse
  GIT_TAG        v3.1
)
FetchContent_MakeAvailable(argparse)

# ---- quill (brew install quill) ----
find_package(quill REQUIRED)

# ---- OpenCV (brew install opencv) ----
find_package(OpenCV REQUIRED)

# ---- Shared camera deps ----
set(CAMERA_DEPS
  argparse
  quill::quill
  depthai::core
  ${OpenCV_LIBS}
)

# ---- Main executable ----
add_executable(navfield src/main.cpp)
target_include_directories(navfield PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(navfield PRIVATE
  argparse
  quill::quill
  depthai::core
)
target_compile_options(navfield PRIVATE -Wall -Wextra -Wpedantic)

# ---- view_rgb ----
add_executable(view_rgb src/view_rgb.cpp)
target_include_directories(view_rgb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(view_rgb PRIVATE ${CAMERA_DEPS})
target_compile_options(view_rgb PRIVATE -Wall -Wextra -Wpedantic)

# ---- view_stereo ----
add_executable(view_stereo src/view_stereo.cpp)
target_include_directories(view_stereo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(view_stereo PRIVATE ${CAMERA_DEPS})
target_compile_options(view_stereo PRIVATE -Wall -Wextra -Wpedantic)

# ---- Tests (cmake -B build -DBUILD_TESTS=OFF to skip) ----
option(BUILD_TESTS "Build test targets" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
