---
schema: adr/v1
id: adr-0035
kind: adr
title: Company AIs are ranked in headless null-driver matches on every reachable LAN machine, and the ranking is a derived record
status: accepted
date: 2026-11-19
tags:
  - ai
  - experiment
  - determinism
  - prismatic-ai
  - cluster
related:
  - file:.ai/repo/adrs/0016-multiplayer-is-deterministic-lockstep-every-peer-runs-the-sa.md
  - file:.ai/repo/adrs/0021-ai-and-game-scripts-run-in-embedded-squirrel-against-a-gener.md
  - file:.ai/repo/adrs/0025-game-time-runs-on-separate-tick-calendar-economy-and-real-ti.md
  - file:.ai/repo/knowledge/curated/duels-run-side-by-side-are-noisy.md
  - file:.ai/repo/rules/project/a-tuning-claim-needs-held-start-pairs.v1.md
  - file:.ai/repo/rules/project/battle-records-are-reproducible.v1.md
  - file:.ai/repo/features/lan-ai-battle.md
  - file:tools/research/ai-battle/battle.py
provenance:
  origin: authored
  derived_from:
    - file:research/ai-battle/README.md
    - file:tools/research/ai-battle/battle.py
    - file:tools/research/ai-battle/referee/BattleReferee/main.nut
---

# 35. Company AIs are ranked in headless null-driver matches on every reachable LAN machine, and the ranking is a derived record

## Context

The owner asked for every machine on the local network to play an AI battle, with PrismaticAI
integrated, the results documented, the run repeated, and the progress visible in a browser
(2026-11-19). This fork already compares bots in **duels**: the MCP panel plans, starts two bots
in a live game, and samples it in wall-clock time (`tools/research/mcp-duel`). Duels are noisy
because the wall clock decides at which tick a bot starts. The same bot can differ by 10-52%
between runs (knowledge `duels-run-side-by-side-are-noisy`), which is why rule
`project.a-tuning-claim-needs-held-start-pairs` exists. A duel also takes wall-clock minutes per
game month, so one machine runs one or two at a time.

OpenTTD's simulation advances in ticks (ADR 0025). The null video driver
(`src/video/null_v.cpp`, `-v null:ticks=N`) runs N ticks as fast as the CPU allows, with no GUI
and no network. A per-game `scripts/game_start.scr` can `start_ai` every contestant at tick 0,
and a Game Script can observe every company without acting (ADR 0021). The LAN has machines of
different architectures and OpenTTD builds, reachable over SSH.

## Decision

Rank company AIs in **headless free-for-all matches** run by `tools/research/ai-battle/battle.py`:

1. **Arenas are discovered.** Every SSH-reachable machine with an OpenTTD binary is an arena. The
   host list (names, addresses, users) is a local file and never enters this public repository.
2. **One match is one null-driver process** in its own `HOME`. Every contestant starts at tick 0,
   in an order that rotates per match. The read-only `BattleReferee` Game Script logs every
   company's standing once a quarter.
3. **An edition is the content hash of the arena bundle.** PrismaticAI and the referee are
   exported with `git archive` at one commit, and an uncommitted export marks the edition dirty.
   Results are only ever compared within one edition.
4. **Every `replay_every`-th match replays an earlier one on another arena.** Determinism across
   machines and builds is thereby measured continuously instead of assumed.
5. **The ranking is a record.** `battle.py report` writes `research/results/battle-*.json` (and its
   derived `.md`). The leaderboard in `research/ai-battle/README.md` is derived from the newest
   record. `battle.py check` enforces both in `tools/gate.sh change` (rule
   `project.battle-records-are-reproducible`).

## Consequences

- A match takes CPU minutes, not wall-clock hours, and machine load cannot change its result. Each
  arena runs as many matches at a time as it has slots.
- The battle measures a bot **without the panel's planner**. It is not a duel and does not
  satisfy rule `project.a-tuning-claim-needs-held-start-pairs`. A bot or planner default still
  changes on held-start duel pairs only.
- Arenas run different builds. Each match records its build, and a replay that differs names the
  pair of builds.
- The tournament adds no code under `src/` and no CMake option. The referee and the orchestrator
  are research tooling, labelled RESEARCH-ONLY (ADR 0004).
- The browser dashboard is served by the orchestrator on loopback. It is not a page of the panel
  shell, so rule `project.web-ui-is-covered-end-to-end` (shell pages) does not cover it; its data
  path is covered by `tools/research/test_ai_battle.py`.

## Alternatives considered

- **Duels on every machine.** Rejected for ranking: they run in wall-clock time, are noisy under
  load, and need the panel stack on every arena.
- **A dedicated server with AIs over the admin port.** Tried first (2026-11-19). A dedicated
  server cannot fast-forward, so one game year took about twelve minutes of wall-clock time, and
  `[server_bind_addresses]` from `private.cfg` was not applied in that run (both ports listened on
  `0.0.0.0`). Rejected.
- **Scoring by a running total instead of records.** Rejected by rule
  `project.docs-are-derived-and-current`: a number typed into a document is a second source of truth.
