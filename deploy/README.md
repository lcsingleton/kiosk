# Deploying to the real tablet (Debian 12, RK3576)

Nothing here is part of the Docker dev flow (`../docker/`) — these are the
files that actually get installed on the tablet's Debian 12 rootfs. Unlike
the dev container (which shares the host's amd64 GPU/kernel), the tablet has
no competing compositor by design, so the eglfs app can just open the DRM
device and become master with nothing to fight.

1. Build an arm64 release binary of the kiosk app. `../docker/` is amd64 and
   not suitable for producing tablet binaries — build on the tablet itself,
   or set up a proper arm64 cross-toolchain/build separately.

2. Copy the binary to `/opt/kiosk/` on the tablet.

3. Install runtime packages (no build tooling needed on the deployed
   device — that's a dev-container-only concern):

   ```
   apt install --no-install-recommends \
     libqt6gui6 libqt6qml6 libqt6quick6 libgbm1 libdrm2 libegl1 libgles2 \
     libinput10 libts0 fonts-dejavu-core i2c-tools libgpiod2 \
     qml6-module-qtquick-virtualkeyboard qt6-virtualkeyboard-plugin
   ```

4. Wire up the MCP2221A (I2C bridge for the VL53L0X + status LEDs):

   ```
   sudo groupadd -f i2c
   sudo usermod -aG i2c,plugdev,video,render kiosk
   sudo cp 99-mcp2221.rules /etc/udev/rules.d/
   sudo cp i2c-dev.conf /etc/modules-load.d/
   sudo udevadm control --reload-rules
   ```

5. Install and enable the kiosk service:

   ```
   sudo cp kiosk.service /etc/systemd/system/
   sudo systemctl enable --now kiosk.service
   ```

6. Reboot, then check it came up on its own:

   ```
   systemctl status kiosk
   journalctl -u kiosk -f
   ```

`kiosk.service` targets `multi-user.target`, not `graphical.target` — there's
no window manager or display manager on this device at all, so nothing
graphical-target-ish needs to be pulled in just to get the app running.
