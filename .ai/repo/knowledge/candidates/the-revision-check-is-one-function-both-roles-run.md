---
schema: knowledge/v1
id: the-revision-check-is-one-function-both-roles-run
kind: knowledge
class: fact
title: "The network revision check is one function both roles run, so a local override is local: a stock server still rejects a mismatched client"
description: "When reasoning about connecting a research build across a revision mismatch, or about what the research_net_compat override can and cannot do."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [network, research, determinism]
about: src/network/core/network_game_info.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/network/core/network_game_info.cpp
    - file:src/network/network_server.cpp
    - file:src/network/network_client.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/adrs/0029-a-research-gated-local-only-override-accepts-a-mismatched-pee.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/network/README.md
  - type: supports
    target: file:.ai/repo/rules/project/research-net-compat-is-local-and-non-spoofing.v1.md
---

# Statement

`src/network/core/network_game_info.cpp:IsNetworkCompatibleVersion` is the one function both roles
run: the server calls it on the client's revision at join
(`src/network/network_server.cpp:ServerNetworkGameSocketHandler`, kicking with
`NetworkErrorCode::WrongRevision`), and the client marks a server incompatible through
`CheckGameCompatibility` before connecting. The client sends its own true revision
(`src/network/network_client.cpp` sends `GetNetworkRevisionString()`); it never sends what the
server reports back.

Two consequences. A single process acting as both the loopback server and the client is covered by
widening that one function (the fork's `research_net_compat` override, ADR 0029). And because the
server runs the check independently on the client's sent revision, a client-side override is
**local only**: a stock, non-consenting server still rejects a mismatched client. Making a stock
server accept a mismatched client would require changing the revision the client *sends* — spoofing
— which this fork forbids (`project.research-net-compat-is-local-and-non-spoofing`).

# Review

Authored from a read of the source at the I0070 head (issue I0070); awaiting the owner's review.
`majordomus knowledge promote the-revision-check-is-one-function-both-roles-run` with evidence, or
`majordomus knowledge reject the-revision-check-is-one-function-both-roles-run --reason ...`.
