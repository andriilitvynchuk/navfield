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

## Fixed: `view_rgb` / `view_stereo` — SEGV after ~59s, no log output

Two separate bugs, both fixed.

---

### Bug A: `createOutputQueue()` called after `pipeline.start()`

Both files called `createOutputQueue()` **after** `pipeline.start()`. In depthai,
`pipeline.start()` compiles the pipeline graph and sends it to the device — any queue registered
after that point is never wired in, the ring-buffer is never initialised, and the first
`tryGet()` dereferences a null/garbage pointer → SEGV.

The ~59s delay was the symptom when no device was attached: `pipeline.start()` blocks ~60s,
then throws; the uncaught exception unwinds through depthai + quill before quill's async backend
can flush → "no log output at all".

```cpp
// WRONG (was)
pipeline.start();
auto queue = out->createOutputQueue(4, false);   // too late — graph already compiled

// CORRECT (now)
auto queue = out->createOutputQueue(4, false);   // registered before graph compile
pipeline.start();
```

---

### Bug B: `quill::BackendOptions::check_backend_singleton_instance` crash with depthai

**Symptom:** with depthai linked, `quill::Backend::start()` crashes with
`SIGSEGV / KERN_INVALID_ADDRESS at 0x3` **before** the first `fprintf` in `main()`.
Without depthai (e.g. `navfield` / `main.cpp` which only links `argparse + quill`), no crash.

**Root cause:** `quill::BackendOptions::check_backend_singleton_instance` (default `true`)
creates a POSIX named semaphore via `BackendWorkerLock`. When `libdepthai-core.dylib` is loaded,
its static initializers run before `main()` — depthai starts spdlog's async thread pool and the
dcl scheduler. This corrupts the POSIX semaphore state on macOS (Darwin 25 / macOS 26), causing
`sem_open` to return an invalid pointer (`(sem_t*)3` instead of a valid heap address).
`sem_trywait((sem_t*)3)` then dereferences address `0x3` → SIGSEGV.

Confirmed via macOS crash report (`~/Library/Logs/DiagnosticReports/`):
```
KERN_INVALID_ADDRESS at 0x0000000000000003
pthread_sem_timed_or_blocked_wait + 15
quill::v11::detail::BackendWorkerLock::BackendWorkerLock()
quill::v11::Backend::start()
main
```

**Fix:** disable the singleton check (it only guards against a misconfigured multi-module build,
not relevant here):

```cpp
quill::BackendOptions backend_opts;
backend_opts.check_backend_singleton_instance = false;
quill::Backend::start(backend_opts);
```

The `BackendOptions` doc explicitly notes: *"In rare cases, this mechanism may interfere with
certain environments. If necessary, this check can be disabled by setting this option to false."*

---

### Files changed
- `src/view_rgb.cpp` — Bug A fix + Bug B fix
- `src/view_stereo.cpp` — Bug A fix + Bug B fix

### Build command
```bash
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=~/navbrain/depthai-core/build/vcpkg_installed/x64-osx
cmake --build build
```
