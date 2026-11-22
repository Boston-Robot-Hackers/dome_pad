# dome_pad

The Yellow Pad: an ESP32 touchscreen gamepad for the DOME robot. Hold an
arrow to drive, tap Joke for a robot joke, and watch the robot's ROS topics on
the pad's screen.

```
                  BLE                      DDS (Wi-Fi)
Yellow Pad ─────────────▶ Pi: joy_node ─/joy─┬─▶ teleop_twist_joy ─/cmd_vel─▶ base
   ▲                                         │
   │ Wi-Fi (ROS tab)                         └─▶ VM: pad_buttons ─/voice/llm_text─▶ llm_node
   └──────────────── VM: rosbridge :9090
```

## What's here

| Path | What it is |
|---|---|
| `firmware/` | ESP32 firmware (PlatformIO, Arduino framework). Not built by colcon. |
| `dome_pad/` | Python: `pad_buttons` (button table, press detection) and its ROS node |
| `launch/robot.launch.py` | On the Pi: `joy_node` + `teleop_twist_joy` |
| `launch/remote.launch.py` | On the VM: rosbridge + `pad_buttons` |
| `config/pad.yaml` | joy and teleop parameters, including the full-stick speeds |
| `backup/` | The board's factory firmware image and how to restore it |
| `process/` | DOME process docs; `process/02-doc/notes.md` has the details |

## The pad

Three tabs:

- **Drive:** four arrows and STOP. Holding an arrow drives at that
  direction's speed with the dead-man button held; letting go stops. Three
  keys on the right, **Joke**, **Status** and **C**, are gamepad buttons 2-4.
- **ROS:** the publish rates of `/odom` and `/scan` (placeholders for now),
  through rosbridge on the VM. Wi-Fi is on only while this tab is open.
- **Settings:** speed for each direction, in m/s and rad/s.

## Buttons

`pad_buttons` on the VM turns each press into an action: a question for
dome_voice's `llm_node` (`/voice/llm_text`), or an intent sent straight to
`/intent` for dome_control. The table is `BUTTON_ACTIONS` in
`dome_pad/pad_buttons.py`.

| Pad | `/joy` button | Does |
|---|---|---|
| any arrow | 0 | dead-man button for teleop (never mapped) |
| Joke | 1 | asks Claude for a robot joke; the robot says it |
| Status | 2 | sends the `get_status` intent (no LLM); dome_control, the robot's `c` subsystem, says the speeds |
| C | 3 | nothing yet |

## Running

```bash
# Pi (pad paired over Bluetooth)
bl dome2 robot.launch.py --options "d vo p"

# VM (ANTHROPIC_API_KEY set)
bl dome2 remote.launch.py --options "vo p"
```

Each side can also run alone: `bl dome_pad robot.launch.py`,
`bl dome_pad remote.launch.py`.

## Building

ROS package, on the Pi and the VM:

```bash
cd ~/ros2_ws
rosdep install --from-paths src/dome_pad -y
colcon build --packages-select dome_pad
```

Firmware, from the Mac with the board on USB:

```bash
cd firmware
cp src/secrets.h.sample src/secrets.h   # once: Wi-Fi and the VM's address
uvx --from platformio pio run -t upload
```

## Development

```bash
/usr/bin/python3 -m pytest test/          # Pi or VM
PYTHONPATH=. uvx pytest test/             # Mac, no ROS needed
```

## License

MIT — see [LICENSE](LICENSE)
