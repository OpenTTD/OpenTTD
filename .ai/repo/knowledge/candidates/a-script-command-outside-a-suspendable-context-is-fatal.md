---
schema: knowledge/v1
id: a-script-command-outside-a-suspendable-context-is-fatal
kind: knowledge
class: constraint
title: "A script that issues a command from its constructor, Save, Load or a valuator dies with a fatal error"
description: "When generating or bridging an AI or Game Script (PrismaticAI and similar)."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [scripting]
about: src/script/api/script_object.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/script/api/script_object.cpp
    - file:src/script/script_instance.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/scripting/README.md
---

# Statement

`src/script/api/script_object.cpp:ScriptObject::DoCommandPrep` throws `Script_FatalError` when the
instance cannot suspend, and the instance wraps the constructor and load in a scope that disables
commands (`src/script/script_instance.cpp`). Issue commands only from `Start()` and what it calls.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote a-script-command-outside-a-suspendable-context-is-fatal` with evidence, or `majordomus knowledge reject a-script-command-outside-a-suspendable-context-is-fatal --reason ...`.
