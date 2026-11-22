# F01 — ROS tab shows only the rates of /odom and /scan

**Priority**: Medium

**Date Created:** 2026-11-22

**Done:** yes

**Tasks File Created:** yes

**Tests Written:** no (firmware display; manual test only, see TF01)

**Test Passing:** no (not flashed yet)

**Description**: The ROS tab stops listing every topic. It shows one row
each for `/odom` and `/scan` with their publish rates, as placeholders
until the real set is chosen. The pinned `/cmd_vel` and battery rows, the
`/rosout` lines, the topic list and its paging all go. Only those two
topics are subscribed.

**The connection state moves to the status bar**, which now has a **BT**
and a **WiFi** indicator in place of "Connected" / "Waiting for
Bluetooth". The ROS tab's own header line goes.

**Why**: streaming every light topic to count its rate overran rosbridge's
queue for the pad ("write queue full" on the VM, 2026-11-22). Requested
directly by the user, who also asked that nothing else stay.

## How to Demo

**Setup**: rosbridge on the VM (`bl dome_pad remote.launch.py`), the robot
running something that publishes `/odom` and `/scan` (`d` on the Pi).

**Steps**:
1. Flash: `cd firmware && uvx --from platformio pio run -t upload`.
2. Open the ROS tab.

**Expected output**: the status bar's WiFi dot goes amber, then teal; BT
stays teal while paired. The page shows `/odom` and `/scan` with rates
("20+ Hz" at or above 20 Hz). A topic that isn't
published shows "not published". No "write queue full" on the VM.

## Process Gate

After creating this feature file and the corresponding task file, **stop and present the plan to the user**. Do not write any code or content until the user gives explicit approval to proceed.
