FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive

# Analysis-only image: same Qt6 dev headers as build.Dockerfile (clang-tidy
# and clazy need them to actually parse the code) plus the static-analysis
# tooling itself. Kept separate from build.Dockerfile so the real compile
# image doesn't carry clang/clazy/cppcheck weight it never needs — mirrors
# why dev.Dockerfile (the emulator) is its own image too.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    qt6-base-dev \
    qt6-declarative-dev \
    libssl-dev \
    clang \
    clang-format \
    clang-tidy \
    clazy \
    cppcheck \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /home/kiosk/kiosk
