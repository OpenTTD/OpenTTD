---
schema: adr/v1
id: adr-0016
kind: adr
title: "Multiplayer is deterministic lockstep: every peer runs the same command stream and only commands cross the wire"
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - network
  - commands
  - determinism
related:
  - file:.ai/repo/workflows/substrate/network/README.md
  - file:.ai/repo/workflows/substrate/commands/README.md
  - file:.ai/repo/workflows/substrate/core-loop/README.md
  - file:.ai/repo/workflows/substrate/core-utilities/README.md
  - rule:project.game-state-mutates-only-through-commands
  - file:src/network/network_command.cpp
  - file:docs/desync.md
provenance:
  origin: extracted
  derived_from:
    - file:docs/desync.md
    - file:src/network/network.cpp
    - file:src/network/network_command.cpp
    - file:src/core/random_func.hpp
    - issue:I0066
---

# 16. Multiplayer is deterministic lockstep: every peer runs the same command stream and only commands cross the wire

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

The game state is large and changes every tick, but almost all of that change is a function of
the previous state. `docs/desync.md` ("OpenTTD multiplayer architecture") states the design: a
joining client receives a savegame, and afterwards only commands — player, AI, Game Script,
admin port and rcon actions — plus the instruction to run up to a network frame.

## Decision

1. **Join by savegame, then commands only.** The server sends the map as a savegame; from then
   on `src/network/network_command.cpp:NetworkDistributeCommands` assigns each queued command a
   frame and sends it to every client, and each peer runs it in the same frame and order
   (`src/network/network_command.cpp:NetworkExecuteLocalCommandQueue`).
2. **The server paces time.** `src/network/network.cpp:NetworkGameLoop` advances the frame
   counter only up to `_frame_counter_max`, which the server extends by `frame_freq`.
3. **Desync detection by random state.** The server samples `_random.state[0]` into
   `_sync_seed_1` and clients compare it at `_sync_frame`
   (`src/network/network_client.cpp:ClientNetworkGameSocketHandler::ReceiveServerSync`).
4. **Two random streams.** `src/core/random_func.hpp:_random` feeds the game state and must
   advance identically on every peer; `_interactive_random` serves everything that does not
   influence the state (GUI, sounds, effects).

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- Every mutation of persistent state goes through a command (ADR 0016 is why
  `project.game-state-mutates-only-through-commands` is blocking), and a handler must be
  deterministic: no wall-clock time, no host-dependent float or iteration order, no
  uninitialised reads, no `_random` draw outside the execute pass.
- A command's effect arrives frames after `Post` in a network game; a caller that reports success
  at dispatch reports something that has not happened yet.
- The desync check compares only the game random state (knowledge candidate
  `the-desync-check-compares-only-the-game-random-state`): a divergence that does not touch
  `_random` is detected late or not at all.
- Settings, NewGRFs and the script set that influence simulation must match between peers
  (ADR 0019, ADR 0021); a research build joins a default build's game only while its gated code
  changes nothing the stream depends on (candidate
  `a-research-build-and-a-default-build-join-the-same-game`).

## For this fork

- The PrismaticMCP server mutates only by `Command<>::Post` (ADR 0008), so an approved action is
  replayed by every peer; its view tools stay outside the stream (ADR 0013).
- Experiments that claim determinism need a `-d desync=` run (`docs/debugging_desyncs.md`);
  a single-player run proves nothing about lockstep.
- Maps: [network](../workflows/substrate/network/README.md),
  [commands](../workflows/substrate/commands/README.md),
  [core loop](../workflows/substrate/core-loop/README.md),
  [core utilities](../workflows/substrate/core-utilities/README.md).
