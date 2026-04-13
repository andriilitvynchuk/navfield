# depthai_bootstrap.cmake
#
# Workaround for depthai's broken CMake install: depthaiConfig.cmake does not
# call find_dependency() for any of its transitive deps, so every consumer must
# pre-find them all before find_package(depthai).  All the noise below belongs
# in upstream depthai — track luxonis/depthai-core CMake install improvements
# and remove this file when it's fixed.
#
# Requires CMAKE_PREFIX_PATH to contain the vcpkg install root.
# Use the CMakePresets.json presets so you never have to pass it manually.

# --- System-installed alongside depthai (/usr/local) -----------------------
find_package(fmt         CONFIG REQUIRED)
find_package(protobuf    CONFIG REQUIRED)
find_package(spdlog      CONFIG REQUIRED)
find_package(websocketpp CONFIG REQUIRED)
find_package(OpenSSL           REQUIRED)
find_package(ZLIB              REQUIRED)
find_package(BZip2             REQUIRED)
find_package(Threads           REQUIRED)

# --- vcpkg-installed (depthai-core/build/vcpkg_installed/x64-osx) ----------
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

# --- Targets depthaiTargets.cmake references but depthaiConfig.cmake omits --
# Guards prevent "target already exists" errors if cmake ever runs this twice or
# if the vcpkg usb-1.0Config.cmake was already included by a dependency.

find_library(DYNAMIC_CALIBRATION_LIB dynamic_calibration REQUIRED)
if(NOT TARGET dynamic_calibration_imported)
  add_library(dynamic_calibration_imported SHARED IMPORTED)
  set_target_properties(dynamic_calibration_imported PROPERTIES
    IMPORTED_LOCATION "${DYNAMIC_CALIBRATION_LIB}")
endif()

find_library(USB_LIB usb-1.0 REQUIRED)
if(NOT TARGET usb-1.0)
  add_library(usb-1.0 SHARED IMPORTED)
  set_target_properties(usb-1.0 PROPERTIES
    IMPORTED_LOCATION "${USB_LIB}")
endif()

find_package(depthai CONFIG REQUIRED)
