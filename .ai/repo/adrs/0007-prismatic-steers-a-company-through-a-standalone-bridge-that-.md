---
schema: adr/v1
id: adr-0007
kind: adr
title: Prismatic steers a company through a standalone bridge that drives a Company AI over rcon, not through a Game Script relay
status: proposed
date: 2026-07-24
tags:
  - bridge
  - prismatic
  - integration
related:
  - file:research/prismatic-bridge/ARCHITECTURE.md
  - file:docs/admin_network.md
  - file:.ai/repo/adrs/0008-an-agent-reaches-the-game-through-an-embedded-loopback-mcp-s.md
provenance:
  origin: extracted
  derived_from:
    - commit:505fb87d7a441500b785e3b8a84f88b1152928df
    - commit:1e5ad23af931f4e5934e78eceb08f45d3c943e71
    - commit:868b5d9269b70f8570ecc7e2f18a12bf52568d7d
    - commit:68d15d4ee31b582821b4ab9abc927ea1c1da0fbb
    - file:research/prismatic-bridge/ARCHITECTURE.md
---

# 7. Prismatic steers a company through a standalone bridge that drives a Company AI over rcon, not through a Game Script relay

## Context

Prismatic needs to steer a company in a running game and observe the result. The first
design proposed a Game Script JSON relay over the admin network (`ScriptAdmin.Send`,
`ScriptCompanyMode`) with an Elixir/Rustler client inside `prismatic-platform`. Mid-session it
turned out that `~/dev/openttd-prismatic-bridge` already existed: a working, tested (8/8)
Python/FastAPI service.

## Decision

The bridge is three independent pieces: OpenTTD, the standalone bridge service, and
`prismatic-platform`. Control is coarse-grained: the bridge uses the admin network's rcon to
call `start_ai`/`reload_ai` with a settings string that a normal Company AI
(`ai/PrismaticAI`) reads through `AIController.GetSetting()`. Prismatic talks plain HTTP to
the bridge and never speaks the binary admin protocol.

The design document is corrected to describe the built system rather than the earlier
hypothesis, and the Game Script relay stays documented as the upgrade path for finer control.

## Alternatives rejected

- **Rewriting the working bridge to match the earlier design.** A large, unrequested change
  to tested code.
- **Rustler inside the Python service.** Rustler binds to the BEAM, not Python. The
  Rust/PyO3/Elixir options are left as an explicit open decision (issue I0012).

## Consequences

- Phase 1 (wire protocol and control round trip) and Phase 2 (a real single-town bus route,
  buses 0 to 3, company value 1 to 11025 on a live server) are done.
- Phase 3, the decision contract behind `/decision/pull`, is the first change that touches
  `prismatic-platform` and is not started (issue I0011).
- A directive mid-run needs an AI restart; the MCP channel (ADR 0008) is the fine-grained
  alternative.
- The world-seeding flag is `-g <heightmap>`; `-G` is the random seed.
