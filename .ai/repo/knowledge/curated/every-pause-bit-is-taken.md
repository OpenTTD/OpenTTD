---
schema: knowledge/v1
id: every-pause-bit-is-taken
kind: knowledge
class: constraint
title: "PauseModes is an 8-bit set whose bits are all used and it is saved as one byte, so a new pause reason needs a wider, version-gated field"
description: "Before adding a pause reason (for example a research or MCP pause)."
status: verified
epistemics: observed
date: 2026-11-17
tags: [core-loop, saveload]
about: src/openttd.h
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/openttd.h
    - file:src/saveload/misc_sl.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/core-loop/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/saveload/README.md
---

# Statement

`src/openttd.h:PauseModes` is `EnumBitSet<PauseMode, uint8_t>` and `src/openttd.h:PauseMode` uses all
eight bits; `src/saveload/misc_sl.cpp` saves the mode as a byte. Rule
`project.savegame-fields-are-version-gated` applies to the widening.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote every-pause-bit-is-taken` with evidence, or `majordomus knowledge reject every-pause-bit-is-taken --reason ...`.

# Evidence

Re-verified against master at 5778b8909c (triage session ff4a0d12, 2026-11-17), read only:
- src/openttd.h:71-83 enum class PauseMode : uint8_t has values 0..7 (Normal .. CommandDuringPause), and PauseModes = EnumBitSet<PauseMode, uint8_t>: all eight bits used.
- src/saveload/misc_sl.cpp:105 saves _pause_mode as VarFileType::U8.
