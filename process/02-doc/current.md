# Current Status — dome_pad

## Open

### Pick up here (2026-11-22, late): the session moves to the Pi

**Everything works on the robot and VM** (dome2 F09, done): driving over
Bluetooth, the ROS tab over Wi-Fi, and the Joke and Status keys. All work
is committed and pushed.

- **Keys:** Joke (`/joy` 1) asks the LLM for a robot joke; Status (`/joy` 2)
  sends `get_status` straight to `/intent` (no LLM; dome_control answers, so
  the Pi needs `c`); C (`/joy` 3) is unassigned. A key can't fire twice
  within 1 s (touch dropouts sent Status twice). Table: `BUTTON_ACTIONS` in
  `dome_pad/pad_buttons.py`.
- **ROS tab:** only `/odom` and `/scan` rates (placeholders); BT and WiFi
  indicators in the status bar.
- **Launch:** Pi `bl dome2 robot.launch.py --options "d c vo p"`, VM
  `bl dome2 remote.launch.py --options "vo p"`.
- **Next for the pad: diagnose I01 (Bluetooth drops) on the Pi.** It needs
  the Pi, so it's the first dome_pad task there. While using the pad, run
  `sudo btmon | grep -A3 "Disconnect Complete"`; when it drops, read the
  reason code and match it to I01's theories (0x08 = radio timeout, 0x13 =
  the pad ended it, no event before a reconnect = the pad rebooted). Note
  what was running (vision, Foxglove, the ROS tab open). Record the result
  in I01.
- The speech delay after a key press is dome_voice's F15, not dome_pad
  (see its `current.md`).

### Firmware is built on the Mac only

- **Don't build or flash the firmware on the Pi** (the user's decision,
  2026-11-22). On the Pi, dome_pad is only the ROS package; leave
  `firmware/` alone.
- `firmware/src/secrets.h` (git-ignored: Wi-Fi password, `ROSBRIDGE_HOST`)
  exists only on the Mac, so the Pi's checkout couldn't build it anyway.
  Firmware changes are made and flashed from the Mac
  (`uvx --from platformio pio run -t upload` in `firmware/`).

### Open issues

- **I01, Bluetooth drops at 5-10 m:** not diagnosed. Next: on the Pi,
  `sudo btmon | grep -A3 "Disconnect Complete"` while using the pad, and
  read the reason code (see the issue).

### Ideas, not started

- A real set of topics for the ROS tab, in place of `/odom` and `/scan`.
- The battery on the ROS tab: dome_telemetry publishes
  `dome_telemetry_msgs/BatteryStats` on `/telemetry/battery`.
- A use for key C.
