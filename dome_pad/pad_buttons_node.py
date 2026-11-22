#!/usr/bin/env python3
# pad_buttons_node — ROS node: pad button presses on /joy → LLM questions or intents
# Author: Pito Salas and Claude Code
# Version: 2
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
"""Runs on the remote computer. Subscribes to `/joy` (`sensor_msgs/Joy`), which
`joy_node` on the robot publishes from the pad, and turns each new press of a
mapped button into its action:
- `LlmText`: the text on `/voice/llm_text` (`std_msgs/String`), which
  dome_voice's `llm_node` answers; the robot speaks the reply.
- `Intent`: the intent JSON on `/intent` (`std_msgs/String`), which
  dome_control handles directly.

The button table and press detection are in `dome_pad.pad_buttons`.

    bl dome_pad remote.launch.py
"""

import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from sensor_msgs.msg import Joy
from std_msgs.msg import String

from dome_pad.pad_buttons import BUTTON_ACTIONS, ButtonPresses, Intent, LlmText

LLM_TEXT_TOPIC = "/voice/llm_text"
INTENT_TOPIC = "/intent"


class PadButtonsNode(Node):
    def __init__(self):
        super().__init__("pad_buttons")
        self.presses = ButtonPresses(BUTTON_ACTIONS)
        self.text_pub = self.create_publisher(String, LLM_TEXT_TOPIC, 10)
        self.intent_pub = self.create_publisher(String, INTENT_TOPIC, 10)
        self.create_subscription(Joy, "/joy", self.on_joy, 10)
        self.get_logger().info(f"Ready: buttons {BUTTON_ACTIONS}")

    def on_joy(self, msg: Joy) -> None:
        for action in self.presses.update(msg.buttons):
            self.get_logger().info(f"Button pressed: {action}")
            self.publish(action)

    def publish(self, action: LlmText | Intent) -> None:
        if isinstance(action, LlmText):
            self.text_pub.publish(String(data=action.text))
        else:
            self.intent_pub.publish(String(data=action.to_json()))


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
