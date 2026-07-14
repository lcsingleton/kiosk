FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive

# Build-only image: compiles the repo and stages install output via DESTDIR
# (see build.sh). No sshd, no runtime QML modules, no hardware CLI tools —
# those belong to dev.Dockerfile (the emulator), which runs the result.
# Only what's actually #include'd/linked today (see root CMakeLists.txt's
# find_package calls) — add libi2c-dev/libgpiod-dev/libusb-1.0-0-dev/
# libhidapi-dev here once the C++ side actually links against them.
# libgstreamer-plugins-base1.0-dev provides the gstreamer-app-1.0 pkg-config
# module ha-kiosk-presence-sync's CMakeLists.txt looks up for camera capture.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    qt6-base-dev \
    qt6-declarative-dev \
    libssl-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /home/kiosk/kiosk
