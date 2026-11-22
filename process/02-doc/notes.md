# Yellow Pad: project notes

A touchscreen Bluetooth gamepad for driving the DOME robot. An ESP32 "Cheap
Yellow Display" board shows four arrows and per-direction speed settings, and
pairs with the robot's Raspberry Pi as a standard BLE gamepad. On the Pi, the
standard `joy` and `teleop_twist_joy` packages turn it into `/cmd_vel`. On
the remote computer (the VM), rosbridge feeds the pad's ROS tab and
`pad_buttons` turns presses of the action keys (Joke, Status, C) into LLM requests.

This repo is `Boston-Robot-Hackers/dome_pad`. It was `yellow_pad` (a
submodule of `esp32-c6-demo`) until 2026-11-22. Paths below are relative to
the repo root. Status and next steps are in `current.md`, not here.

## Known open checks

**The pad has driven the robot**: dome2's F05 run E2d (`d y`, with the old
`yellow_pad.launch.py`) passed, tested by the user. Still open:

- **The exact `/cmd_vel` values** against the table under "Testing" below
  were never recorded.
- **Possible duplicate joystick nodes.** Check `ros2 node list` with the
  other subsystems running: if any already starts `joy_node` or
  `teleop_twist_joy_node`, it will clash with this package.

## Layout

```
dome_pad/
├── firmware/         ESP32 firmware (PlatformIO project; colcon ignores it)
│   ├── platformio.ini   one env, cyd
│   └── src/main.cpp     the tabbed design; src/ros_tab.cpp is the ROS tab
├── dome_pad/         Python: pad_buttons.py (button table) and pad_buttons_node.py
├── launch/           robot.launch.py (joy + teleop), remote.launch.py (rosbridge + pad_buttons)
├── config/pad.yaml   joy and teleop parameters
├── backup/           factory firmware image + how to restore it (see backup/README.md)
├── test/             unit tests (conftest fakes rclpy)
└── process/          DOME process docs; this file is process/02-doc/notes.md
```

## The hardware

- **Board:** ESP32-2432S028R, the "Cheap Yellow Display" (CYD). 2.8" 320x240
  ILI9341 screen with resistive touch (XPT2046). Sold by Shenzhen Jingcai
  Intelligent; manual at http://pan.jczn1688.com/1
- **Chip:** ESP32-D0WD-V3 rev 3.1, dual core 240 MHz, Wi-Fi, Bluetooth Classic
  and BLE. 4 MB flash, no PSRAM. MAC 68:09:47:85:c8:b0 (Bluetooth address
  should be ...:c8:b2).
- **USB:** micro-USB through a CH340 serial chip. macOS has the driver built
  in; it appears as `/dev/cu.usbserial-*` (was `-2130`; the number depends on
  the port), or as `/dev/cu.wchusbserial*` (seen 2026-11-10 as `...110`).
  PlatformIO may pick another serial port (e.g. a Bluetooth label maker), so
  pass `--upload-port` if more than one is listed.
- **Cables:** the manual says don't use a USB-C to USB-C cable (the board
  lacks the resistors that make a USB-C port switch on). A micro-USB data
  cable through a USB-C adapter works.
- **Serial reliability:** through the Mac mini's USB-C adapter, large reads
  corrupt at every speed tried. Uploads are set to 115200 baud. Short
  operations are fine.
- **Power on the go:** a USB power bank with a micro-USB cable. Watch for
  banks that auto-shut-off at low current (the board draws about 0.1 A,
  measured with the CPU at 80 MHz and the backlight at half); use a bank
  with a low-current/trickle mode. (A power bank's
  own lithium cells are fine: it outputs regulated 5 V and has its own
  charging and protection.) Don't wire a bare LiPo cell to the board: it
  gives 3.0-4.2 V rather than 5 V, and the board has no charging or
  low-voltage cutoff.
- **Built-in battery (on hand, not yet tested):** NULLLAB PM11 module
  (docs: https://github.com/nulllaborg/pm11-module). It is the board sold as
  "LiPo Battery Module, USB-C Rechargeable 1200mAh, 3.3V 5V USB Output".
  Full connection and usage steps are in the parent repo's `POWER.md`,
  under "Using the PM11 with the CYD". 1200 mAh LiPo, boost to
  5 V/1.2 A, USB-C charging (~1 hour), 4-LED gauge, slide switch, 56×40 mm
  with two M4 holes, so it fits behind the board. Expect roughly 5-8 hours of
  driving per charge.
  - Connect with a short USB-A to micro-USB cable, or wire its 5V header to
    the CYD's 4-pin P1 connector (5V → VIN, GND → GND; check polarity against
    both pinouts first). Never use its 3.3V, BAT or JST outputs: the CYD
    needs 5 V.
  - **It has no low-battery cutoff.** Switch it OFF after driving (OFF fully
    disconnects the battery), and recharge when only one LED is lit.
  - **Not a UPS:** plugging or unplugging USB-C while running will likely
    reset the pad. That's safe (Bluetooth drops, the robot stops), but don't
    do it mid-drive.
  - With the switch OFF and USB-C plugged in, USB powers the board and the
    battery stays out of the circuit. Handy at the bench.
  - To check once it arrives: the docs don't mention a low-load auto
    shut-off, but confirm by running the pad on it for 30+ minutes.
  - Its LEDs aren't a reliable gauge. Options for a real one (MAX17048 fuel
    gauge on the pad's I2C, an inline USB meter, a resistor divider) and the
    MAX17048 wiring are in the parent repo's `power_monitor.md`, under
    "Measuring charge with an added part".
- **Other battery options** (suggestions from Alan Kilian, 2026-10-18):
  - All-in-one: the NULLLAB PM11 above
    (https://www.amazon.com/dp/B0GWZTQC35).
  - Separate charger plus cell: Diymore TP4056 USB-C charger boards, 5-pack
    (https://www.amazon.com/dp/B07MDPLQ18). 1 A charging, 4.2 V cutoff,
    discharge cutoff at 3.2 V, 3 A over-current protection. Check whether
    this board also boosts to 5 V: a plain TP4056 outputs only the battery's
    3.0-4.2 V, so it would need a separate 5 V boost board as well.
  - A matching cell: Qimoo 503035 3.7 V 500 mAh LiPo
    (https://www.amazon.com/dp/B0CNLNFPYM). About 2-3 hours of driving at
    the pad's draw, less than half the PM11's runtime.
  - Adafruit PowerBoost 500C (https://www.adafruit.com/product/1944):
    charger and 5 V boost in one board, runs the load while charging, pairs
    with any 1S LiPo cell. Well documented. It's the more reliable choice
    if the PM11 disappoints.
  - Cheapest: TP4057 USB-C charger boards with protection, 9-pack, from
    Temu
    (https://www.temu.com/9pcs-tp4057-1a-3-7v-lithium-battery-charging-and--board-with-protection-type-c-usb-c-li--charging-board-g-606737797099759.html).
    These charge only and don't boost, so they need a 5 V boost board too.

## How it works

```
Yellow Pad ──BLE gamepad──▶ Pi: /dev/input/js0
                              └▶ joy_node          → /joy      (sensor_msgs/Joy)
                                  ├▶ teleop_twist_joy → /cmd_vel (geometry_msgs/Twist)
                                  │                       └▶ robot's motor driver
                                  └▶ VM: pad_buttons  → /voice/llm_text (std_msgs/String)
                                                          └▶ dome_voice llm_node → robot speaks
```

### The action keys: Joke, Status, C

`pad_buttons` (on the VM) watches `/joy` and acts once on each press of a
mapped button; holding a button sends nothing more. The table is
`BUTTON_ACTIONS` in `dome_pad/pad_buttons.py`, with two kinds of action:
`LlmText` (a question on `/voice/llm_text`) and `Intent` (JSON straight to
`/intent`, source `pad`). The labels are `ACTION_LABELS` in `main.cpp`; they
were A, B and C until 2026-11-22.

- **Joke** (`/joy` button 1): `LlmText("Tell me a robot joke.")`.
  dome_voice's `llm_node` (VM) asks Claude, `voice_input` (Pi) relays the
  reply to `/announcement`, and `speech_output` (Pi) says it. So it needs
  `vo` in both dome2 launches.
- **Status** (button 2): `Intent("get_status")`. dome_control's
  `motion_behavior` says "linear speed …, angular speed …", so it needs the
  `control` subsystem on the Pi: `--options "d c vo p"`.
- **C** (button 3): unassigned.

**Why Status skips the LLM.** It first sent "What's your status?" to the
LLM, which calls `get_status`. That worked once, but the LLM never sees
dome_control's spoken answer (its tool result is just "Sent to the
robot."). After a few identical questions in one conversation, Claude
decided the robot wasn't replying, stopped sending the intent, and said so
instead (2026-11-22). Commands go straight to `/intent`; only conversational
buttons go through the LLM.

### The screen (landscape, 320x240)

The firmware is `src/main.cpp`, env `cyd`:

- **Top:** the status bar, then three tabs, **Drive**, **ROS** and
  **Settings**. The status bar has the runtime on the left (see "Runtime
  tracking") and two indicators on the right:
  - **BT:** teal when connected to the Pi, grey while waiting.
  - **WiFi:** grey when off (the ROS tab is closed), amber while joining
    Wi-Fi or connecting to rosbridge, teal when connected to rosbridge, red
    when `secrets.h` lacks `WIFI_SSID` or `ROSBRIDGE_HOST`.
- **Drive:** the cross of four arrows with a red STOP in the middle, and three
  keys **Joke**, **Status**, **C** on the right. Holding one holds gamepad
  button 2/3/4 (see "The action keys" above).
- **ROS:** publish rates of a few topics (`src/ros_tab.cpp`), fed by rosbridge on the
  remote computer (the VM) over Wi-Fi. **Wi-Fi is on only while this tab is
  open**, so driving never shares the radio with it; opening the tab takes a
  few seconds to connect.
  - Needs `src/secrets.h` (git-ignored, copy `secrets.h.sample`): `WIFI_SSID`,
    `WIFI_PASSWORD`, `ROSBRIDGE_HOST` (the VM's IP, 192.168.4.83 as of
    2026-11-22; `.local` names aren't resolved), optional `ROSBRIDGE_PORT`
    (9090). rosbridge is started by `bl dome_pad remote.launch.py` (needs
    `ros-kilted-rosbridge-suite` on the VM). If DHCP moves the VM, update
    `secrets.h` and reflash.
  - The connection state is the status bar's WiFi indicator; the page has no
    header of its own.
  - One row per topic in `topics[]` in `ros_tab.cpp`: now `/odom`
    and `/scan`, as placeholders. Each shows its rate (capped at "20+ Hz": the
    pad asks rosbridge for at most one message per topic every 50 ms), "-" if
    it hasn't published in 3 s, or "not published" if it isn't in rosapi's
    topic list. Green dot = active, amber = seen earlier, grey = never.
  - **Only these topics are subscribed.** An earlier version streamed every
    light topic to count its rate, which overran rosbridge's queue for the
    pad ("write queue full" on the VM) on 2026-11-22. `/scan` is a few KB of
    JSON per message; the pad parses only the envelope.
  - **Dead connections.** The warning kept coming after that fix: rosbridge
    listed two connections from the pad's address, one stale. The pad
    turned Wi-Fi off right after closing the socket (and sleeps or resets
    without closing it), and rosbridge's default `websocket_ping_interval`
    of 0 never drops a dead client. Fixed both ends: the pad waits 100 ms
    after the close, and `remote.launch.py` turns pings on (10 s, 30 s
    timeout).
  - While rosbridge is unreachable, each retry (every 3 s) can freeze touch for
    up to 1.5 s (`WEBSOCKETS_TCP_TIMEOUT` in `platformio.ini`).
- **Settings:** speeds per direction in real units: Forward and Back in m/s
  (step 0.05, default 0.15), Left and Right in rad/s (step 0.10, default 0.50).
  Tap a row, then **−** / **+**. Saved in flash (NVS, namespace `yellowpad2`).
  New settings go in the `settings[]` table in `main.cpp`. Four rows fill the
  page, so a fifth setting will need paging or scrolling.
- The pad converts a speed to a stick value by dividing by the robot's
  full-stick speed, `LINEAR_MAX` (1.5 m/s) and `ANGULAR_MAX` (1.0 rad/s) in
  `main.cpp`. **These must match `scale_linear.x` and `scale_angular.yaw` in
  `config/pad.yaml`**; if the YAML changes, change them too, or the
  displayed m/s will be wrong. The maximum speed is still capped by the YAML.

An earlier single-screen design (`arrows_v1.cpp`, speeds in %) was dropped
when the repo became dome_pad; it's in `yellow_pad`'s history. It used the
NVS namespace `yellowpad`, which is why this one uses `yellowpad2`.

### What the pad sends

| Pad | Gamepad | ROS `/joy` | `/cmd_vel` |
|---|---|---|---|
| Up / Forward held | left stick Y up by that speed | `axes[1]` = +speed | `linear.x` = +speed × 1.5 |
| Down / Back held | left stick Y down | `axes[1]` = −speed | `linear.x` = −speed × 1.5 |
| Left held | left stick X left | `axes[0]` = +speed | `angular.z` = +speed × 1.0 |
| Right held | left stick X right | `axes[0]` = −speed | `angular.z` = −speed × 1.0 |
| Any arrow held | button 1 pressed | `buttons[0]` = 1 | enables motion |
| Release / STOP / slide off | stick centered, button released | all 0 | robot stops |
| Joke / Status / C held | button 2 / 3 / 4 | `buttons[1..3]` = 1 | nothing (Joke's press goes to `pad_buttons`) |

Here "speed" is the stick fraction, 0 to 1: the set m/s or rad/s divided by
the full-stick speed. The full-stick speeds live on the Pi in `config/pad.yaml`
(`scale_linear.x` = 1.5 m/s, `scale_angular.yaw` = 1.0 rad/s), so the top
speed is set in the robot's config, not on the pad.

**Safety:** `teleop_twist_joy` only publishes motion while button 0 is held
(`require_enable_button: true`), and the pad only holds it while an arrow is
touched. Lifting your finger, Bluetooth dropping, or the pad losing power all
stop the robot. On release, teleop_twist_joy sends one zero Twist, then goes
quiet.

**Screen dimming:** after 2 minutes without a touch, the
backlight dims to a faint glow. The first touch only brightens it again and
does nothing else until you lift your finger, so tapping a dim screen can't
drive the robot. Holding an arrow counts as touching, so the screen never dims
while driving. Bluetooth connecting or dropping also brightens it.

**Deep sleep:** after 10 minutes without a touch, the pad
turns the backlight and screen off and deep-sleeps. Bluetooth drops, so the
robot stops. Touch the screen to wake it: it reboots, reconnects in a few
seconds, and ignores that touch until you lift your finger. It also sleeps
with the ROS tab open, and wakes on the Drive tab. The touch controller's
IRQ line (GPIO 36) wakes the chip. Asleep, the board's regulator,
USB-serial chip and power LED still draw several mA.

**Runtime tracking, for measuring battery life:** the left
end of the status bar (where the name was) shows `Run h:mm`, the running time since the battery
was last marked charged. The status bar is the thin strip across the very
top of the screen, above the tabs. **Press a finger on it and keep it there
for 3 seconds to mark the battery charged** (it goes back to `Run 0:00`).
Press near the top edge: a little lower hits a tab instead. For the first 10 seconds after power-on
it shows `Last run h:mm` instead: how long the previous power-on lasted.
Totals are saved to flash once a minute (namespace `yellowpad_pwr`), so a dead
battery loses at most a minute. Power-ons shorter than a minute (a dying
battery restarts repeatedly) don't replace the last run. Amber text means a
brownout reset happened since the last charge, a hint that the battery ran
out. The serial log also shows minutes spent dimmed and asleep. Deep sleep
doesn't end a run: the time asleep counts toward `Run` and `Last run`, and
waking doesn't show `Last run`.

To measure a charge: charge the PM11, hold the status bar, use the pad as
normal until the battery dies, then power it from USB and read `Last run`
(one continuous session) or `Run` (the total since charge).

**Limits:** resistive touch reads one point at a time, so one direction at a
time, no diagonals, and you can't hold an action key while driving. Pairing works
with Linux, macOS, Windows and Android; iOS generally ignores generic BLE
gamepads.

## Running it

### Pad

Power it on (USB or power bank). It advertises as "Yellow Pad" and reconnects
to the Pi on its own once paired and trusted. A BLE gamepad connects to one
host at a time, so if it's connected to the Mac, the Pi can't see it.

### Pairing with a Pi (one-time)

```bash
sudo apt update && sudo apt install bluez      # if bluetoothctl is missing
sudo systemctl enable --now bluetooth
bluetoothctl list                               # must show a Controller
# On Ubuntu for Pi, if no controller: sudo apt install pi-bluetooth && sudo reboot

bluetoothctl
  power on
  agent on
  default-agent
  scan on            # wait for "Yellow Pad"
  scan off
  pair <addr>
  trust <addr>       # makes it auto-reconnect
  connect <addr>
  exit
```

Check: `ls /dev/input/js*` and `ros2 run joy joy_enumerate_devices`.
If pairing gets stuck: `bluetoothctl remove <addr>` and start again.

### ROS package on the Pi and the VM

dome_pad is in dome2's `stack/packages.yaml`, so it's cloned into
`~/ros2_ws/src/dome_pad` on both computers and pulled with the rest. On the
Pi, first remove the old copy from before the rename (two packages that
start the same nodes are confusing, and `yellow_joy` builds as `yellow_pad`):

```bash
rm -rf ~/ros2_ws/src/yellow_joy ~/ros2_ws/build/yellow_pad ~/ros2_ws/install/yellow_pad
```

Then on each computer:

```bash
cd ~/ros2_ws
rosdep install --from-paths src/dome_pad -y   # joy, teleop_twist_joy, rosbridge_server
colcon build --packages-select dome_pad
source install/setup.bash
```

Run:

```bash
bl dome2 robot.launch.py --options "d vo p"   # Pi: drivers, voice, pad
bl dome2 remote.launch.py --options "vo p"    # VM: LLM node, rosbridge + pad_buttons
```

Or one side alone: `bl dome_pad robot.launch.py` / `remote.launch.py`.

To actually drive, the robot's drivers must also be running (they own the
motor driver). Run only one teleop at a time: anything else publishing to
`/cmd_vel` (e.g. teleop_twist_keyboard) will fight the pad.

### Testing (do this with wheels off the ground or bringup off)

```bash
ros2 topic echo /cmd_vel
```

With the default speeds (Forward and Back 0.15 m/s, Left and Right
0.50 rad/s): Forward → `linear.x: 0.15`, Back → `-0.15`,
Left → `angular.z: 0.5`, Right → `-0.5`, release → one all-zero message.
Forward set to 0.25 → `linear.x: 0.25`.

Joke key: `ros2 topic echo /voice/llm_text` on either computer, tap Joke once
→ one `data: Tell me a robot joke.`; holding it sends no more.

Troubleshooting:

- Nothing on `/cmd_vel`: `ros2 topic echo /joy` and press an arrow. If `/joy`
  moves but `/cmd_vel` doesn't, the enable button index is wrong (check which
  `buttons[]` entry goes to 1 and set `enable_button` to it).
- A pair of directions reversed: negate the scale in the YAML (e.g.
  `scale_angular.yaw: -1.0`).
- Robot ignores commands: `ros2 topic info /cmd_vel`. If it's `TwistStamped`,
  set `publish_stamped_twist: true`.

## Changing the firmware

The firmware is `firmware/src/main.cpp` and `firmware/src/ros_tab.cpp`;
build settings are in `firmware/platformio.ini` (one env, `cyd`).

### Build, flash, watch

No install needed: PlatformIO runs via `uvx` (from `brew install uv`).

```bash
cd firmware                                    # from the repo root
uvx --from platformio pio run                  # build only
uvx --from platformio pio run -t upload        # build + flash
uvx --from platformio pio device monitor       # serial output (Ctrl-C to quit)
```

Close any serial monitor before uploading. If an upload fails with corrupt
data, run it again. The Arduino IDE's bundled esptool is Intel-only and fails
on this Mac ("bad CPU type"); use the commands above instead.

Library and platform versions are pinned in `platformio.ini` (espressif32
7.1.3, ESP32-BLE-Gamepad 0.8.0, NimBLE-Arduino 2.5.1, TFT_eSPI 2.5.43,
XPT2046_Touchscreen from GitHub). Change them deliberately: the BLE libraries
are sensitive to version mismatches.

### Common changes

| Want | Change |
|---|---|
| Speed step, default, or a new setting | the `settings[]` table (values in thousandths: mm/s, mrad/s). Saved values override the defaults. |
| Robot's full-stick speed changed in the YAML | `LINEAR_MAX`, `ANGULAR_MAX` |
| Bluetooth name | `BleGamepad gamepad("Yellow Pad", ...)`. Then update `device_name` in the YAML and re-pair on the Pi (`remove`, then pair again). |
| Taps land off-target | `RAW_X_MIN/MAX`, `RAW_Y_MIN/MAX` (touch calibration) |
| Button positions/sizes | `TAB_RECTS`, `PAD_CX`, `PAD_CY`, `KEY`, `ACTION_RECTS`, `ROW_Y`, `ROW_H`, `MINUS_RECT`, `PLUS_RECT` |
| Action key labels (Joke, Status, C) or gamepad buttons | `ACTION_LABELS`, `ACTION_BUTTONS` |
| Colors | `COLOR_*` constants |
| Which way an arrow pushes the stick | the `switch` in `sendGamepad()` |
| Report rate / touch polling | `LOOP_MS` (20 ms) |
| Screen brightness or CPU speed | `BACKLIGHT_LEVEL` (0–255, default 128), `CPU_MHZ` (80; BLE needs at least 80) |
| Idle dimming | `DIM_AFTER_MS` (2 min), `DIM_LEVEL` (0–255, default 12) |
| Idle deep sleep | `SLEEP_AFTER_MS` (10 min) |
| Runtime tracking | `SAVE_EVERY_MS` (1 min), `CHARGED_HOLD_MS` (3 s), `LAST_RUN_SHOW_MS` (10 s) |

How the code is organized:

- `setup()` loads speeds from flash, starts the display, the touch controller
  (on its own SPI bus, pins 25/32/33/36/39) and the BLE gamepad (2 axes,
  4 buttons, range 0-32767, center 16384, manual reports).
- `loop()` redraws the status when the connection changes, then calls
  `handleTouch()` every 20 ms.
- `handleTouch()` decides what's under the finger. On the Drive page, arrows,
  STOP and the action keys follow the finger continuously. Tabs, setting rows and −/+ act
  once per tap.
- `setActive()` / `setActionHeld()` change the held arrow or button, redraw,
  and call `sendGamepad()`, which sets the axes and buttons 1-4 and sends a
  report.
- `selectTab()` redraws the tab bar and the page below it.
- Drawing is in the `draw*()` functions; only changed parts are redrawn.

### If the screen looks wrong

There are several CYD variants. If colors are inverted, add
`-DTFT_INVERSION_ON=1` to `build_flags`. If the image is wrong or blank, the
board may use an ST7789 instead: replace `-DILI9341_2_DRIVER=1` with
`-DST7789_DRIVER=1` (and possibly add `-DTFT_RGB_ORDER=TFT_BGR`).

### Ideas not done yet

- Long-press −/+ to change speed quickly.
- Tap-to-toggle motion instead of hold-to-move (would need care: it removes
  the dead-man behavior).
- Battery operation: the PM11 module is the plan (see "The hardware").

## Changing the ROS side

- **What a button does:** `BUTTON_ACTIONS` in `dome_pad/pad_buttons.py`
  (`LlmText` for a question, `Intent` for a command), keyed
  by `/joy` button index (Joke = 1, Status = 2, C = 3; never 0, the dead-man
  button). The key labels are `ACTION_LABELS` in `main.cpp`.
- **Top speeds:** `scale_linear.x` and `scale_angular.yaw` in
  `config/pad.yaml`. Start low; raise once trusted.
- **TwistStamped robots:** add `publish_stamped_twist: true` under
  `teleop_twist_joy_node` → `ros__parameters`.
- **Keep `deadzone: 0.0`.** joy rescales values outside the deadzone, so the
  stock teleop launch file's 0.3 deadzone would ignore 20% and turn 50% into
  about 29%. That's why this package has its own launch file.
- Changes go through git: commit and push from where you edit, then pull
  and rebuild on the other computers.

## Restoring the factory firmware

See `backup/README.md`. Short version:

```bash
cd backup                                       # from the repo root
shasum -a 256 -c factory-firmware-4MB.bin.sha256
uvx esptool --port /dev/cu.usbserial-XXXX --baud 115200 write-flash 0 factory-firmware-4MB.bin
```

Replace `XXXX` with the board's port (`ls /dev/cu.usbserial-*`). The Yellow
Pad firmware can be flashed again anytime from `firmware/`.
