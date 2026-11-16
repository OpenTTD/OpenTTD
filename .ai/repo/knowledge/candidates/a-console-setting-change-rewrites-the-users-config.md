---
schema: knowledge/v1
id: a-console-setting-change-rewrites-the-users-config
kind: knowledge
class: constraint
title: "Changing a setting from the console in a research run rewrites the user's openttd.cfg unless the run was started with -x"
description: "Before scripting setting changes in an experiment, the bridge or an MCP flow."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [settings]
about: src/settings.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/settings.cpp
    - file:src/openttd.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/settings/README.md
---

# Statement

`src/settings.cpp:IntSettingDesc::ChangeValue` and the string variant call `src/settings.cpp:SaveToConfig`
whenever `_save_config` is set; `-x` clears it (`src/openttd.cpp`). An experiment that must not alter the
machine starts the game with `-x`.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote a-console-setting-change-rewrites-the-users-config` with evidence, or `majordomus knowledge reject a-console-setting-change-rewrites-the-users-config --reason ...`.
