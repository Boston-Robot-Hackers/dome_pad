#!/usr/bin/env python3
# pad_buttons_node — ROS node: pad button presses on /joy → text on /voice/llm_text
# Author: Pito Salas and Claude Code
# Version: 1
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
"""Runs on the remote computer. Subscribes to `/joy` (`sensor_msgs/Joy`), which
`joy_node` on the robot publishes from the pad, and publishes each new press
of a mapped button as its text on `/voice/llm_text` (`std_msgs/String`).
dome_voice's `llm_node` answers it, and the robot speaks the reply. The
button table and press detection are in `dome_pad.pad_buttons`.

    bl dome_pad remote.launch.py
"""

import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from sensor_msgs.msg import Joy
from std_msgs.msg import String

from dome_pad.pad_buttons import BUTTON_TEXTS, ButtonPresses

LLM_TEXT_TOPIC = "/voice/llm_text"


class PadButtonsNode(Node):
    def __init__(self):
        super().__init__("pad_buttons")
        self.presses = ButtonPresses(BUTTON_TEXTS)
        self.text_pub = self.create_publisher(String, LLM_TEXT_TOPIC, 10)
        self.create_subscription(Joy, "/joy", self.on_joy, 10)
        self.get_logger().info(f"Ready: buttons {BUTTON_TEXTS}")

    def on_joy(self, msg: Joy) -> None:
        for text in self.presses.update(msg.buttons):
            self.get_logger().info(f"Button pressed, sending {text!r}")
            self.text_pub.publish(String(data=text))


def main() -> None:
    rclpy.init()
    node = PadButtonsNode()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        node.get_logger().info("Stopped")
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == "__main__":
    main()
