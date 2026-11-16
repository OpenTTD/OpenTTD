---
id: project.research-net-compat-is-local-and-non-spoofing
version: 1
kind: rule
title: The research network-compatibility override is local-only and never spoofs the build's revision
description: The override that accepts a mismatched peer revision for a loopback R&D game may only widen acceptance on a game the owner runs. It must never change the revision this build reports to a peer, so it cannot impersonate another build or defeat a non-consenting server's own check.
statement: The research override of the network revision check may only make IsNetworkCompatibleVersion accept more peers; it must never alter GetNetworkRevisionString or the revision this build sends, must stay compile-gated and default-off, and must never be used to connect to a server the owner does not run.
status: active
class: blocking
depends_on:
  - project.research-code-is-gated-and-labelled@1
  - project.no-upstream-interaction@1
tags: [research, network, honesty, containment]
---

# Rationale

The deterministic-lockstep revision check (ADR 0016) refuses a client whose revision differs from
the server's. A loopback R&D game of this fork trips it when the two ends are the same code built
from a dirty and a clean tree (ADR 0029). Widening acceptance on a game the owner runs is their own
choice and harms no one: a stock remote server still runs its own check.

Spoofing is the opposite act and is forbidden. Making this build report a revision it is not would
let a modified or bot-driven client pass a *non-consenting* server's check while concealing what it
is — deceiving a third party and their players. That is never in scope for this fork, whatever the
stated purpose.

# Required behaviour

- The override compiles only under `OPTION_RESEARCH_INSTRUMENTATION` and defaults to off
  (`project.research-code-is-gated-and-labelled`).
- It only widens `src/network/core/network_game_info.cpp:IsNetworkCompatibleVersion` to accept more
  peers. It must not read, build, or alter `GetNetworkRevisionString()` or the revision packet the
  client or server sends.
- It is documented as local-only and non-spoofing (`research/net-compat.md`, ADR 0029), and the
  unit test pins that with the override off the build still matches only its own revision.

# Failure behaviour

A change that makes the override touch the sent revision, drop the compile gate, default it on, or
present a forged revision to a peer violates this rule and is refused. The honest alternative is to
rebuild both ends from one clean commit
(knowledge `a-build-from-a-modified-tree-cannot-join-a-clean-build`).

# Related

ADR 0029, ADR 0016, `project.research-code-is-gated-and-labelled`, `project.no-upstream-interaction`.
