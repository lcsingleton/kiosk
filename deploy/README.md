# Deploying to the real tablet (Debian 12, RK3576)

This is the walkthrough for installing the built app on the tablet's Debian
12 rootfs — nothing here is part of the Docker dev flow (`../docker/`). The
config files it references live next to their owning module — `../ha-kiosk/etc/`
for the systemd unit and udev/module-load rules,
`../ha-kiosk-google-calendar-sync/etc/` and `../ha-kiosk-weather-sync/etc/`
for each daemon's example config, and each module's own `etc/logrotate.d/`
for its `/var/log` rotation config — and each mirrors its destination path on the
tablet (`ha-kiosk/etc/udev/rules.d/` becomes `/etc/udev/rules.d/` there, same as a
build system's DESTDIR staging tree), so each `cp` below just copies the
matching relative path across. Unlike
the dev container (which shares the host's amd64 GPU/kernel), the tablet has
no competing compositor by design, so the eglfs app can just open the DRM
device and become master with nothing to fight.

1. Build an arm64 release binary of the kiosk app. `../docker/` is amd64 and
   not suitable for producing tablet binaries — build on the tablet itself,
   or set up a proper arm64 cross-toolchain/build separately.

2. Install it to `/opt/kiosk/` (the root `CMakeLists.txt` defaults
   `CMAKE_INSTALL_PREFIX` there, so this needs no extra flags):

   ```
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   sudo cmake --install build
   ```

   This lands the binary at `/opt/kiosk/bin/ha-kiosk`, matching
   `ExecStart` in `kiosk.service` below.

3. Install runtime packages (no build tooling needed on the deployed
   device — that's a dev-container-only concern):

   ```
   apt install --no-install-recommends \
     libqt6gui6 libqt6qml6 libqt6quick6 libgbm1 libdrm2 libegl1 libgles2 \
     libinput10 libts0 fonts-dejavu-core i2c-tools libgpiod2 \
     qml6-module-qtquick-virtualkeyboard qt6-virtualkeyboard-plugin
   ```

4. The vendor says this mainboard breaks out GPIO directly (and possibly
   I2C) — worth checking before wiring up the MCP2221A, since native pins
   would mean driving the status LEDs (and maybe the VL53L0X) off the SoC
   instead of through the dongle. On the tablet:

   ```
   gpiodetect                # any gpiochip besides the MCP2221A's?
   gpioinfo <chip>            # line numbers/names, if so
   i2cdetect -l               # any native i2c-N bus besides the MCP2221A's?
   ```

   - If a native `gpiochip` shows up: use it for the status LEDs via
     `99-native-gpio.rules` below instead of the MCP2221A's 4 low-current
     3.3V lines. Note the line numbers from `gpioinfo` for the app config.
   - If a native `i2c-N` bus shows up: the VL53L0X can move to it too,
     dropping the MCP2221A from this flow entirely.
   - If neither shows up (or the vendor's "GPIO" turns out to be something
     else, e.g. debug UART pins): fall back to the MCP2221A path below as-is.

5. Wire up I2C/GPIO. Always:

   ```
   sudo cp ../ha-kiosk/etc/modules-load.d/i2c-dev.conf /etc/modules-load.d/
   ```

   For the MCP2221A (I2C bridge for the VL53L0X + status LEDs, or whichever
   half of that step 4 didn't move to native pins):

   ```
   sudo groupadd -f i2c
   sudo usermod -aG i2c,plugdev,video,render kiosk
   sudo cp ../ha-kiosk/etc/udev/rules.d/99-mcp2221.rules /etc/udev/rules.d/
   ```

   For native mainboard GPIO (status LEDs, if step 4 found a gpiochip):

   ```
   sudo groupadd -f gpio
   sudo usermod -aG gpio kiosk
   sudo cp ../ha-kiosk/etc/udev/rules.d/99-native-gpio.rules /etc/udev/rules.d/
   ```

   Then:

   ```
   sudo udevadm control --reload-rules
   ```

6. Install the calendar-sync daemon's config, then enable it. Its runtime
   state (command socket, snapshot, single-instance lock) lives under
   `/run/kiosk`, created automatically by the unit's `RuntimeDirectory=`
   below — `kiosk.conf` is only needed if you ever run the binary outside
   this unit (e.g. manual `--once` testing). Its logs land under
   `/var/log/ha-kiosk-google-calendar-sync/`, created the same way by the
   unit's `LogsDirectory=`, rotated per `logrotate.d`:

   ```
   sudo mkdir -p /etc/kiosk
   sudo cp ../ha-kiosk-google-calendar-sync/etc/daemon-config.example.json /etc/kiosk/daemon-config.json
   # edit /etc/kiosk/daemon-config.json: serviceAccountKeyPath, calendars, people
   sudo cp ../ha-kiosk-google-calendar-sync/etc/tmpfiles.d/kiosk.conf /etc/tmpfiles.d/
   sudo cp ../ha-kiosk-google-calendar-sync/etc/logrotate.d/ha-kiosk-google-calendar-sync /etc/logrotate.d/
   sudo cp ../ha-kiosk-google-calendar-sync/etc/systemd/system/ha-kiosk-google-calendar-sync.service /etc/systemd/system/
   sudo systemctl enable --now ha-kiosk-google-calendar-sync.service
   ```

7. Install the weather-sync daemon's config, then enable it. Same
   `/run/kiosk` runtime dir as the calendar daemon above, and its own
   `/var/log/ha-kiosk-weather-sync/`:

   ```
   sudo cp ../ha-kiosk-weather-sync/etc/daemon-config.example.json /etc/kiosk/weather-config.json
   # edit /etc/kiosk/weather-config.json: geohash (see the file's own comment for how to resolve one)
   sudo cp ../ha-kiosk-weather-sync/etc/logrotate.d/ha-kiosk-weather-sync /etc/logrotate.d/
   sudo cp ../ha-kiosk-weather-sync/etc/systemd/system/ha-kiosk-weather-sync.service /etc/systemd/system/
   sudo systemctl enable --now ha-kiosk-weather-sync.service
   ```

8. Install and enable the kiosk service (logs to `/var/log/ha-kiosk/` the
   same way):

   ```
   sudo cp ../ha-kiosk/etc/logrotate.d/ha-kiosk /etc/logrotate.d/
   sudo cp ../ha-kiosk/etc/systemd/system/kiosk.service /etc/systemd/system/
   sudo systemctl enable --now kiosk.service
   ```

9. Set the system timezone — Debian defaults to UTC, and the app has no
   timezone override of its own (it trusts the system clock for every
   local-time computation: new-event creation, the header clock, the
   agenda's "now" line), so skipping this step means new events get created
   in GMT instead of wherever the tablet actually is:

   ```
   sudo timedatectl set-timezone Australia/Sydney   # replace with the tablet's actual zone
   ```

10. Reboot, then check all three came up on their own:

   ```
   systemctl status ha-kiosk-google-calendar-sync ha-kiosk-weather-sync kiosk
   journalctl -u ha-kiosk-google-calendar-sync -u ha-kiosk-weather-sync -u kiosk -f
   ```

`kiosk.service` targets `multi-user.target`, not `graphical.target` — there's
no window manager or display manager on this device at all, so nothing
graphical-target-ish needs to be pulled in just to get the app running.
