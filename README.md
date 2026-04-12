# navfield

OAK-D Lite sensor integration for NavBrain visual-inertial navigation.
Progresses from raw camera visualization to calibration, dataset collection,
and real-time SLAM. See `PLAN.md` for the full roadmap.

---

## Requirements

- macOS (dev) or Ubuntu/Jetson (deployment)
- CMake ≥ 3.20, Ninja
- Xcode Command Line Tools (macOS)

---

## 1. System Dependencies

### macOS

```bash
brew install quill opencv ninja cmake curl
```

### depthai-core (one-time build & install)

depthai-core does not support `add_subdirectory` / FetchContent — must be
built and installed system-wide once. CMake then finds it via `find_package`.

depthai v3 manages its own vcpkg internally. Do **not** pass a system vcpkg
toolchain — mixing two vcpkg instances causes missing package errors.

```bash
git clone --recurse-submodules https://github.com/luxonis/depthai-core.git -b v3.5.0
cd depthai-core
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DVCPKG_BUILD_TYPE=release

```

`-DVCPKG_BUILD_TYPE=release` skips debug variants for all vcpkg deps,
cutting build time roughly in half.

### Ubuntu / Jetson

```bash
sudo apt install libopencv-dev ninja-build cmake libcurl4-openssl-dev
# quill: build from source or via vcpkg
# depthai-core: same git clone + install steps as above
```

---

## 2. Build

```bash
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

To skip tests:

```bash
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=OFF
```

---

## 3. compile_commands.json (IDE / clangd)

```bash
ln -s build/compile_commands.json compile_commands.json
```

---

## 4. Run

All tools read from `config/camera.json`. Edit it to change FPS or resolution
before running.

```bash
# RGB stream viewer
./build/view_rgb --config config/camera.json

# Stereo (left | right) viewer
./build/view_stereo --config config/camera.json
```

Press `q` to quit any viewer.

### Camera config (`config/camera.json`)

| Key                 | Default        | Options                                               |
| ------------------- | -------------- | ----------------------------------------------------- |
| `fps`               | `30`           | 1–60                                                  |
| `stereo_resolution` | `"THE_400_P"`  | `THE_400_P` / `THE_480_P` / `THE_720_P` / `THE_800_P` |
| `rgb_resolution`    | `"THE_1080_P"` | `THE_1080_P` / `THE_4_K` / `THE_12_MP`                |
| `show_fps`          | `true`         | `true` / `false`                                      |

---

## 5. Tests

```bash
ctest --test-dir build
```

---

## Project Layout

```
navfield/
  config/
    camera.json          shared camera config (fps, resolution)
  src/
    camera_config.hpp    config loader (shared header)
    view_rgb.cpp         RGB stream viewer
    view_stereo.cpp      stereo stream viewer
    main.cpp             navfield CLI binary
  tests/
  PLAN.md                full 4-phase roadmap
  README.md              this file
```

---

## Roadmap

See `PLAN.md` for details on all phases:

- **Phase 1** — Visualization (done)
- **Phase 2** — Calibration & ROS bag
- **Phase 3** — Dataset collection (EuRoC format)
- **Phase 4** — Real-time SLAM (VINS-Fusion on Jetson)
