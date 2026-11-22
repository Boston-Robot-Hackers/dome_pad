- [x] Renamed the action keys A and B to "Joke" and "Status" (`ACTION_LABELS`; C
      unchanged), and mapped Status (`/joy` button 2) to "What's your status?"
      for the LLM, so it calls `get_status` and dome_control says the speeds
      (needs the robot's `c` subsystem). Test added; docs and comments
      updated. (2026-11-22)
- [x] Status skips the LLM: `BUTTON_TEXTS` became `BUTTON_ACTIONS`, with
      `LlmText` (question on `/voice/llm_text`) and `Intent` (JSON on
      `/intent`, source `pad`) actions; Status is `Intent("get_status")`.
      Why: after repeated identical status questions, Claude decided the
      robot wasn't answering (it never sees dome_control's spoken reply),
      stopped sending the intent and apologised instead. Tests for the
      intent JSON and the node's `/intent` publish. (2026-11-22)
