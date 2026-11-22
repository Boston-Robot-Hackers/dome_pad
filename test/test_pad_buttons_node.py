#!/usr/bin/env python3
# test_pad_buttons_node — the node publishes each press on /voice/llm_text
# Author: Pito Salas and Claude Code
# Version: 2
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
"""Uses conftest's fake rclpy, so it runs without a ROS graph. Skipped where
real rclpy is installed, because the fake node is what records publishers."""

from unittest.mock import Mock

import pytest
from conftest import HAS_REAL_ROS
from rclpy.executors import ExternalShutdownException

from dome_pad import pad_buttons_node
from dome_pad.pad_buttons import BUTTON_ACTIONS

pytestmark = pytest.mark.skipif(HAS_REAL_ROS, reason="needs conftest's fake rclpy")

JOKE_DOWN = [0, 1, 0, 0]
STATUS_DOWN = [0, 0, 1, 0]


@pytest.fixture
def node():
    return pad_buttons_node.PadButtonsNode()


def send_joy(node, buttons: list[int]) -> None:
    joy = pad_buttons_node.Joy(buttons=buttons)
    node.callbacks_by_topic["/joy"](joy)


def test_joke_press_publishes_its_text_once(node):
    send_joy(node, JOKE_DOWN)
    send_joy(node, JOKE_DOWN)
    publisher = node.publishers_by_topic["/voice/llm_text"]
    publisher.publish.assert_called_once_with(
        pad_buttons_node.String(data=BUTTON_ACTIONS[1].text)
    )
    node.publishers_by_topic["/intent"].publish.assert_not_called()


def test_status_press_publishes_the_intent_not_llm_text(node):
    send_joy(node, STATUS_DOWN)
    node.publishers_by_topic["/intent"].publish.assert_called_once_with(
        pad_buttons_node.String(data=BUTTON_ACTIONS[2].to_json())
    )
    node.publishers_by_topic["/voice/llm_text"].publish.assert_not_called()


def test_no_press_publishes_nothing(node):
    send_joy(node, [1, 0, 0, 0])
    node.publishers_by_topic["/voice/llm_text"].publish.assert_not_called()
    node.publishers_by_topic["/intent"].publish.assert_not_called()


@pytest.mark.parametrize("stop", [ExternalShutdownException, KeyboardInterrupt])
def test_main_exits_cleanly_on_stop(monkeypatch, stop):
    node = Mock()
    monkeypatch.setattr(pad_buttons_node, "PadButtonsNode", Mock(return_value=node))
    monkeypatch.setattr(pad_buttons_node.rclpy, "spin", Mock(side_effect=stop))
    try_shutdown = Mock()
    monkeypatch.setattr(pad_buttons_node.rclpy, "try_shutdown", try_shutdown)

    pad_buttons_node.main()

    node.destroy_node.assert_called_once()
    try_shutdown.assert_called_once()
