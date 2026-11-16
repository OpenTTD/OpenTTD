---
schema: knowledge/v1
id: scripts-run-only-on-the-server-and-the-game-script-runs-while-paused
kind: knowledge
class: fact
title: "AIs and the Game Script run only on the server, and the Game Script keeps ticking while the game is paused"
description: "Before designing a bridge, MCP or experiment flow that pauses the game or expects script activity on clients."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [core-loop, scripting]
about: src/openttd.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/ai/ai_core.cpp
    - file:src/game/game_core.cpp
    - file:src/openttd.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/scripting/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/core-loop/README.md
---

# Statement

`src/ai/ai_core.cpp:AI::GameLoop` returns on clients (and when AIs in multiplayer are off), as does
`src/game/game_core.cpp:Game::GameLoop`. `src/openttd.cpp:StateGameLoop` still calls the Game Script in
its paused branch but not the AIs. A paused game therefore still has Game Script activity and no
AI activity.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote scripts-run-only-on-the-server-and-the-game-script-runs-while-paused` with evidence, or `majordomus knowledge reject scripts-run-only-on-the-server-and-the-game-script-runs-while-paused --reason ...`.
