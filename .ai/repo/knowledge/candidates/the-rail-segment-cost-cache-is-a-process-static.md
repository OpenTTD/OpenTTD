---
schema: knowledge/v1
id: the-rail-segment-cost-cache-is-a-process-static
kind: knowledge
class: constraint
title: "The YAPF rail segment cost cache is unsaved process state, flushed only through YapfNotifyTrackLayoutChange"
description: "Before adding any command or hook that changes rail layout or signals."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [vehicles-and-pathfinding]
about: src/pathfinder/yapf/yapf_costcache.hpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/pathfinder/yapf/yapf_costcache.hpp
    - file:src/pathfinder/yapf/yapf_rail.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/vehicles-and-pathfinding/README.md
---

# Statement

The global cache in `src/pathfinder/yapf/yapf_costcache.hpp` is flushed when the rail change counter
moves, which the layout-changing commands do by calling `YapfNotifyTrackLayoutChange`. A new path that
changes track without it leaves the host with stale costs that a client joining later does not have
(the desync consequence is inferred).

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote the-rail-segment-cost-cache-is-a-process-static` with evidence, or `majordomus knowledge reject the-rail-segment-cost-cache-is-a-process-static --reason ...`.
