# NavField — OAK-D Lite Integration Plan

## Goal
Turn OAK-D Lite (stereo + RGB + IMU) into a full real-time SLAM sensor stack,
progressing from raw visualization to calibrated dataset collection to live SLAM.

---

## Phase 1 — Visualization (CURRENT)

**Executables** (C++, depthai-core):

| Script | What it does |
|---|---|
| `src/view_rgb.cpp` | Single RGB stream viewer with FPS overlay |
| `src/view_stereo.cpp` | Side-by-side left + right grayscale viewer |

**Shared config** — `config/camera.json`:
- `fps` — capture frequency (default 30)
- `resolution` — `THE_1080_P` / `THE_720_P` / `THE_400_P`
- `show_fps` — overlay frame counter

**Deliverable**: both scripts read from the same `config/camera.yaml`, display
OpenCV windows, quit on `q`.

---

## Phase 2 — Calibration & ROS Bag

**Executables** (C++):

| Source | What it does |
|---|---|
| `src/collect_rosbag.cpp` | Record stereo + IMU to ROS 2 bag (rclcpp + rosbag2) |
| `src/calibrate_intrinsics.cpp` | Checkerboard intrinsic calibration (OpenCV) |
| `src/calibrate_stereo.cpp` | Stereo extrinsic calibration (OpenCV stereoCalibrate) |
| `src/calibrate_imu_cam.cpp` | IMU–camera extrinsic calibration helper (Kalibr-compatible output) |

**Deliverables**:
- `calibration/intrinsics_left.yaml`, `intrinsics_right.yaml`, `intrinsics_rgb.yaml`
- `calibration/stereo_extrinsics.yaml`
- `calibration/imu_cam_extrinsics.yaml` (T_cam_imu in SE3)

---

## Phase 3 — Dataset Collection (EuRoC Format)

**Executables** (C++):

| Source | What it does |
|---|---|
| `src/collect_dataset.cpp` | Synchronized stereo + IMU → EuRoC directory layout |
| `src/verify_dataset.cpp` | Sanity-check timestamps, frame drops, IMU rate |

**EuRoC layout**:
```
dataset/
  mav0/
    cam0/data/   ← left grayscale PNGs
    cam0/data.csv
    cam1/data/   ← right grayscale PNGs
    cam1/data.csv
    imu0/data.csv
    sensor.yaml  ← calibration embedded
```

**Deliverable**: `.csv` + PNGs loadable directly by VINS-Fusion / ORB-SLAM3.

---

## Phase 4 — Real-Time SLAM

**Target**: VINS-Fusion (preferred) running live on Jetson Orin with OAK-D Lite.

**Steps**:
1. Write `config/vins_fusion_oakd.yaml` (camera + IMU params from Phase 2 calibration)
2. Write `src/oakd_vins_bridge.cpp` — publishes stereo + IMU as ROS 2 topics
3. Launch VINS-Fusion node against live OAK-D feed
4. (Optional) Add ORB-SLAM3 as alternative backend

**Deliverable**: live pose output at ≥10 Hz on Jetson Orin.

---

## File Layout

```
navfield/
  config/
    camera.json              ← Phase 1 shared camera config
    vins_fusion_oakd.yaml    ← Phase 4
  src/
    camera_config.hpp        ← Phase 1 shared config loader
    view_rgb.cpp             ← Phase 1
    view_stereo.cpp          ← Phase 1
    collect_rosbag.cpp       ← Phase 2
    calibrate_intrinsics.cpp ← Phase 2
    calibrate_stereo.cpp     ← Phase 2
    calibrate_imu_cam.cpp    ← Phase 2
    collect_dataset.cpp      ← Phase 3
    verify_dataset.cpp       ← Phase 3
    oakd_vins_bridge.cpp     ← Phase 4
    main.cpp                 ← existing navfield binary
  calibration/               ← Phase 2 outputs (gitignored raw data)
  tests/
```

---

## Dependencies

| Library | Install |
|---|---|
| depthai-core | fetched automatically by CMake |
| OpenCV | `brew install opencv` |
| rclcpp + rosbag2 | ROS 2 Humble/Iron on Jetson |
| Kalibr | Docker image (Phase 2 optional) |

---

## Progress

- [x] Phase 1 — Visualization
- [ ] Phase 2 — Calibration & ROS Bag
- [ ] Phase 3 — Dataset Collection
- [ ] Phase 4 — Real-Time SLAM
