---
schema: knowledge/v1
id: a-build-from-a-modified-tree-cannot-join-a-clean-build
kind: knowledge
class: constraint
title: "A binary built from a modified work tree carries -m<hash> and is refused by a server of the same commit built clean (-g<hash>)"
description: "Before a multiplayer experiment, or when a client fails the version check against a server built from the same commit."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [build-and-packaging, network]
about: cmake/scripts/FindVersion.cmake
route: platform
provenance:
  origin: authored
  derived_from:
    - file:cmake/scripts/FindVersion.cmake
    - file:src/network/core/network_game_info.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/build-and-packaging/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/network/README.md
---

# Statement

The `find_version` target runs `cmake/scripts/FindVersion.cmake` on every build; when
`git diff-index HEAD` prints anything it sets `REV_MODIFIED 2`, and
`src/network/core/network_game_info.cpp:GetNetworkRevisionString` appends `-m<hash>` instead of
`-g<hash>`. For untagged builds `IsNetworkCompatibleVersion` compares the text after the last
`-`, marker letter included, so a client built from a dirty tree and a server built clean from the
same commit refuse each other. Build every participant from the same tree state (all committed,
or all with the same local diff) and record the revision string in the experiment manifest.
Complements `a-research-build-and-a-default-build-join-the-same-game`: CMake options do not
change the string, the tree state does.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0063); awaiting the owner's review.
`majordomus knowledge promote a-build-from-a-modified-tree-cannot-join-a-clean-build` with evidence, or `majordomus knowledge reject a-build-from-a-modified-tree-cannot-join-a-clean-build --reason ...`.
