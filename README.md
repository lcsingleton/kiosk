# ha-tab kiosk

Qt6 eglfs kiosk app, prototyped in Docker before flashing the real RK3576
tablet.

- [`ha-kiosk/`](ha-kiosk/) — the Qt Quick app, built identically in both
  places below. [`ha-kiosk/src/`](ha-kiosk/src/) is the C++; the binary is
  the only architecture-dependent thing here, so it's the only thing that
  installs to `bin/`. [`ha-kiosk/qml/`](ha-kiosk/qml/) (UI source) and
  [`ha-kiosk/assets/`](ha-kiosk/assets/) (fonts/icons/images) are both
  architecture-independent data, so per the FHS they install under
  `share/ha-kiosk/qml/` and `share/ha-kiosk/assets/` instead — `main.cpp`
  resolves both at runtime via a fixed `bin/../share` offset from
  `applicationDirPath()`. [`ha-kiosk/etc/`](ha-kiosk/etc/) holds the config
  it owns on the tablet: the systemd unit and the udev/module-load rules for
  the I2C/GPIO hardware path.
- [`ha-kiosk-google-calendar-sync/`](ha-kiosk-google-calendar-sync/) — the
  calendar-sync daemon: Google Calendar auth, polling, and the local command
  socket the app talks to. Separate CMake target, same root build.
  [`ha-kiosk-google-calendar-sync/etc/`](ha-kiosk-google-calendar-sync/etc/)
  holds its example config.
- [`calendar-sync-client/`](calendar-sync-client/) — the command-socket wire
  protocol (types + a `QLocalSocket` client wrapper), shared between
  `ha-kiosk` (client side) and the daemon's `CommandServer` (server side of
  the same protocol). Lives at the root rather than nested under either
  consumer, since both link it.
- [`docker/build.Dockerfile`](docker/build.Dockerfile) + [`build.sh`](build.sh)
  — build-only Debian 12 image (Qt6 dev headers, no SSH, no runtime QML
  modules). Compiles the repo and stages install output into `./dist` via
  `DESTDIR=dist cmake --install build` — the same FHS tree
  [`deploy/README.md`](deploy/README.md) installs by hand on the tablet, and
  what a future `dpkg-buildpackage`-style `.deb` would stage too.
- [`docker/dev.Dockerfile`](docker/dev.Dockerfile) + [`run.sh`](run.sh) — the
  emulator: a Debian 12 container that runs `systemd` as PID 1 so the
  **real** `kiosk.service`/`ha-kiosk-google-calendar-sync.service`/
  `ha-kiosk-weather-sync.service` units (deploy/README.md) come up on their
  own, against whatever `/dev/dri/*`, `/dev/i2c-*`, `/dev/gpiochip*` your dev
  box passes through — no manual build/run step once it's up. Not a
  hardware-accurate clone of the tablet's Mali GPU — it uses whatever
  DRM/KMS device your dev box has (here: `amdgpu` via `/dev/dri/card0`).
  Good enough to validate app logic, layout, and the I2C/USB sensor+LED
  path; GPU-driver-specific eglfs quirks will need a final check on the
  actual tablet. **Never deployed anywhere** — dev-only.
- [`deploy/README.md`](deploy/README.md) — the walkthrough for installing
  the built app on the tablet's Debian 12: packages, hardware wiring, and
  where each module's `etc/` files land. **Nothing here runs in Docker.**

## Build + run (build container + emulator)

```
./build.sh   # compiles, stages ./dist/opt/kiosk + ./dist/etc
./run.sh     # loads i2c-dev/hid-mcp2221 on the host, starts the emulator
```

`run.sh` bind-mounts `./dist/opt/kiosk` (read-only) and a seeded
`./etc-kiosk/` (from the `*.example.json` files, first run only — same
manual step as deploy/README.md steps 6-7, just automated) into the
container, then passes through whatever `/dev/dri/*`, `/dev/i2c-*`,
`/dev/gpiochip*` exist at that moment. It runs `systemd` as PID 1, so all
three services are up immediately — then it stops the headless, auto-started
`kiosk.service` and opens `ha-kiosk` itself over `ssh -Y` (see "Windowed
preview" below), so `./run.sh` drops you straight
into a window with the app running, no separate manual step. Ctrl+C or close
the window when done; re-run `./run.sh` to reopen it, or `docker exec
ha-tab-kiosk-dev systemctl start kiosk.service` for the headless copy
instead (only ever check on that one via `journalctl`/`systemctl status` —
see "Full DRM master" below for why you should never VT-switch to watch it).

The calendar-sync daemon will crash-loop harmlessly until
`etc-kiosk/daemon-config.json`'s `serviceAccountKeyPath` points at a real key
(edit it, then `docker exec ha-tab-kiosk-dev systemctl restart
ha-kiosk-google-calendar-sync`) — everything else, including weather-sync,
comes up working out of the box.

After a source change, re-run `./build.sh` (incremental — it reuses `./build`
and `./dist` across runs) then re-run `./run.sh` to pick up the new
binaries (or, to avoid recreating the container, just restart the services
and reopen the window yourself):

```
docker exec ha-tab-kiosk-dev systemctl restart kiosk ha-kiosk-google-calendar-sync ha-kiosk-weather-sync
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

## Hello-world app (`ha-kiosk/`)

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
cd ha-kiosk && cmake -B build && cmake --build build
./build/bin/ha-kiosk
```

(that builds just `ha-kiosk/` standalone — see
[`ha-kiosk-google-calendar-sync/README`](ha-kiosk-google-calendar-sync/) or
the root build below to also build the calendar-sync daemon, which
additionally needs `libssl-dev` and links `Qt6::Network`.)

Build/run the same source in the emulator, forcing eglfs. The root
[`CMakeLists.txt`](CMakeLists.txt) builds `ha-kiosk/`,
`ha-kiosk-google-calendar-sync/`, and `ha-kiosk-weather-sync/` as sibling
targets from one `cmake -B build`, done by [`build.sh`](build.sh) (not
inside the emulator — it has no build tools, see above):

```
./build.sh && ./run.sh
```

`run.sh` already opens `ha-kiosk` for you in a window (see "Windowed
preview" below) — no manual invocation needed for day-to-day iteration. To
run it by hand instead (e.g. to try one-off env vars):

```
ssh -Y -p 2222 kiosk@localhost
/opt/kiosk/bin/ha-kiosk
```

The daemon binary lands at `/opt/kiosk/bin/ha-kiosk-google-calendar-sync` —
`ha-kiosk-google-calendar-sync.service` already runs it with `--config
/etc/kiosk/daemon-config.json` (seeded from
[`ha-kiosk-google-calendar-sync/etc/daemon-config.example.json`](ha-kiosk-google-calendar-sync/etc/daemon-config.example.json)
by `run.sh`'s first run — edit `etc-kiosk/daemon-config.json` on the host,
then `systemctl restart` it).

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

## Windowed preview (no VT switching, no stealing the whole screen)

For day-to-day UI iteration you don't want to fight your desktop compositor
for the GPU every time. `docker/dev.Dockerfile` has SSH X11 forwarding
enabled, so `./run.sh` opens `ha-kiosk` for you over a forwarded X11
connection — **this is what `./run.sh` opens by default**:

```
export QT_IM_MODULE=qtvirtualkeyboard
/opt/kiosk/bin/ha-kiosk
```

Deliberately **no** `QT_QPA_PLATFORM` override. An earlier version of this
container forced `QT_QPA_PLATFORM=eglfs` with the `eglfs_x11`
device-integration backend (renders through EGL into a normal X11 window
instead of taking DRM master — same `eglfs` QPA code paths the app uses on
the tablet, just windowed). That turned out to be the source of every
windowed-preview bug hit during this container's refactor: dead keyboard
input, content that never repaints after alt-tabbing away and back, no
I-beam cursor over text fields. Root cause: `eglfs` is built for a single
surface that continuously owns the whole display and is never obscured —
true on the tablet's real DRM/KMS output, never true of a window on your
desktop that you can alt-tab away from — so `eglfs_x11` simply doesn't wire
up expose/focus/cursor-theme handling the way a real desktop platform does.
Leaving `QT_QPA_PLATFORM` unset lets Qt fall back to its normal `xcb`
platform instead, which handles all of that correctly.

Use `-Y`, not `-X`, for the SSH forwarding itself: `-X` is *untrusted*
forwarding, which applies the X11 SECURITY extension and silently
restricts/drops a specific set of requests on the connection (cursor shape
changes among them). `-Y` is *trusted* forwarding and skips that
restriction. Same "local single-user dev box only, never network-exposed"
trust tradeoff already accepted elsewhere in this container.

To run it by hand instead (e.g. from a different terminal alongside the
one `run.sh` is holding open):

```
SSH_ASKPASS_REQUIRE=never ssh -Y -p 2222 kiosk@localhost   # -Y forwards X11 over the SSH tunnel, trusted
```

(see the `ksshaskpass` note above if plain `ssh -Y` hangs/fails on prompts),
then the exported-vars block above inside that session.

A window should appear on your host desktop via XWayland — no `xhost`, no
socket bind-mounts, no VT switching required, since `ssh -Y` sets up its own
scoped `DISPLAY`/Xauthority for the session. It's a normal resizable
desktop window (Qt's default `xcb` platform, not eglfs), same as the plain
host-desktop build above — `main.qml`'s `Window` sized 540x960 by default,
with the whole UI sitting in a fixed 1080x1920 `Item` that scales uniformly
(letterboxed, never stretched) to fit however you resize it. Alt+Tab works
like a normal window because it *is* one.

If you specifically need to validate `eglfs`/`eglfs_x11` behavior (fixed
resolution, rotation, virtual-keyboard-without-a-compositor), export
`QT_QPA_PLATFORM=eglfs QT_QPA_EGLFS_INTEGRATION=eglfs_x11
QT_QPA_EGLFS_WIDTH=1080 QT_QPA_EGLFS_HEIGHT=1920 QSG_RENDER_LOOP=basic` by
hand in a manual session (`QSG_RENDER_LOOP=basic` avoids a separate bug:
Qt Quick's default threaded render loop waits on a "frame swapped"
callback that can silently never arrive under `eglfs_x11` + software
rendering over a forwarded connection — the first frame paints, then it
never repaints again). Treat that as a one-off validation path, not the
day-to-day loop, and still do a real full-screen DRM-master pass (below)
before considering something "done" — `eglfs_x11` doesn't exercise real
DRM/KMS mode-setting, exclusive-master behavior, or cursor handling on
real hardware planes even when it's not actively broken.

## Full DRM master — do not test this locally via VT switching

**Do not switch to a local VT to let the emulator's `kiosk.service` grab
full DRM master.** An earlier version of this doc recommended exactly that
(retargeting `kiosk.service` to a spare host VT, e.g. `tty4`, then
Ctrl+Alt+F4 to it) — testing it caused a hard lockup of the physical
console (last-rendered frame frozen, no keyboard response, no way back
except a hard reboot of the whole machine). Root cause, best understanding:
Qt's eglfs KMS backend registers itself as the process the kernel signals
on VT-switch-away so it can cleanly drop master first; that handshake needs
a live, cooperating process, and this app is running across a container
boundary, restarted by `Restart=always`, with no `/dev/input` passthrough
for it to even receive input — if it crashes, hangs, or doesn't answer that
signal, the kernel has no one to negotiate the switch with and the VT is
stuck. Unlike a desktop compositor (which properly drops master on the
`logind` Pause/Resume signal), there's no guarantee this container-run app
does the equivalent cleanly.

The **real** validation of exclusive DRM-master/KMS behavior belongs on the
actual tablet (deploy/README.md) — hardware you can power-cycle without
losing your desktop session, not this dev box. `kiosk.service` in the
emulator *does* still attempt DRM master automatically the moment it
starts (unlike the old build-shell container, there's no manual opt-in),
so if it's fighting your desktop compositor for `/dev/dri`, expect it to
just fail with `drmSetMaster failed: Permission denied` and sit in its
`Restart=always`/`RestartSec=1` loop harmlessly (`docker exec
ha-tab-kiosk-dev journalctl -u kiosk -f`) — that's fine and expected, leave
it be. For actually seeing/interacting with the app on this dev box, use
the windowed preview above instead (or the manual `eglfs_x11` session for
eglfs-specific checks) — no VT takeover, no risk to your session.

(`docker/dev.Dockerfile` still retargets `kiosk.service`'s `TTYPath=`/
`Conflicts=` from `tty1` to `tty4` via `sed` — that part's fine and unrelated
to the lockup, it's about not fighting a real host getty for the *unit
name*, not an invitation to VT-switch there.)

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
option under `eglfs_x11`, so worth toggling now during a manual `eglfs_x11`
validation session (see "Windowed preview" above) — cheaper to get 90 vs.
270 right on your desktop than by rebooting the tablet. Once confirmed, add
all three `Environment=` lines to
[`ha-kiosk/etc/systemd/system/kiosk.service`](ha-kiosk/etc/systemd/system/kiosk.service).

## On-screen keyboard

There's no window manager, so there's no compositor to broker an on-screen
keyboard the way GNOME/KDE (or Wayland tools like `squeekboard`/`maliit`) do
— that infra doesn't exist here. **Qt Virtual Keyboard** is the fit instead:
it runs inside your own Qt process as a QML component, not a separate
service, so it needs nothing beyond what eglfs already gives you.

Packages (already added to [`docker/dev.Dockerfile`](docker/dev.Dockerfile) and
[`deploy/README.md`](deploy/README.md)):
`qml6-module-qtquick-virtualkeyboard`, `qt6-virtualkeyboard-plugin`.

Wiring, already in [`ha-kiosk/qml/main.qml`](ha-kiosk/qml/main.qml):
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
`ha-kiosk/etc/systemd/system/kiosk.service` for the tablet; export it manually for the dev-loop
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
- `run.sh`'s device passthrough (`--device`) is scoped to what's plugged in
  at run time, but the emulator container itself runs `--privileged` (plus
  the cgroup/tmpfs mounts systemd-as-PID-1 needs) — broader than the old
  build-shell container ever needed. Same "local single-user dev box only,
  never network-exposed" trust tradeoff as before, just no longer optional.
- TTYs are **not** namespaced away from the host the way PIDs/mounts/network
  are — `/dev/ttyN` inside the emulator is the exact same device the host
  sees. `kiosk.service`'s `Conflicts=getty@.service`/`TTYPath=` mechanism
  still works, but it's fighting over a *real host console*, not some
  container-private one — see "Full DRM master" above for why
  `docker/dev.Dockerfile` retargets it from `tty1` to `tty4`.
