# ros:humble is the official pre-built image (Ubuntu 22.04 + ROS2 Humble).
# Using it as the base eliminates a slow, fragile manual ROS install step.
FROM ros:humble

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=UTC \
    LANG=C.UTF-8

# ── Layer 1: Build toolchain + system deps ────────────────────────────────
# Stable; changes only when a new system dep is added.
# cmake 3.22 ships with ros:humble but vcpkg (bundled in depthai-core) requires
# 3.31+. We add Kitware's apt repo to get a current cmake so vcpkg uses the
# system binary instead of trying to download one from GitHub (which hits 504s
# in restricted build environments).
RUN echo 'Acquire::Check-Date "false";' > /etc/apt/apt.conf.d/99no-check-date \
    && apt-get update && apt-get install -y --no-install-recommends \
    gpg wget ca-certificates \
    && wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc \
         | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg \
    && echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' \
         > /etc/apt/sources.list.d/kitware.list \
    && apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    wget \
    ca-certificates \
    pkg-config \
    # Needed by vcpkg bootstrap
    zip \
    unzip \
    tar \
    # Python (vcpkg uses it for some ports)
    python3 \
    python3-pip \
    # OAK camera USB access
    libusb-1.0-0-dev \
    udev \
    # OpenCV
    libopencv-dev \
    # SSL / compression (depthai transitive deps)
    libssl-dev \
    zlib1g-dev \
    libbz2-dev \
    # nlohmann/json (header-only, used by navfield main.cpp)
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# ── Layer 2: quill v11.1.0 ────────────────────────────────────────────────
# Not available in Ubuntu 22.04 apt repos. quill is header-only so this
# cmake install just copies headers — completes in ~2 s, not worth skipping.
RUN git clone --depth 1 --branch v11.1.0 \
      https://github.com/odygrd/quill.git /tmp/quill \
    && cmake -S /tmp/quill -B /tmp/quill/build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DQUILL_BUILD_EXAMPLES=OFF \
        -DQUILL_BUILD_TESTS=OFF \
    && cmake --build /tmp/quill/build -j"$(nproc)" \
    && cmake --install /tmp/quill/build \
    && rm -rf /tmp/quill

# ── Layer 3: depthai-core v3.5.0 — clone only ────────────────────────────
# Separating clone from build: a changed cmake flag does not re-clone.
RUN git clone --depth 1 --branch v3.5.0 \
      --recurse-submodules --shallow-submodules \
      https://github.com/luxonis/depthai-core.git /opt/depthai-core

# ── Layer 4a: depthai-core — cmake configure (vcpkg bootstrap + downloads) ──
# THE slow step: vcpkg bootstraps itself then downloads and compiles ~20
# packages from source. Isolated here so an OOM during compilation (Layer 4b)
# does NOT bust this cache — you re-run only the compile, not the downloads.
RUN cmake -S /opt/depthai-core -B /opt/depthai-core/build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DDEPTHAI_BUILD_EXAMPLES=OFF \
        -DDEPTHAI_BUILD_TESTS=OFF \
        -DDEPTHAI_OPENCV_SUPPORT=ON \
        -DVCPKG_TARGET_TRIPLET=x64-linux \
        "-DVCPKG_INSTALL_OPTIONS=--clean-buildtrees-after-build;--clean-packages-after-build"

# ── Layer 4b: depthai-core — compile ─────────────────────────────────────────
# Capped at 4 parallel jobs to avoid OOM on Docker Desktop (default 2 GB VM).
# Increase the cap or raise Docker Desktop memory (Settings > Resources) if slow.
RUN cmake --build /opt/depthai-core/build -j"$(( $(nproc) > 4 ? 4 : $(nproc) ))"

# ── Layer 4c: depthai-core — install ─────────────────────────────────────────
RUN cmake --install /opt/depthai-core/build --prefix /usr/local \
    # dynamic_calibration is a pre-built FetchContent binary — NOT installed
    # by cmake --install. Copy it so find_library() can locate it later.
    && find /opt/depthai-core/build -name "libdynamic_calibration.so*" \
         -exec cp {} /usr/local/lib/ \; \
    # Make vcpkg's shared libs (libusb-1.0.so) discoverable at runtime.
    && echo "/opt/depthai-core/build/vcpkg_installed/x64-linux/lib" \
         > /etc/ld.so.conf.d/depthai-vcpkg.conf \
    && ldconfig

# ── Layer 5: navfield source ──────────────────────────────────────────────
# This layer — and only this layer — is invalidated on every code change.
# Everything above stays cached, so a code change → rebuild takes seconds.
WORKDIR /workspace
COPY . .

# ── Layer 6: navfield build ───────────────────────────────────────────────
RUN . /opt/ros/humble/setup.sh \
    && cmake --preset docker \
    && cmake --build build -j"$(nproc)"

CMD ["/workspace/build/view_rgb"]
