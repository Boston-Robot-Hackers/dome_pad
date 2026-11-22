# Current Status — dome_pad

## Open

### Pick up here (2026-11-22)

- **New repo, from `yellow_pad`.** The firmware, factory backup and notes
  moved here with their history; the ROS package became `dome_pad`
  (ament_python). Tracked as dome2's F09.
- **Not run on ROS yet.** Unit tests pass on the Mac with fake rclpy; the
  firmware builds. Next: build and run on the Pi and the VM (TF09.6 in
  dome2).
- **Firmware not flashed** with the VM's rosbridge address yet.
- **Architecture confirmed (2026-11-22):** Bluetooth for driving (pad →
  Pi), Wi-Fi only for the ROS tab and the remote side. Wi-Fi-only driving
  was considered and rejected: it adds a network dependency, jitter, and
  roughly halves battery life.
- **Bluetooth drops at 5-10 m:** `process/05-issues/open/I01`. Not
  diagnosed; next step is `btmon` on the Pi for the disconnect reason.

### Blocked

- **The joke can't be heard yet:** the voice runtime (Piper etc.) isn't
  installed on the Pi. See dome2's `current.md`.
