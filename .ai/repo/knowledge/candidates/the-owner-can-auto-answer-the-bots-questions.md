---
schema: knowledge/v1
id: the-owner-can-auto-answer-the-bots-questions
kind: knowledge
class: convention
title: "The owner auto-answers the bot's questions with openttd.view.ask.auto, so no 'Your bot asks' dialog interrupts play"
description: "When the bot asks yes/no questions through openttd.view.ask and the owner does not want to click each red dialog."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [prismatic-mcp, gui, owner-switch]
about: src/prismatic_mcp/prismatic_mcp_view.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/prismatic_mcp/prismatic_mcp_view.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/prismatic-mcp/README.md
  - type: relates_to
    target: file:.ai/repo/adrs/0014-interface-input-from-an-agent-is-armed-per-process-by-the-ow.md
---

# Statement

`openttd.view.ask` shows the bot's yes/no question as the game's own query dialog ("Your bot
asks"). `src/prismatic_mcp/prismatic_mcp_view.cpp` adds an owner switch, `openttd.view.ask.auto`:
with an answer (`yes`, `no`, or any text; `on`/`off`/`true`/`false`/`1`/`0` normalise to yes/no), a
later `openttd.view.ask` records that answer and shows **no** dialog; omit `answer`, or pass
`null`, to turn it off and show the dialog again. Default off.

Because every tool is derived into the surfaces, the switch is reached three ways with no extra
code: the tilde console (`mcp view.ask.auto answer=yes`, through the `mcp` command of I0067),
Swagger/OpenAPI, and the `.ai` tool catalogue. It is a GUI-client tool: `View::Tools()` lists
nothing on a dedicated server (`_network_dedicated`), which is also where no dialog is ever shown.
The switch sets a per-client preference only; it issues no command and changes no game state, so it
is read before the viewport guard and works even where there is no viewport.

# Review

Authored from a read of the source at the I0067 head; awaiting the owner's review.
`majordomus knowledge promote the-owner-can-auto-answer-the-bots-questions` with evidence, or
`majordomus knowledge reject the-owner-can-auto-answer-the-bots-questions --reason ...`.
