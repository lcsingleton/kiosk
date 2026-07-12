#!/usr/bin/env bash
# Build + run the kiosk emulator container: a systemd-based stand-in for the
# whole tablet deployment (deploy/README.md), not just a build shell. It
# runs the real kiosk.service/ha-kiosk-google-calendar-sync.service/
# ha-kiosk-weather-sync.service units against the host's DRM/I2C/GPIO
# devices, so starting this one container is meant to leave everything up
# and running the same way it would on the tablet.
#
# It does NOT build the app — run ./build.sh first (and after every source
# change; this container only ever reads ./dist, it never compiles). Run
# from a real Linux TTY/desktop session (needs /dev/dri).
set -euo pipefail

IMAGE=ha-tab-kiosk-dev
CONTAINER=ha-tab-kiosk-dev

if [ ! -d dist/opt/kiosk/bin ]; then
  echo "./dist not found — run ./build.sh first" >&2
  exit 1
fi

echo "== seeding /etc/kiosk from examples (first run only) =="
mkdir -p etc-kiosk
[ -f etc-kiosk/daemon-config.json ] || { cp dist/etc/kiosk/daemon-config.example.json etc-kiosk/daemon-config.json; echo "  wrote etc-kiosk/daemon-config.json — edit in serviceAccountKeyPath/calendars/people (see deploy/README.md step 6) or the calendar-sync daemon will just crash-loop harmlessly"; }
[ -f etc-kiosk/weather-config.json ] || { cp dist/etc/kiosk/weather-config.example.json etc-kiosk/weather-config.json; echo "  wrote etc-kiosk/weather-config.json — ships a working Sydney geohash by default"; }

echo "== host prep =="
# hid-mcp2221 usually autoloads on plug via udev/modalias; i2c-dev does not.
sudo modprobe i2c-dev
sudo modprobe hid-mcp2221 || true

echo "== building image =="
docker build -f docker/dev.Dockerfile -t "$IMAGE" .

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

docker rm -f "$CONTAINER" >/dev/null 2>&1 || true

# systemd-as-PID-1 needs to manage cgroups/services, but NOT full
# --privileged for that — --privileged blanket-exposes the host's entire
# /dev (including /dev/input/*, which the explicit --device list below
# deliberately excludes) and disables the device cgroup entirely. SYS_ADMIN
# (cgroup management) is the narrow capability systemd actually needs — same
# "local single-user dev box only, never network-exposed" trust tradeoff as
# before, just no longer handing over every host device to get it. (Note:
# this was tried as a fix for a windowed-preview input/repaint bug during
# this container's refactor and didn't move the needle either way — the
# actual cause was eglfs_x11 itself, see "Windowed preview" in README.md.
# Kept anyway as a real, independent least-privilege improvement.)
# Debian's base image has no timezone configured (system clock reads UTC),
# so without these two bind-mounts every local-time computation in the app
# (new-event creation, the "now" line, the clock) runs in GMT regardless of
# the host's real zone — match the host's clock instead of guessing a zone.
docker run -d \
  --name "$CONTAINER" \
  --cap-add SYS_ADMIN \
  --security-opt seccomp=unconfined \
  --cgroupns=host \
  -v /sys/fs/cgroup:/sys/fs/cgroup:rw \
  --tmpfs /run \
  --tmpfs /run/lock \
  -p 2222:22 \
  -v "$(pwd)/dist/opt/kiosk:/opt/kiosk:ro" \
  -v "$(pwd)/etc-kiosk:/etc/kiosk" \
  -v /etc/localtime:/etc/localtime:ro \
  -v /etc/timezone:/etc/timezone:ro \
  "${DEVICE_ARGS[@]}" \
  "${GROUP_ARGS[@]}" \
  "$IMAGE"

echo "== waiting for sshd =="
# Checking the published port from outside isn't reliable: docker's
# port-forwarding proxy accepts the TCP connection the moment the container
# starts, even before sshd inside has actually bound port 22 — that races
# ahead of ssh.service and gets you a "Connection closed by remote host"
# from the SSH client below. Ask systemd directly instead.
for i in $(seq 1 30); do
  [ "$(docker exec "$CONTAINER" systemctl is-active ssh.service 2>/dev/null)" = "active" ] && break
  sleep 1
done

# Stop the headless, auto-started copy — see README.md's "Full DRM master"
# section for why you should never chase that one via a local VT switch.
# The windowed instance below is the one you actually look at.
docker exec "$CONTAINER" systemctl stop kiosk.service || true

echo "== opening windowed instance (password: kiosk) =="
# -o StrictHostKeyChecking=no/UserKnownHostsFile=/dev/null: this container's
# host key is fresh every time it's recreated, so strict checking would
# just fail on the 2nd run — see the ksshaskpass note in README.md if this
# hangs/fails on a KDE desktop instead of prompting normally.
# No QT_QPA_PLATFORM override here — deliberately. Forcing eglfs's
# eglfs_x11 dev-convenience integration was the actual cause of every
# windowed-preview bug chased in this file's history (dead input, stuck
# repaint after alt-tab, no I-beam cursor over text fields — see git log):
# eglfs_x11 doesn't handle expose/focus events or cursor themes the way a
# normal desktop platform does, because eglfs itself is built for a single
# continuously-owned surface that's never obscured, which is never true of
# a window you can alt-tab away from. Leaving QT_QPA_PLATFORM unset lets Qt
# fall back to its normal `xcb` platform against the forwarded X11
# connection instead — a real desktop integration with correct
# expose/focus/cursor handling. You lose eglfs-specific fidelity (fixed
# 1080x1920, no window decorations) this way; that's fine for day-to-day
# UI iteration — do a real eglfs/DRM-master pass on the actual tablet
# before considering something "done" (see README.md).
# -Y (trusted forwarding), not -X (untrusted): untrusted forwarding applies
# the X11 SECURITY extension, which silently restricts/drops a specific set
# of requests on the forwarded connection — cursor shape changes among them.
# -Y skips that restriction entirely. Same "local single-user dev box
# only" trust tradeoff as the rest of this container already accepts.
SSH_ASKPASS_REQUIRE=never ssh -Y -p 2222 \
  -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  kiosk@localhost \
  'export QT_IM_MODULE=qtvirtualkeyboard; exec /opt/kiosk/bin/ha-kiosk' \
  || true

echo
echo "== window closed =="
echo "re-run ./run.sh to reopen it, or:"
echo "docker exec $CONTAINER systemctl start kiosk.service   # headless, full DRM master — do NOT VT-switch to watch it, see README.md"
echo "docker exec $CONTAINER systemctl status kiosk ha-kiosk-google-calendar-sync ha-kiosk-weather-sync"
echo "docker exec $CONTAINER journalctl -f"
echo
echo "after a source change: ./build.sh, then re-run ./run.sh"
