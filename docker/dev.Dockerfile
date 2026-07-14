FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive

# Emulator image: runtime-only, no build tooling (that's build.Dockerfile's
# job — see ../build.sh). systemd runs as PID 1 so the *real* systemd units
# below run exactly as they would on the tablet (deploy/README.md), instead
# of some container-only supervisor script standing in for them.
RUN apt-get update && apt-get install -y --no-install-recommends \
    systemd \
    systemd-sysv \
    dbus \
    logrotate \
    # --- ssh (runs as ssh.service once enabled below, not a custom CMD) ---
    openssh-server \
    # --- qt6 + eglfs runtime (see deploy/README.md step 3) ---
    libqt6gui6 \
    libqt6qml6 \
    libqt6quick6 \
    libqt6opengl6 \
    qml6-module-qtquick \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-templates \
    qml6-module-qtquick-window \
    qml6-module-qtquick-layouts \
    qml6-module-qtqml-workerscript \
    qml6-module-qtquick-virtualkeyboard \
    qml6-module-qt-labs-folderlistmodel \
    qt6-virtualkeyboard-plugin \
    libgbm1 \
    libdrm2 \
    libegl1 \
    libgles2 \
    libinput10 \
    libts0 \
    fonts-dejavu-core \
    fonts-noto-color-emoji \
    xauth \
    # --- calendar sync daemon's TLS ---
    libssl3 \
    # --- usb / i2c / gpio bring-up tooling (see root README.md) ---
    i2c-tools \
    usbutils \
    libhidapi-hidraw0 \
    gpiod \
    libgpiod2 \
    python3-pip \
    python3-smbus2 \
    vim-tiny \
    sudo \
    # --- ha-kiosk-presence-sync's camera capture + bring-up tooling ---
    # gstreamer1.0-libcamera (the libcamerasrc element) isn't guaranteed to
    # exist/work against Debian 12's bookworm libcamera build for every
    # sensor — confirm on the real tablet (see deploy/README.md's camera
    # bring-up section) and fall back to v4l2src (gstreamer1.0-plugins-good,
    # already below) if it doesn't.
    libgstreamer1.0-0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-libcamera \
    libcamera-tools \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --break-system-packages --no-cache-dir adafruit-blinka adafruit-circuitpython-vl53l0x

RUN sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/' /etc/ssh/sshd_config \
    && sed -i 's/#X11Forwarding yes/X11Forwarding yes/' /etc/ssh/sshd_config \
    && grep -q '^X11Forwarding yes' /etc/ssh/sshd_config || echo 'X11Forwarding yes' >> /etc/ssh/sshd_config

# Matches deploy/README.md's assumption that a "kiosk" user already exists —
# the real tablet's provisioning isn't in scope here, so create it
# ourselves. Group membership matches typical host GIDs for dialout/plugdev/
# video/render; actual device access on run comes from --group-add (see
# ../run.sh), since /dev/i2c-N only exists once the MCP2221A is plugged in.
RUN useradd -m -s /bin/bash -G dialout,plugdev,video,render kiosk \
    && echo 'kiosk:kiosk' | chpasswd \
    && echo 'kiosk ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# --- staged config, same relative paths as deploy/README.md ---
# Baked into the image (unlike /opt/kiosk below) since these are source,
# not build output, and change far less often than the C++ — rebuilding
# this image for a systemd-unit/logrotate edit is cheap and rare. Installed
# via CMake's etc/ install() rules too (see each module's CMakeLists.txt),
# so this COPY list and that install list should stay in sync.
COPY ha-kiosk/etc/systemd/system/kiosk.service /etc/systemd/system/kiosk.service
# Container-only adaptation, applied to the copy above, never to the source
# file (deploy/README.md installs that one unmodified on the real tablet).
# TTYs aren't namespaced in Linux containers: /dev/tty1 in here is the exact
# same device (major 4, minor 1) as the host's real /dev/tty1, so
# TTYPath=/dev/tty1 + Conflicts=getty@tty1.service fight the host's actual
# VT1 (and whatever real getty/session lives there), not some
# container-private console. Retarget to a VT the host isn't using instead
# (tty4 here), so Ctrl+Alt+F4 on the host is what shows this. A
# kiosk.service.d/ drop-in can't achieve this: TTYPath= generates an
# implicit Conflicts=getty@tty1.service the moment systemd parses it in the
# base fragment, before any later drop-in even loads, and that implicit
# edge survives even an explicit `Conflicts=` reset — has to be fixed at
# the source text instead. Change both tty numbers together if tty4 isn't
# free on your host (check `systemctl list-units 'getty@*' 'autovt@*'`).
RUN sed -i 's/tty1/tty4/g' /etc/systemd/system/kiosk.service
COPY ha-kiosk/etc/logrotate.d/ha-kiosk /etc/logrotate.d/ha-kiosk
COPY ha-kiosk/etc/modules-load.d/i2c-dev.conf /etc/modules-load.d/i2c-dev.conf
COPY ha-kiosk/etc/udev/rules.d/99-mcp2221.rules /etc/udev/rules.d/99-mcp2221.rules
COPY ha-kiosk/etc/udev/rules.d/99-native-gpio.rules /etc/udev/rules.d/99-native-gpio.rules
COPY ha-kiosk-google-calendar-sync/etc/systemd/system/ha-kiosk-google-calendar-sync.service /etc/systemd/system/ha-kiosk-google-calendar-sync.service
COPY ha-kiosk-google-calendar-sync/etc/tmpfiles.d/kiosk.conf /etc/tmpfiles.d/kiosk.conf
COPY ha-kiosk-google-calendar-sync/etc/logrotate.d/ha-kiosk-google-calendar-sync /etc/logrotate.d/ha-kiosk-google-calendar-sync
COPY ha-kiosk-weather-sync/etc/systemd/system/ha-kiosk-weather-sync.service /etc/systemd/system/ha-kiosk-weather-sync.service
COPY ha-kiosk-weather-sync/etc/logrotate.d/ha-kiosk-weather-sync /etc/logrotate.d/ha-kiosk-weather-sync
COPY ha-kiosk-presence-sync/etc/systemd/system/ha-kiosk-presence-sync.service /etc/systemd/system/ha-kiosk-presence-sync.service
COPY ha-kiosk-presence-sync/etc/logrotate.d/ha-kiosk-presence-sync /etc/logrotate.d/ha-kiosk-presence-sync

# /opt/kiosk (the binaries + qml/assets) is deliberately NOT copied in here
# — run.sh bind-mounts ./dist/opt/kiosk from the build container's output
# (see ../build.sh) so a rebuild is `./build.sh` + a service restart, not a
# rebuild of this image. /etc/kiosk (the daemon configs, which hold
# per-deployment secrets/location) is bind-mounted the same way, seeded
# from the *.example.json files by run.sh on first run.
RUN mkdir -p /opt/kiosk /etc/kiosk

RUN systemctl enable \
    ssh.service \
    kiosk.service \
    ha-kiosk-google-calendar-sync.service \
    ha-kiosk-weather-sync.service \
    ha-kiosk-presence-sync.service

STOPSIGNAL SIGRTMIN+3
EXPOSE 22

CMD ["/lib/systemd/systemd"]
