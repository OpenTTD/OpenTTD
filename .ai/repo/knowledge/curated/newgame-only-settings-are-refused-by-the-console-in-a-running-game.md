---
schema: knowledge/v1
id: newgame-only-settings-are-refused-by-the-console-in-a-running-game
kind: knowledge
class: lesson
title: "A NewgameOnly setting is refused by the console in a running game: put it in openttd.cfg before the world is made, and read it back"
description: "When a tool (panel, duel, test stack) changes a game setting with the console's `setting` after the server started, or when a measurement depends on a server setting."
status: verified
epistemics: observed
date: 2026-11-17
tags: [settings, console, panel, duel, scripts, measurement]
about: tools/research/mcp-rhai/src/panel.rs
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/settings_internal.h
    - file:src/settings.cpp
    - file:src/table/settings/script_settings.ini
    - file:tools/research/mcp-rhai/src/panel.rs
    - file:research/results/duel-20261117T112500-seed31337.md
---

# Lesson

Settings flagged `SettingFlag::NewgameOnly` (`src/settings_internal.h`: "This setting cannot be changed in
a game") are refused once a game runs (`src/settings.cpp`). The console's `setting <name> <value>` sent
then does not take, and nothing about it shows up as an error in the tool that sent it.

`script.script_max_opcode_till_suspend` is one of them (`src/table/settings/script_settings.ini`). The
panel set it to 250000 with the console after the world was made, so from the first panel until
2026-11-17 every game ran its AIs at the default 10000 opcodes a tick. The bot plans rail paths in
Squirrel, so it built one line every two to three game months while its money waited. Once the
budget was written into `openttd.cfg` before the world was made (panel `SCRIPT_OPCODES`, 1d8c45fa77),
the same bot and plan on seed 31337 went from 597075 to 995647 company value in 36 months
(duel-20261117T112500).

# How to apply

- Put a server setting a tool depends on into the `openttd.cfg` the tool writes before the server starts
  (section = the part before the dot, key = the rest: `[script]` `script_max_opcode_till_suspend`).
- Read it back after start (`setting <name>` without a value prints "Current value for ... is ...") and
  log the value, so a run records the conditions it actually had.
- Before trusting a measurement that depends on a setting, check what the game reports, not what the
  tool intended.
