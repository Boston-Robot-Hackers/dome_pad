#!/usr/bin/env python3
# conftest — fake ROS 2 modules so the tests run without a ROS install
# Author: Pito Salas and Claude Code
# Version: 1
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
"""Installs minimal fake `rclpy`, `sensor_msgs.msg` and `std_msgs.msg` modules
before any test imports `dome_pad.pad_buttons_node`. The fake `Node` records
what the node creates: its publishers (each a `Mock`) and its subscription
callbacks, keyed by topic, so a test can call a callback and check what was
published. If real rclpy is installed (on the robot or the VM), it's used
instead and only the node tests that need the fake are skipped.
"""

import sys
from dataclasses import dataclass, field
from types import ModuleType
from unittest.mock import Mock


class FakeNode:
    def __init__(self, name: str):
        self.node_name = name
        self.publishers_by_topic: dict[str, Mock] = {}
        self.callbacks_by_topic: dict = {}
        self.logger = Mock()

    def create_publisher(self, msg_type, topic: str, qos_depth: int) -> Mock:
        publisher = Mock()
        self.publishers_by_topic[topic] = publisher
        return publisher

    def create_subscription(self, msg_type, topic: str, callback, qos_depth: int):
        self.callbacks_by_topic[topic] = callback
        return Mock()

    def get_logger(self) -> Mock:
        return self.logger


@dataclass
class FakeJoy:
    buttons: list[int] = field(default_factory=list)


@dataclass
class FakeString:
    data: str = ""


def install_module(name: str, **attributes) -> None:
    module = ModuleType(name)
    for attribute_name, value in attributes.items():
        setattr(module, attribute_name, value)
    sys.modules[name] = module


def install_fake_ros() -> None:
    install_module("rclpy", init=Mock(), spin=Mock(), try_shutdown=Mock(), ok=Mock())
    install_module(
        "rclpy.executors",
        ExternalShutdownException=type("ExternalShutdownException", (Exception,), {}),
    )
    install_module("rclpy.node", Node=FakeNode)
    install_module("sensor_msgs")
    install_module("sensor_msgs.msg", Joy=FakeJoy)
    install_module("std_msgs")
    install_module("std_msgs.msg", String=FakeString)


try:
    import rclpy  # noqa: F401

    HAS_REAL_ROS = True
except ModuleNotFoundError:
    HAS_REAL_ROS = False
    install_fake_ros()
