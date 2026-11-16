---
schema: knowledge/v1
id: a-research-build-and-a-default-build-join-the-same-game
kind: knowledge
class: constraint
title: "A research-instrumented build and a default build of the same commit are network-compatible, so they join one game"
description: "Before running a multiplayer experiment that mixes binaries built with different CMake options."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [network]
about: src/network/core/network_game_info.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/network/core/network_game_info.cpp
    - file:src/network/network_server.cpp
    - file:cmake/Options.cmake
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/network/README.md
---

# Statement

`src/network/core/network_game_info.cpp:IsNetworkCompatibleVersion` compares revision strings
(and the `dbg_seed`/`dbg_sync` prefixes); CMake options such as `OPTION_RESEARCH_INSTRUMENTATION`
or `OPTION_PRISMATIC_MCP_SERVER` are not part of the revision. A mixed pair therefore joins, and a
gated hunk that ever touched simulation state would desync them with no version error. Keep every
participant of a multiplayer experiment on the same profile, and record it in the manifest.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote a-research-build-and-a-default-build-join-the-same-game` with evidence, or `majordomus knowledge reject a-research-build-and-a-default-build-join-the-same-game --reason ...`.
