FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    # --- ssh ---
    openssh-server \
    # --- qt6 + eglfs runtime ---
    qt6-base-dev \
    qt6-declarative-dev \
    qml6-module-qtquick \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-templates \
    qml6-module-qtquick-window \
    qml6-module-qtquick-layouts \
    qml6-module-qtqml-workerscript \
    qml6-module-qtquick-virtualkeyboard \
    qml6-module-qt-labs-folderlistmodel \
    qt6-virtualkeyboard-plugin \
    libqt6gui6 \
    libqt6opengl6 \
    libgbm1 \
    libdrm2 \
    libegl1 \
    libgles2 \
    libinput10 \
    libts0 \
    fonts-dejavu-core \
    fonts-noto-color-emoji \
    xauth \
    # --- usb / i2c ---
    i2c-tools \
    libi2c-dev \
    usbutils \
    libusb-1.0-0-dev \
    libhidapi-hidraw0 \
    gpiod \
    libgpiod2 \
    python3-pip \
    python3-smbus2 \
    # --- build tooling ---
    build-essential \
    cmake \
    git \
    pkg-config \
    sudo \
    vim-tiny \
    && rm -rf /var/lib/apt/lists/*

# quick sensor bring-up without touching C++ yet
RUN pip3 install --break-system-packages --no-cache-dir adafruit-blinka adafruit-circuitpython-vl53l0x

RUN mkdir -p /run/sshd \
    && sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/' /etc/ssh/sshd_config \
    && sed -i 's/#X11Forwarding yes/X11Forwarding yes/' /etc/ssh/sshd_config \
    && grep -q '^X11Forwarding yes' /etc/ssh/sshd_config || echo 'X11Forwarding yes' >> /etc/ssh/sshd_config

# dev user — group membership matches typical host GIDs for dialout/plugdev/video/render;
# actual I2C device access on run comes from --group-add (see run.sh), since /dev/i2c-N
# only exists once the MCP2221A is plugged in on the host.
RUN useradd -m -s /bin/bash -G dialout,plugdev,video,render kiosk \
    && echo 'kiosk:kiosk' | chpasswd \
    && echo 'kiosk ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

WORKDIR /home/kiosk/app
RUN chown -R kiosk:kiosk /home/kiosk

EXPOSE 22
CMD ["/usr/sbin/sshd", "-D", "-e"]
