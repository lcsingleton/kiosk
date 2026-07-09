# ha-tab kiosk

Qt6 eglfs kiosk app, prototyped in Docker before flashing the real RK3576
tablet.

- [`app/`](app/) — the Qt Quick source, built identically in both places
  below.
- [`docker/`](docker/) + [`run.sh`](run.sh) — Debian 12 dev container (eglfs,
  SSH, I2C/USB tooling). Not a hardware-accurate clone of the tablet's Mali
  GPU — it uses whatever DRM/KMS device your dev box has (here: `amdgpu` via
  `/dev/dri/card1`). Good enough to validate app logic, layout, and the
  I2C/USB sensor+LED path; GPU-driver-specific eglfs quirks will need a final
  check on the actual tablet. **Never deployed anywhere** — dev-only.
- [`deploy/`](deploy/) — the systemd unit, udev rule, and package list that
  actually get installed on the tablet's Debian 12. **Nothing here runs in
  Docker.**

## Build + run (dev container)

```
./run.sh
```

This loads `i2c-dev`/`hid-mcp2221` on the **host**, builds the image, and
passes through whatever `/dev/dri/*`, `/dev/i2c-*`, `/dev/gpiochip*` exist at
that moment. Then:

```
ssh -p 2222 kiosk@localhost   # password: kiosk
```

If you plug/unplug the MCP2221A, re-run `./run.sh` so the device list is
current — `/dev/i2c-N` numbering isn't guaranteed stable across replugs.

## MCP2221A + VL53L0X path

Since Linux 5.10, `hid-mcp2221` turns the dongle into standard kernel
devices — no vendor SDK needed:

- I2C bus → `/dev/i2c-N` (standard `i2c-dev`)
- 4x GPIO → a `gpiochip` (for driving the status LEDs)
- ADC/DAC also exposed, unused here

Inside the container:

```
lsusb                    # confirm 04d8:00dd MCP2221A shows up
i2cdetect -l             # find which i2c-N is "MCP2221 I2C Adapter"
i2cdetect -y N           # scan the bus — VL53L0X should show at 0x29
gpiodetect                # find the gpiochip exposed by the MCP2221A
gpioinfo <chip>           # GP0-GP3 lines, for the status LEDs
gpioset <chip> 0=1        # drive GP0 high — sanity-check an LED
```

`adafruit-blinka` + `adafruit-circuitpython-vl53l0x` are preinstalled for a
quick Python read of the sensor before wiring the real logic into the Qt
app:

```python
import board, busio
import adafruit_vl53l0x
i2c = busio.I2C(board.SCL, board.SDA)   # Blinka auto-detects the Linux i2c-dev bus
sensor = adafruit_vl53l0x.VL53L0X(i2c)
print(sensor.range)
```

Note: the MCP2221A's GPIO pins are 3.3V logic only, low current — fine to
toggle an LED through a driver transistor/MOSFET, not enough to drive one
directly at any real brightness.

## Hello-world app (`app/`)

Minimal Qt Quick window ("Hello, kiosk!") to sanity-check the toolchain
before writing real kiosk logic.

Build/run directly on the host desktop first (uses whatever platform plugin
your desktop session provides — Wayland/X11, not eglfs). The window is a
normal resizable desktop window — `main.qml` wraps the whole UI in a fixed
1080x1920 canvas that scales uniformly (letterboxed, never stretched) to fit
whatever size you resize the window to, so this is the easiest way to
iterate on layout/visuals without any eglfs/SSH/X11-forwarding involved:

```
sudo apt install qt6-base-dev qt6-declarative-dev cmake build-essential \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-virtualkeyboard qt6-virtualkeyboard-plugin   # if not already present
cd app && cmake -B build && cmake --build build
./build/hello-kiosk
```

Build/run the same source inside the container over SSH, forcing eglfs.
`run.sh` bind-mounts `app/` to `/home/kiosk/app`, so it's already there —
edit on the host, build/run over SSH:

```
ssh -p 2222 kiosk@localhost
cd /home/kiosk/app
cmake -B build && cmake --build build
QT_QPA_PLATFORM=eglfs QT_QPA_EGLFS_ALWAYS_SET_MODE=1 ./build/hello-kiosk
```

If SSH throws you into a GUI `ksshaskpass` dialog that can't parse the
host-key/password prompts (seen on KDE desktops — it hijacks any `ssh`
lacking a proper controlling terminal), bypass it for this command:

```
SSH_ASKPASS_REQUIRE=never ssh -p 2222 kiosk@localhost
```

(needs OpenSSH ≥ 8.4; older versions: `unset SSH_ASKPASS` first.)

## Running the Qt app under eglfs

```
export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_ALWAYS_SET_MODE=1
export QT_QPA_EGLFS_HIDECURSOR=1
export QT_QPA_GENERIC_PLUGINS=evdevtouch   # if a touchscreen is attached
./your_kiosk_app
```

No compositor/window manager involved — eglfs owns the DRM plane directly,
matching how it'll run standalone on the tablet. This is the "for real"
mode — it grabs DRM master, so it will fail with `drmSetMaster failed:
Permission denied` while your desktop compositor is also holding the GPU.
See the VT-switch dance below if you need this exact path.

## Windowed eglfs (no VT switching, no stealing the whole screen)

For day-to-day UI iteration you don't want to fight your desktop compositor
for the GPU every time. Qt's eglfs platform ships a second device-integration
backend, `eglfs_x11`, that renders through EGL into a normal X11 window
instead of taking DRM master — same `eglfs` QPA platform code paths in your
app, just windowed. It's already on this box (`libqeglfs-x11-integration.so`
from `libqt6opengl6`) and now installed in the image too.

Rebuild the image (adds `libqt6opengl6` + `xauth`, enables SSH X11
forwarding), then:

```
./run.sh                                             # rebuilds + runs
SSH_ASKPASS_REQUIRE=never ssh -X -p 2222 kiosk@localhost   # -X forwards X11 over the SSH tunnel
```

(see the `ksshaskpass` note above if plain `ssh -X` hangs/fails on prompts)

Inside that SSH session:

```
export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_INTEGRATION=eglfs_x11
export QT_QPA_EGLFS_WIDTH=1080
export QT_QPA_EGLFS_HEIGHT=1920
export QT_IM_MODULE=qtvirtualkeyboard
./build/hello-kiosk
```

`QT_QPA_EGLFS_WIDTH`/`HEIGHT` force the logical screen size eglfs reports to
the app — independent of whatever the real backend (X11 here, KMS on the
tablet) would've reported.

A window should appear on your host desktop via XWayland — no `xhost`, no
socket bind-mounts, no VT switching required, since `ssh -X` sets up its own
scoped `DISPLAY`/Xauthority for the session. Note it'll be **borderless and
sized exactly 1080x1920**, not a normal titlebar'd window you can drag/resize
— eglfs's model is "one surface = the screen," so even this x11-hosted dev
backend creates a fixed-size undecorated window rather than a real one.
Alt+Tab still gets you back to your desktop/terminal; it's not a real
takeover like the DRM-master path below.

If what you actually want is to *resize* the window and see the UI scale to
fit — e.g. checking layout at different sizes without the borderless/fixed
constraint — skip eglfs entirely and use the plain host-desktop build
instead (above). `main.qml`'s `Window` is a normal resizable window sized
540x960 by default; the whole UI sits in a fixed 1080x1920 `Item` that scales
uniformly (letterboxed, never stretched) to fit however you resize it.

Caveat: `eglfs_x11` is a Qt-provided *development convenience* — it doesn't
exercise real DRM/KMS mode-setting, exclusive-master behavior, or cursor
handling on real hardware planes. Treat it as the fast iteration loop, and
still do a real full-screen DRM-master pass (below) before considering
something "done".

## Full DRM master (real full-screen, matches the tablet)

Only one process can hold DRM master on a card at a time, and your desktop
compositor already holds it. To let the container's app take over the
physical screen:

```
Ctrl+Alt+F3                              # switch to a free text console —
                                          # logind tells your compositor to
                                          # drop DRM master
ssh -p 2222 kiosk@localhost              # from another machine, or a
                                          # session opened before switching
QT_QPA_PLATFORM=eglfs QT_QPA_EGLFS_ALWAYS_SET_MODE=1 ./build/hello-kiosk
```

Ctrl+C the app, then Ctrl+Alt+F1 (or F2) to get back to your desktop. If the
compositor doesn't release master on VT switch (`drmSetMaster failed:
Permission denied` persists), fall back to `sudo systemctl stop sddm` to
kill it outright, run the app, then `sudo systemctl start sddm`.

## Portrait output on the real tablet (1080x1920)

The RK3576 tablet's panel is physically 1920x1080 landscape — the listing's
"horizontal or vertical mode" is a software rotation, not a different native
panel mode. So on the real KMS backend you generally want both the forced
logical size *and* a rotation, not just the size on its own:

```
export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_WIDTH=1080
export QT_QPA_EGLFS_HEIGHT=1920
export QT_QPA_EGLFS_ROTATION=90    # or 270 — whichever matches your mount;
                                    # trivial to tell apart empirically
```

`QT_QPA_EGLFS_ROTATION` rotates the composited output before it's scanned
out, so the panel keeps running its native 1920x1080 KMS mode while the app
sees (and lays out for) a 1080x1920 screen. It's the same generic eglfs
option under `eglfs_x11`, so worth toggling now during the windowed dev loop
above — cheaper to get 90 vs. 270 right on your desktop than by rebooting
the tablet. Once confirmed, add all three `Environment=` lines to
[`deploy/kiosk.service`](deploy/kiosk.service).

## On-screen keyboard

There's no window manager, so there's no compositor to broker an on-screen
keyboard the way GNOME/KDE (or Wayland tools like `squeekboard`/`maliit`) do
— that infra doesn't exist here. **Qt Virtual Keyboard** is the fit instead:
it runs inside your own Qt process as a QML component, not a separate
service, so it needs nothing beyond what eglfs already gives you.

Packages (already added to [`docker/Dockerfile`](docker/Dockerfile) and
[`deploy/README.md`](deploy/README.md)):
`qml6-module-qtquick-virtualkeyboard`, `qt6-virtualkeyboard-plugin`.

Wiring, already in [`app/main.qml`](app/main.qml):
```qml
import QtQuick.VirtualKeyboard

InputPanel {
    id: inputPanel
    y: Qt.inputMethod.visible ? parent.height - height : parent.height
    anchors.left: parent.left
    anchors.right: parent.right
}
```
plus `QT_IM_MODULE=qtvirtualkeyboard` in the environment (in
`deploy/kiosk.service` for the tablet; export it manually for the dev-loop
commands above). Tapping any `TextField`/`TextInput` then auto-shows it.

If you want a custom layout instead (e.g. numeric-only, since this kiosk is
mostly proximity/light sensing + LEDs and may not need a full QWERTY
anywhere), skip the module entirely and build your own keypad out of plain
`Rectangle`/`Button`s that send `insert()`/`remove()` calls to whichever
field has focus — reasonable for a kiosk with a small, fixed set of input
screens.

## Caveats vs. the real deployment

- Different GPU/driver than the tablet's RK3576 Mali — eglfs backend
  selection and any vendor-specific KMS quirks won't be caught here.
- Container is `amd64` (or whatever your dev box is), tablet is `arm64` —
  fine for logic/UI prototyping, but don't ship binaries built here.
- `run.sh`'s device passthrough is scoped to what's plugged in at build
  time; the fallback `--privileged -v /dev:/dev` line in `run.sh` trades
  that scoping for convenience against hotplug churn — only use it on a
  trusted local box, never anything network-exposed.
