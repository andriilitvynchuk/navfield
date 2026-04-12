# Build & Runtime Debugging Context

## Issues Fixed

### 1. `ld: library 'dynamic_calibration_imported' not found`
**Root cause:** The installed depthai cmake config (`/usr/local/lib/cmake/depthai/depthaiTargets.cmake`) references the CMake target `dynamic_calibration_imported` in its `INTERFACE_LINK_LIBRARIES`, but the installed `depthaiConfig.cmake` never defines that target. The depthai build-tree does define it in `depthaiDependencies.cmake`, but that file is not shipped in the install.

**Fix in `CMakeLists.txt`:** Manually define the IMPORTED target before `find_package(depthai)`:
```cmake
find_library(DYNAMIC_CALIBRATION_LIB dynamic_calibration REQUIRED)
add_library(dynamic_calibration_imported SHARED IMPORTED)
set_target_properties(dynamic_calibration_imported PROPERTIES
  IMPORTED_LOCATION "${DYNAMIC_CALIBRATION_LIB}"
)
```
Library lives at `/usr/local/lib/libdynamic_calibration.dylib`.

### 2. `ld: library 'usb-1.0' not found`
**Root cause:** XLink's cmake config emits `-lusb-1.0` as a bare flag. macOS 26.4's new linker no longer searches `/usr/local/lib` by default, so the flag fails. The library is at `/usr/local/lib/libusb-1.0.dylib` (also a copy at the vcpkg path).

**Fix in `CMakeLists.txt`:** Define an IMPORTED target for `usb-1.0` before `find_package(depthai)`:
```cmake
find_library(USB_LIB usb-1.0 REQUIRED)
add_library(usb-1.0 SHARED IMPORTED)
set_target_properties(usb-1.0 PROPERTIES
  IMPORTED_LOCATION "${USB_LIB}"
)
```

### 3. `Undefined symbols: cv::cvtColor / cv::cvtColorTwoPlane`
**Root cause:** `libdepthai-core.a` was compiled against OpenCV 4.1.x. OpenCV 4.13.0 (current Homebrew) added an `AlgorithmHint` parameter to `cvtColor` and `cvtColorTwoPlane`, changing their C++ mangled symbol names. The 4.1.x symbols no longer exist in 4.13.0's dylib.

Both versions are present on the system:
- `/usr/local/lib/libopencv_imgproc.4.1.0.dylib` — old symbols, no cmake config
- `/usr/local/lib/libopencv_imgproc.4.13.0.dylib` — new symbols, cmake config only for 4.13

**Fix:** Rebuild depthai-core against OpenCV 4.13:
```bash
cd ~/navbrain/depthai-core
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
sudo cmake --install build
```

### 4. `navfield` segfaults (SIGV, exit 139) after ~25s
**Root cause:** `navfield` was linking against `depthai::core` unnecessarily. `main.cpp` doesn't use depthai at all. depthai registers global state / destructors that crash at program shutdown when no device context was initialized.

**Fix in `CMakeLists.txt`:** Remove `depthai::core` from navfield's link libraries:
```cmake
target_link_libraries(navfield PRIVATE
  argparse
  quill::quill
)
```

---

## Current Open Problem: `view_rgb` — SEGV after ~59s, no log output

### Symptoms
- `./build/view_rgb --config config/camera.json` runs for ~59 seconds then SIGSEGV
- No log output appears at all, even after adding `logger->flush_log()` before all key steps
- `fprintf(stderr, ...)` diagnostics were added (latest build) to bypass quill entirely

### Hypothesis
- The 59s matches depthai's device discovery timeout — likely no OAK-D camera is connected, or the camera is in a bad state
- `pipeline.start()` blocks for ~60s waiting for device, then throws an exception
- The thrown exception (or subsequent stack unwind) crashes — possibly depthai's signal handlers conflicting with quill's signal handlers
- `quill::Backend::start()` registers signal handlers; depthai/spdlog may also register them, causing a double-registration crash

### Next diagnostic step
Run the latest `view_rgb` build — it now prints `[diag] ...` lines to stderr at each step using `fprintf(stderr)`/`fflush(stderr)`. The last `[diag]` line printed before the crash will identify exactly where it fails.

Expected output if working:
```
[diag] logger created
[diag] pipeline created
[diag] camera built
[diag] output requested, connecting to device...
[diag] device connected, starting loop
```

If it hangs/crashes at "connecting to device..." → device not found / depthai timeout.

### Files changed
- `CMakeLists.txt` — stubs for `dynamic_calibration_imported` and `usb-1.0`, removed `depthai::core` from navfield
- `src/view_rgb.cpp` — diagnostic fprintf lines, no_frame_count warning log in loop

### Build command
```bash
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=~/navbrain/depthai-core/build/vcpkg_installed/x64-osx
cmake --build build
```
