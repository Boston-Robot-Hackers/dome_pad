#!/usr/bin/env python3
# pad_buttons — ROS-free core of pad_buttons: which pad buttons send which text
# Author: Pito Salas and Claude Code
# Version: 1
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
"""`BUTTON_TEXTS` maps a `/joy` button index to the text it sends to the LLM.

The pad's gamepad buttons 1-4 arrive as `/joy` buttons 0-3. Button 0 is the
dead-man button that every arrow holds, so it is never mapped here. A, B and
C are buttons 1, 2 and 3.

`ButtonPresses.update` takes each `/joy` button array and returns the texts
for mapped buttons that went from released to pressed since the last call.
**A press fires once**: holding a button sends nothing more, and a button
already down in the first message counts as a press.
"""

from collections.abc import Sequence

BUTTON_TEXTS = {
    1: "Tell me a robot joke.",  # A
}


class ButtonPresses:
    """Tracks which mapped buttons are down, to report each new press once."""

    def __init__(self, button_texts: dict[int, str]):
        self.button_texts = button_texts
        self.button_count = max(button_texts) + 1
        self.held_buttons: set[int] = set()

    def update(self, buttons: Sequence[int]) -> list[str]:
        if len(buttons) < self.button_count:
            raise ValueError(
                f"/joy has {len(buttons)} buttons; the mapped buttons need "
                f"{self.button_count}"
            )
        down = {index for index in self.button_texts if buttons[index]}
        new_presses = sorted(down - self.held_buttons)
        self.held_buttons = down
        return [self.button_texts[index] for index in new_presses]
