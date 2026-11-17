---
schema: knowledge/v1
id: the-owner-can-auto-answer-the-bots-questions
kind: knowledge
class: convention
title: "The owner auto-answers the bot's questions with openttd.view.ask.auto, so no 'Your bot asks' dialog interrupts play"
description: "When the bot asks yes/no questions through openttd.view.ask and the owner does not want to click each red dialog."
status: verified
epistemics: observed
date: 2026-11-17
tags: [prismatic-mcp, gui, owner-switch]
about: src/prismatic_mcp/prismatic_mcp_view.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/prismatic_mcp/prismatic_mcp_view.cpp
    - file:tools/research/mcp-e2e
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

## Auto-confirm with a countdown

The owner asked (2026-11-16, session d1269870-428a-4061-aef8-d8ad33da2674, verbatim): "i want
automode so "your bot asks" is auto confirmed in 10, 9, 8 (there should be progress bar; when user
does nothing it is accepted and game continues". So the switch also takes `countdown` (seconds,
1-600; `countdown` alone means `answer=yes`): the question is shown in the fork's own window
(`WindowClass::PrismaticAsk`, laid out like the query dialog) with a bar that empties in real time,
so it also runs while the game is paused. When the bar runs out the switch's answer is recorded and
the window closes; a click on No or Yes answers instead, a click on the bar stops the countdown and
the window waits, and the close box answers no. A process with no main window (a dedicated server,
`openttd_test`) answers at once, as without a countdown.

`tools/research/mcp-e2e` turns this on in the GUI client's `autoexec.scr` on every start
(`mcp view.ask.auto answer=yes countdown=10`); `--ask-countdown N` changes the time and
`--ask-countdown 0` makes every question wait for the owner again.

# Review

Authored from a read of the source at the I0067 head; awaiting the owner's review.
`majordomus knowledge promote the-owner-can-auto-answer-the-bots-questions` with evidence, or
`majordomus knowledge reject the-owner-can-auto-answer-the-bots-questions --reason ...`.

# Evidence

Re-verified against master at 5778b8909c (triage session ff4a0d12, 2026-11-17), read only:
- src/prismatic_mcp/prismatic_mcp_view.cpp:65-67 hold the owner switch (_ask_auto), its countdown (_ask_countdown) and ASK_COUNTDOWN_MAX = 600; :82-90 the countdown widgets and the bar that empties.
- :234 and :281 return no tools on _network_dedicated, so the switch is a GUI-client tool.
The owner's request is quoted verbatim in the record with its session id.
