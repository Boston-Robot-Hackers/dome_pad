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
- **Next for the pad:** nothing in progress. The speech delay after a key
  press is dome_voice's F15 (start there; see its `current.md`).

### Hazards for a session on the Pi

- **`firmware/src/secrets.h` is only on the Mac** (git-ignored: Wi-Fi
  password, `ROSBRIDGE_HOST` 192.168.4.83). The Pi's checkout can't build
  the firmware until it's copied over from `~/mydev/dome_pad/firmware/src/`
  on the Mac, or recreated from `secrets.h.sample`. Never commit it.
- **Flashing needs the pad on USB** of the machine running PlatformIO
  (`uvx --from platformio pio run -t upload` in `firmware/`). It has always
  been the Mac; the Pi has not been tried.

### Open issues

- **I01, Bluetooth drops at 5-10 m:** not diagnosed. Next: on the Pi,
  `sudo btmon | grep -A3 "Disconnect Complete"` while using the pad, and
  read the reason code (see the issue).

### Ideas, not started

- A real set of topics for the ROS tab, in place of `/odom` and `/scan`.
- The battery on the ROS tab: dome_telemetry publishes
  `dome_telemetry_msgs/BatteryStats` on `/telemetry/battery`.
- A use for key C.
