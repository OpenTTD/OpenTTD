---
schema: knowledge/v1
id: widget-enums-are-part-of-the-script-api
kind: knowledge
class: constraint
title: "The widget enums in src/widgets/*_widget.h are generated into the ScriptWindow API, so widgets are appended, never inserted"
description: "Before adding, renaming or reordering a widget."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [gui-and-console, scripting]
about: src/script/api/script_window.hpp.in
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/script/api/script_window.hpp.in
    - file:src/script/api/CMakeLists.txt
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/gui-and-console/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/scripting/README.md
---

# Statement

`src/script/api/script_window.hpp.in` is generated from the widget headers by the rule in
`src/script/api/CMakeLists.txt`. Renumbering a widget changes the AI and Game Script API.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote widget-enums-are-part-of-the-script-api` with evidence, or `majordomus knowledge reject widget-enums-are-part-of-the-script-api --reason ...`.
