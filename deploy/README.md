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

6. Camera bring-up for `ha-kiosk-presence-sync` (motion-based screen wake).
   The vendor's MIPI-CSI camera depends on the kernel's `rkisp1` pipeline
   handler being wired up for libcamera — unconfirmed for this board, same
   as the native-GPIO header in step 4. On the tablet:

   ```
   libcamera-hello --list-cameras   # does the sensor show up at all?
   dmesg | grep -i rkisp            # did the ISP pipeline handler bind it?
   ```

   - If a camera is listed and `rkisp1` bound cleanly: the daemon's default
     config (`libcamerasrc ! videoconvert ! ...`, see
     `../ha-kiosk-presence-sync/etc/daemon-config.example.json`) should work
     as-is.
   - If nothing shows up, or the vendor BSP only exposes the sensor as a
     plain V4L2 device: change `cameraPipeline` in the daemon's config to
     `v4l2src device=/dev/videoN ! videoconvert ! video/x-raw,format=GRAY8,width=320,height=240,framerate=1/1`
     (find `N` via `v4l2-ctl --list-devices`) — nothing else about the
     daemon changes either way.

   Either path needs the `kiosk` user in the `video` group for device
   access — already covered if you took the MCP2221A path in step 5
   (its `usermod` includes `video`); if you took the native-GPIO path
   instead, add it explicitly: `sudo usermod -aG video kiosk`.

   Optional — higher-precision (>8-bit) capture for better low-light
   presence detection. Only worth trying if the above worked; whether it
   actually helps depends on this SoC's ISP, not just the sensor:

   ```
   gst-launch-1.0 libcamerasrc ! video/x-raw,format=GRAY16_LE ! fakesink -v
   ```

   Check the `-v` caps output: if this fails to negotiate at all, the
   pipeline (as opposed to the raw sensor) only ever exposes 8-bit on its
   normal video path — stick with the GRAY8 default; there's nothing to
   gain here. If it does negotiate, capture a still and check whether pixel
   values genuinely span more than 256 levels (e.g. via `v4l2-ctl
   --list-formats-ext -d /dev/videoN` to see the raw sensor's native format
   — `SRGGB10`/`SRGGB12` mean real 10-/12-bit sensor data exists upstream of
   the ISP, though the ISP's *processed* output reaching `libcamerasrc`
   may still crush it back to 8-bit). If it genuinely carries more
   precision, set `cameraPipeline` to the `GRAY16_LE` variant and
   `pixelMaxValue` in the daemon's config to match (see that file's own
   comment) — most embedded ISPs only rescale to fill the full 16-bit
   range if explicitly configured to, so check whether dark/bright test
   frames actually hit values near 0/65535 or stay clustered in a much
   narrower native-bit-depth band (e.g. 0-1023) before assuming full scale.

   Better still, if available — skip the ISP's demosaic/tone-mapping
   entirely and capture the sensor's raw Bayer mosaic directly. This needs
   libcamera's Raw stream role, which `rkisp1` may or may not expose for
   this sensor (unconfirmed, same as everything else in this step). The
   exact caps a given sensor/pipeline-handler can produce aren't worth
   guessing at — discover them directly:

   ```
   GST_DEBUG=libcamerasrc:5 gst-launch-1.0 libcamerasrc ! fakesink -v 2>&1 | grep -i caps
   ```

   Look for a raw/Bayer-shaped caps entry (a `format` naming a Bayer
   pattern — `rggb`/`bggr`/`grbg`/`gbrg` — or a libcamera/V4L2 raw fourcc
   like `SRGGB10`/`SBGGR12`) in what it reports being able to produce for
   this camera. If one shows up, set `cameraPipeline` to request it
   explicitly, with **no** `videoconvert` stage at all (there's nothing to
   convert — MotionDetector compares the raw mosaic against its own
   background reference position-for-position, which works without ever
   demosaicing it into real RGB/luma), e.g.:

   ```
   libcamerasrc ! video/x-bayer,format=rggb,width=320,height=240,framerate=1/1
   ```

   One real caveat: this only works if the reported format is *unpacked*
   (each sample sitting in its own whole byte or 16-bit slot). A *packed*
   raw format (several 10-/12-bit samples crammed across byte boundaries
   to save space — often named with a trailing `P`, e.g. `SRGGB10P`) needs
   real bit-unpacking code this daemon doesn't have; if that's all `rkisp1`
   offers, fall back to the GRAY8/GRAY16_LE path above instead. Set
   `pixelMaxValue` to the sensor's native bit depth (1023 for 10-bit, 4095
   for 12-bit) either way — raw sensor data is essentially never rescaled
   to fill a wider container the way a processed ISP output sometimes is.

7. Install the calendar-sync daemon's config, then enable it. Its runtime
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

8. Install the weather-sync daemon's config, then enable it. Same
   `/run/kiosk` runtime dir as the calendar daemon above, and its own
   `/var/log/ha-kiosk-weather-sync/`:

   ```
   sudo cp ../ha-kiosk-weather-sync/etc/daemon-config.example.json /etc/kiosk/weather-config.json
   # edit /etc/kiosk/weather-config.json: geohash (see the file's own comment for how to resolve one)
   sudo cp ../ha-kiosk-weather-sync/etc/logrotate.d/ha-kiosk-weather-sync /etc/logrotate.d/
   sudo cp ../ha-kiosk-weather-sync/etc/systemd/system/ha-kiosk-weather-sync.service /etc/systemd/system/
   sudo systemctl enable --now ha-kiosk-weather-sync.service
   ```

9. Install the presence-sync daemon's config, then enable it (see step 6
   for the `cameraPipeline` value to use). Same `/run/kiosk` runtime dir as
   the other two daemons, and its own `/var/log/ha-kiosk-presence-sync/`:

   ```
   sudo cp ../ha-kiosk-presence-sync/etc/daemon-config.example.json /etc/kiosk/presence-config.json
   # edit /etc/kiosk/presence-config.json: cameraPipeline, if step 6 needed the V4L2 fallback
   sudo cp ../ha-kiosk-presence-sync/etc/logrotate.d/ha-kiosk-presence-sync /etc/logrotate.d/
   sudo cp ../ha-kiosk-presence-sync/etc/systemd/system/ha-kiosk-presence-sync.service /etc/systemd/system/
   sudo systemctl enable --now ha-kiosk-presence-sync.service
   ```

10. Install and enable the kiosk service (logs to `/var/log/ha-kiosk/` the
   same way). How long the screen stays awake with no touch/camera activity
   before dimming defaults to 5 minutes; override it by adding
   `--idle-timeout-ms <ms>` to `ExecStart=` in `kiosk.service` before
   copying it across, if needed:

   ```
   sudo cp ../ha-kiosk/etc/logrotate.d/ha-kiosk /etc/logrotate.d/
   sudo cp ../ha-kiosk/etc/systemd/system/kiosk.service /etc/systemd/system/
   sudo systemctl enable --now kiosk.service
   ```

11. Set the system timezone — Debian defaults to UTC, and the app has no
   timezone override of its own (it trusts the system clock for every
   local-time computation: new-event creation, the header clock, the
   agenda's "now" line), so skipping this step means new events get created
   in GMT instead of wherever the tablet actually is:

   ```
   sudo timedatectl set-timezone Australia/Sydney   # replace with the tablet's actual zone
   ```

12. Reboot, then check all four came up on their own:

   ```
   systemctl status ha-kiosk-google-calendar-sync ha-kiosk-weather-sync ha-kiosk-presence-sync kiosk
   journalctl -u ha-kiosk-google-calendar-sync -u ha-kiosk-weather-sync -u ha-kiosk-presence-sync -u kiosk -f
   ```

`kiosk.service` targets `multi-user.target`, not `graphical.target` — there's
no window manager or display manager on this device at all, so nothing
graphical-target-ish needs to be pulled in just to get the app running.
