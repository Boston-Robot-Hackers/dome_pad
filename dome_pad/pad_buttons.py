#!/usr/bin/env python3
# pad_buttons — ROS-free core of pad_buttons: which pad buttons do what
# Author: Pito Salas and Claude Code
# Version: 3
# Created: 2026-11-22
# Updated: 2026-11-22
# Open Source Under MIT license
"""`BUTTON_ACTIONS` maps a `/joy` button index to what a press does.

The pad's gamepad buttons 1-4 arrive as `/joy` buttons 0-3. Button 0 is the
dead-man button that every arrow holds, so it is never mapped here. The keys
labelled Joke, Status and C (`ACTION_LABELS` in the firmware) are buttons 1,
2 and 3.

Two kinds of action:
- **`LlmText`**: a question for dome_voice's `llm_node`, on `/voice/llm_text`.
  For anything conversational.
- **`Intent`**: an intent published straight to `/intent`, skipping the LLM.
  For commands. Status is one: through the LLM, repeated identical status
  questions made Claude conclude the robot wasn't answering (it never sees
  dome_control's spoken reply) and stop sending the intent.

`ButtonPresses.update` takes each `/joy` button array and returns the actions
for mapped buttons that went from released to pressed since the last call.
**A press fires once**: holding a button sends nothing more, and a button
already down in the first message counts as a press.
"""

import json
from collections.abc import Sequence
from dataclasses import dataclass

INTENT_SOURCE = "pad"


@dataclass(frozen=True)
class LlmText:
    text: str


@dataclass(frozen=True)
class Intent:
    name: str

    def to_json(self) -> str:
        """The `/intent` payload, in the format dome_control's IntentParser reads."""
        return json.dumps({"name": self.name, "source": INTENT_SOURCE, "slots": {}})


BUTTON_ACTIONS: dict[int, LlmText | Intent] = {
    1: LlmText("Tell me a robot joke."),  # Joke
    2: Intent("get_status"),  # Status: dome_control says the speeds
}


class ButtonPresses:
    """Tracks which mapped buttons are down, to report each new press once."""

    def __init__(self, button_actions: dict[int, LlmText | Intent]):
        self.button_actions = button_actions
        self.button_count = max(button_actions) + 1
        self.held_buttons: set[int] = set()

    def update(self, buttons: Sequence[int]) -> list[LlmText | Intent]:
        if len(buttons) < self.button_count:
            raise ValueError(
                f"/joy has {len(buttons)} buttons; the mapped buttons need "
                f"{self.button_count}"
            )
        down = {index for index in self.button_actions if buttons[index]}
        new_presses = sorted(down - self.held_buttons)
        self.held_buttons = down
        return [self.button_actions[index] for index in new_presses]
