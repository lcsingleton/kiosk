#!/usr/bin/env bash
# Build + run the kiosk dev container with the host's DRM/I2C/GPIO devices
# passed through. Run from a real Linux TTY/desktop session (needs /dev/dri).
set -euo pipefail

IMAGE=ha-tab-kiosk-dev
CONTAINER=ha-tab-kiosk-dev

echo "== host prep =="
# hid-mcp2221 usually autoloads on plug via udev/modalias; i2c-dev does not.
sudo modprobe i2c-dev
sudo modprobe hid-mcp2221 || true

echo "== building image =="
docker build -f docker/Dockerfile -t "$IMAGE" .

echo "== collecting host devices =="
DEVICE_ARGS=()

for d in /dev/dri/*; do
  # skip /dev/dri/by-path — a directory of symlinks, not a device node
  { [ -c "$d" ] || [ -b "$d" ]; } && DEVICE_ARGS+=(--device "$d")
done

for d in /dev/i2c-*; do
  [ -c "$d" ] && DEVICE_ARGS+=(--device "$d")
done

for d in /dev/gpiochip*; do
  [ -c "$d" ] && DEVICE_ARGS+=(--device "$d")
done

if [ -z "${DEVICE_ARGS+x}" ] || [ ${#DEVICE_ARGS[@]} -eq 0 ]; then
  echo "no /dev/dri, /dev/i2c-*, or /dev/gpiochip* found yet — is the MCP2221A plugged in?"
fi

# --group-add by GID (not name) so container membership matches whatever
# owns these device nodes on THIS host, without needing matching group names.
GROUP_ARGS=()
for g in video render plugdev dialout; do
  gid=$(getent group "$g" | cut -d: -f3 || true)
  [ -n "${gid:-}" ] && GROUP_ARGS+=(--group-add "$gid")
done

echo "== devices passed through =="
printf '%s\n' "${DEVICE_ARGS[@]:-}"

docker run --rm -it \
  --name "$CONTAINER" \
  -p 2222:22 \
  -v "$(pwd)/app:/home/kiosk/app" \
  "${DEVICE_ARGS[@]}" \
  "${GROUP_ARGS[@]}" \
  "$IMAGE"

# Convenience alternative if devices keep changing (hotplug churn) and you don't
# care about tight scoping since this is a local single-user dev box:
#   docker run --rm -it --privileged -v /dev:/dev -p 2222:22 "$IMAGE"
