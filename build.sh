#!/usr/bin/env bash
# Compile the repo in the build container and stage install output into
# ./dist. ./dist mirrors the real FHS layout (see the etc/ install() rules
# added to each module's CMakeLists.txt) via `DESTDIR=dist cmake --install`,
# so it's exactly what a future dpkg-buildpackage-style .deb would stage
# too — this script just isn't that yet.
#
# ./build (the CMake build tree) and ./dist are both bind-mounted from the
# repo root, so re-running this script after a source edit is an
# incremental rebuild, not a from-scratch one. Follow with ./run.sh (or, if
# the emulator container is already up, `docker exec ha-tab-kiosk-dev
# systemctl restart kiosk ha-kiosk-google-calendar-sync ha-kiosk-weather-sync`
# to pick up the new binaries without restarting the container).
set -euo pipefail

IMAGE=ha-tab-kiosk-build

echo "== building image =="
docker build -f docker/build.Dockerfile -t "$IMAGE" .

echo "== configuring + building =="
docker run --rm \
  -v "$(pwd):/home/kiosk/kiosk" \
  "$IMAGE" \
  bash -c "cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j\$(nproc)"

echo "== staging install (DESTDIR=dist) =="
docker run --rm \
  -v "$(pwd):/home/kiosk/kiosk" \
  "$IMAGE" \
  bash -c "rm -rf dist && DESTDIR=dist cmake --install build"

echo "== staged to ./dist =="
find dist -type f | sort
