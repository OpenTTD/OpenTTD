---
schema: knowledge/v1
id: the-desync-check-compares-only-the-game-random-state
kind: knowledge
class: fact
title: "Multiplayer desync detection compares only _random.state[0] at sync frames; a divergence that never feeds Random() goes unseen"
description: "When using 'no desync reported' as evidence that a hook or experiment left clients in step."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [network]
about: src/network/network.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/network/network.cpp
    - file:src/network/network_client.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/network/README.md
---

# Statement

`src/network/network.cpp:NetworkGameLoop` stores `_sync_seed_1 = _random.state[0]` and the client
compares it at `_sync_frame` (`src/network/network_client.cpp`). A second seed is sent only when
`NETWORK_SEND_DOUBLE_SEED` is defined. State that diverges without changing the random stream, such as
a cached value or a counter, is not detected until it changes a later random draw. For research
hooks, a quiet desync check proves only that the random streams agree.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote the-desync-check-compares-only-the-game-random-state` with evidence, or `majordomus knowledge reject the-desync-check-compares-only-the-game-random-state --reason ...`.
