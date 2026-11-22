#!/usr/bin/env python3
# test_pad_buttons — press detection and the button table
# Author: Pito Salas and Claude Code
# Version: 1
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
import pytest

from dome_pad.pad_buttons import BUTTON_TEXTS, ButtonPresses

TEXTS = {1: "joke", 3: "status"}
RELEASED = [0, 0, 0, 0]
A_DOWN = [0, 1, 0, 0]
DRIVING = [1, 0, 0, 0]


@pytest.fixture
def presses():
    return ButtonPresses(TEXTS)


def test_press_fires_once(presses):
    assert presses.update(RELEASED) == []
    assert presses.update(A_DOWN) == ["joke"]


def test_holding_does_not_repeat(presses):
    presses.update(A_DOWN)
    assert presses.update(A_DOWN) == []
    assert presses.update(A_DOWN) == []


def test_release_then_press_fires_again(presses):
    presses.update(A_DOWN)
    presses.update(RELEASED)
    assert presses.update(A_DOWN) == ["joke"]


def test_button_down_in_first_message_counts_as_press(presses):
    assert presses.update(A_DOWN) == ["joke"]


def test_unmapped_buttons_send_nothing(presses):
    assert presses.update(DRIVING) == []
    assert presses.update([0, 0, 1, 0]) == []


def test_two_new_presses_in_one_message_both_fire_in_index_order(presses):
    assert presses.update([0, 1, 0, 1]) == ["joke", "status"]


def test_too_few_buttons_raises_with_the_count(presses):
    with pytest.raises(ValueError, match="has 3 buttons"):
        presses.update([0, 1, 0])


def test_button_a_asks_for_a_robot_joke():
    assert "robot joke" in BUTTON_TEXTS[1]


def test_dead_man_button_is_never_mapped():
    assert 0 not in BUTTON_TEXTS
