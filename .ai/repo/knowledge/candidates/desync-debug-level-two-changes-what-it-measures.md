---
schema: knowledge/v1
id: desync-debug-level-two-changes-what-it-measures
kind: knowledge
class: lesson
title: "-d desync=2 recomputes caches every tick and runs the rail pathfinder twice, so never time an experiment with it on"
description: "When choosing debug flags for a measured run; record them as an observer effect."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [network, vehicles-and-pathfinding]
about: src/cachecheck.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/cachecheck.cpp
    - file:src/pathfinder/yapf/yapf_rail.cpp
    - file:src/command.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/vehicles-and-pathfinding/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/network/README.md
---

# Statement

`src/cachecheck.cpp:CheckCaches` returns early below desync level 2; at that level it rebuilds and
compares caches each tick, and the rail pathfinder repeats each choice with caching disabled. Level 1
already logs every executed command to `commands-out.log` and writes `dmp_cmds` saves
(`src/command.cpp`), which `docs/desync.md` places at higher levels.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote desync-debug-level-two-changes-what-it-measures` with evidence, or `majordomus knowledge reject desync-debug-level-two-changes-what-it-measures --reason ...`.
