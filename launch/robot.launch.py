#!/usr/bin/env python3
# robot.launch.py — dome_pad's driving nodes on the robot: joy and teleop_twist_joy
# Author: Pito Salas and Claude Code
# Version: 1
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
#
# Usage:
#   bl dome_pad robot.launch.py                  # this package alone
#   bl dome2 robot.launch.py --options "d p"     # via dome2's "pad" subsystem
"""Start `joy_node` and `teleop_twist_joy` for the Yellow Pad.

The pad pairs with the Pi over Bluetooth, so these run on the robot: driving
and the dead-man button don't depend on the remote computer. `joy_node`
publishes `/joy`; `teleop_twist_joy` turns it into `/cmd_vel`, only while
button 0 is held. Both read `config/pad.yaml`, which has `deadzone: 0.0`:
the stock teleop launch's 0.3 deadzone would rescale the pad's exact values.
Per stack/launch_conventions.md this file starts only outside packages'
nodes; dome2 combines it with the rest.
"""

from better_launch import BetterLaunch, launch_this


@launch_this(ui=True)
def robot_launch():
    """Start joy_node and teleop_twist_joy with config/pad.yaml. Takes no arguments."""
    bl = BetterLaunch()
    config = bl.find("dome_pad", "pad.yaml")
    bl.node("joy", "joy_node", "joy_node", param_files=[config])
    bl.node(
        "teleop_twist_joy", "teleop_node", "teleop_twist_joy_node", param_files=[config]
    )
