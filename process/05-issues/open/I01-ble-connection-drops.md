# I01 The pad sometimes loses its Bluetooth connection to the Pi

**Opened:** 2026-11-22

## Symptom

The pad sometimes drops its BLE connection to the Pi at only 5-10 m, in
line of sight with no obstructions. Reported by the user; how often, and
what was running at the time, isn't recorded yet.

## Already known (from the firmware and library, not from tests)

- **TX power is already the maximum**, +9 dBm (ESP32-BLE-Gamepad's default
  `_transmitPowerLevel(9)`).
- **Supervision timeout is 6 s** (`updateConnParams(handle, 6, 7, 0, 600)`
  in the library's `BleConnectionStatus::onConnect`; interval 7.5-8.75 ms).
  So a radio drop means 6 s with no packet getting through, which is unlikely
  from range alone.
- The pad re-advertises on disconnect (`advertiseOnDisconnect(true)`), so it
  should reconnect by itself.

## Theories, most likely first

1. **The pad reset or slept**, not a radio drop: deep sleep after 10 min
   idle (by design), a brownout reset (amber `Run` text on the status bar),
   or a crash. A `Last run` in the status bar right after a drop means it
   rebooted.
2. **Opening the ROS tab**: joining Wi-Fi takes over the ESP32's single
   2.4 GHz radio and can starve BLE.
3. **The Pi's radio is starved**: its Wi-Fi and Bluetooth share one chip and
   antenna (CYW43455); heavy 2.4 GHz Wi-Fi traffic (camera, Foxglove) can
   squeeze Bluetooth out. Moving the Pi's Wi-Fi to 5 GHz would help.
4. **Body or hand blocking** the pad's PCB antenna, adding to 3.

## Next tests

- On the Pi, during use: `sudo btmon | grep -A3 "Disconnect Complete"`.
  The reason splits the theories: `Connection Timeout (0x08)` is the radio
  (2-4); `Remote User Terminated (0x13)` is the pad ending it (1, sleep);
  no disconnect event before a new connection means the pad rebooted (1).
- On the pad, over USB: `uvx --from platformio pio device monitor` in
  `firmware/`; after a drop, look for `Runtime: reset reason N` (15 =
  brownout, 4 or 7 = crash or watchdog).
- On the Pi: `iw dev wlan0 link` for the Wi-Fi band.
- Note whether drops line up with opening the ROS tab, or with vision or
  Foxglove running on the Pi.
