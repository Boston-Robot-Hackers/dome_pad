#!/usr/bin/env python3
# remote.launch.py — dome_pad on the remote computer: rosbridge and pad_buttons
# Author: Pito Salas and Claude Code
# Version: 1
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
#
# Usage:
#   bl dome_pad remote.launch.py                     # this package alone
#   bl dome2 remote.launch.py --options "vo p"       # with dome_voice's LLM node
"""Start rosbridge and `pad_buttons` on the remote computer (the VM).

- **rosbridge** (`rosbridge_server`, port 9090) feeds the pad's ROS tab. The
  firmware's `ROSBRIDGE_HOST` must be this computer's address.
- **`pad_buttons`** turns pad button presses on `/joy` into text on
  `/voice/llm_text`, which dome_voice's `llm_node` answers (A asks for a
  robot joke). Without `llm_node` running, presses go unanswered.

Per stack/launch_conventions.md this file starts only dome_pad's own node and
an outside package (rosbridge); dome2 combines it with dome_voice.
"""

from better_launch import BetterLaunch, launch_this


@launch_this(ui=True)
def remote_launch():
    """Start rosbridge_websocket and pad_buttons. Takes no arguments."""
    bl = BetterLaunch()
    bl.include("rosbridge_server", "rosbridge_websocket_launch.xml")
    bl.node("dome_pad", "pad_buttons", "pad_buttons")
