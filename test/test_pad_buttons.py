#!/usr/bin/env python3
# test_pad_buttons — press detection and the button table
# Author: Pito Salas and Claude Code
# Version: 3
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
import json

import pytest

from dome_pad.pad_buttons import BUTTON_ACTIONS, ButtonPresses, Intent, LlmText

JOKE = LlmText("joke")
STATUS = Intent("get_status")
ACTIONS = {1: JOKE, 3: STATUS}
RELEASED = [0, 0, 0, 0]
A_DOWN = [0, 1, 0, 0]
DRIVING = [1, 0, 0, 0]


@pytest.fixture
def presses():
    return ButtonPresses(ACTIONS)


def test_press_fires_once(presses):
    assert presses.update(RELEASED) == []
    assert presses.update(A_DOWN) == [JOKE]


def test_holding_does_not_repeat(presses):
    presses.update(A_DOWN)
    assert presses.update(A_DOWN) == []
    assert presses.update(A_DOWN) == []


def test_release_then_press_fires_again(presses):
    presses.update(A_DOWN)
    presses.update(RELEASED)
    assert presses.update(A_DOWN) == [JOKE]


def test_button_down_in_first_message_counts_as_press(presses):
    assert presses.update(A_DOWN) == [JOKE]


def test_unmapped_buttons_send_nothing(presses):
    assert presses.update(DRIVING) == []
    assert presses.update([0, 0, 1, 0]) == []


def test_two_new_presses_in_one_message_both_fire_in_index_order(presses):
    assert presses.update([0, 1, 0, 1]) == [JOKE, STATUS]


def test_too_few_buttons_raises_with_the_count(presses):
    with pytest.raises(ValueError, match="has 3 buttons"):
        presses.update([0, 1, 0])


def test_intent_json_is_what_dome_controls_parser_reads():
    payload = json.loads(Intent("get_status").to_json())
    assert payload == {"name": "get_status", "source": "pad", "slots": {}}


def test_joke_key_asks_the_llm_for_a_robot_joke():
    action = BUTTON_ACTIONS[1]
    assert isinstance(action, LlmText)
    assert "robot joke" in action.text


def test_status_key_sends_get_status_without_the_llm():
    assert BUTTON_ACTIONS[2] == Intent("get_status")


def test_dead_man_button_is_never_mapped():
    assert 0 not in BUTTON_ACTIONS
