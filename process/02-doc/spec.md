# dome_pad — Spec

dome_pad is the Yellow Pad, a handheld touchscreen gamepad for the DOME
robot: an ESP32-2432S028R ("Cheap Yellow Display") running this repo's
firmware, plus the ROS 2 package that connects it to the robot.

## Scope

- **Firmware** (`firmware/`): a BLE gamepad with Drive, ROS and Settings
  tabs. Drive sends stick values with button 0 held as the dead-man button;
  the Joke, Status and C keys are buttons 1-3 on `/joy`. The ROS tab shows the publish rates
  of a few fixed topics (now `/odom` and `/scan`), fed by rosbridge over
  Wi-Fi; nothing else is subscribed. Power saving: dimming, deep sleep, runtime
  tracking.
- **Robot side** (`launch/robot.launch.py`, dome2's `pad` subsystem):
  `joy_node` and `teleop_twist_joy`, publishing `/cmd_vel` only while the
  dead-man button is held. **Driving never depends on the remote computer.**
- **Remote side** (`launch/remote.launch.py`, dome2's remote `pad`
  subsystem): rosbridge for the ROS tab, and `pad_buttons`, which publishes
  a text on `/voice/llm_text` once per press of a mapped button. dome_voice's
  `llm_node` answers it and the robot speaks the reply.

## Button map

| `/joy` button | Pad | Action |
|---|---|---|
| 0 | any arrow | dead-man button; never mapped |
| 1 | Joke | "Tell me a robot joke." to the LLM |
| 2 | Status | "What's your status?" to the LLM, which publishes a `get_status` intent; dome_control answers it |
| 3 | C | unassigned |

## Out of scope

- Talking DDS directly from the pad; it uses rosbridge.
- LLM and speech code; dome_voice owns them.

## Key ROS interfaces

| Topic | Type | Direction |
|---|---|---|
| `/joy` | `sensor_msgs/Joy` | joy_node → teleop, pad_buttons |
| `/cmd_vel` | `geometry_msgs/Twist` | teleop → base |
| `/voice/llm_text` | `std_msgs/String` | pad_buttons → llm_node |
