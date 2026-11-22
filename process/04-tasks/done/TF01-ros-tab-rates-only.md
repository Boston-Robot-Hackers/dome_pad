# TF01 ROS tab shows only the rates of /odom and /scan, for Feature F01

**Date Created:** 2026-11-22

## TF01.0 — Strip the ROS tab to two rate rows

**Status**: done (2026-11-22)

**Description**: `firmware/src/ros_tab.cpp`: `topics[]` is `/odom` and
`/scan`; each is subscribed (throttle 50 ms) once rosapi lists it. The
message filter keeps only the envelope, so `/scan`'s ranges aren't
parsed. Removed: the all-topics list and paging, `HEAVY_TYPES`, the
`/cmd_vel` and battery rows, the `/rosout` lines, and `rosTabTouch`
(from `ros_tab.h` and `main.cpp`). README, notes and spec updated.

**Test**: builds (`pio run`: success; RAM 17.8%, flash 41.3%). The display
is hardware, so the test is manual: F01's demo, not yet run.

## TF01.1 — Connection state in the status bar

**Status**: done (2026-11-22)

**Description**: `main.cpp`'s `drawStatus` shows **BT** and **WiFi**
indicators (dot and label) instead of "Connected" / "Waiting for
Bluetooth". `ros_tab.h` exposes `rosTabLink()` (OFF, JOINING, CONNECTING,
ONLINE, UNCONFIGURED); the loop redraws the status bar when it changes.
The ROS tab's header line is removed and its rows move up.

**Test**: builds with no warnings (`pio run`). Manual: F01's demo.
