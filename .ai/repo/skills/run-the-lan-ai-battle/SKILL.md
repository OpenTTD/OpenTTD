---
schema: skill/v1
id: run-the-lan-ai-battle
version: 1
title: Run the LAN AI battle and publish its record
description: Discover the arenas, prepare an edition from a commit, run the headless tournament with its dashboard, and publish a derived, reproducible record of the result.
status: draft
tags: [ai-battle, prismatic-ai, experiment, determinism, results]
related: [load-focused-context-for-a-change]
inputs:
  - a local OpenTTD build and the OpenGFX tarball
  - SSH key access to the machines that should be arenas
  - committed PrismaticAI and referee (tools/research/prismatic-ai, tools/research/ai-battle/referee)
outputs:
  - research/results/battle-<utc>-<edition>.json and .md
  - the derived leaderboard in research/ai-battle/README.md
  - a passing battle.py check
---

# Purpose

To rank company AIs, PrismaticAI among them, in headless matches on every machine that can run
one. The result is a record anyone can reproduce from its edition (ADR 0035, rule
`project.battle-records-are-reproducible`). This skill is not for tuning a bot or planner default:
that takes held-start duel pairs (rule `project.a-tuning-claim-needs-held-start-pairs`).

# When to use

When the question is "which AI does better on these match parameters", or "does PrismaticAI at
this commit do better than at the last one". Also after an upstream rebase or a new arena build,
to measure whether matches still replay identically across machines.

# Procedure

1. **Arenas.** `python3 tools/research/ai-battle/battle.py discover` scans the local /24 and writes
   `~/ttd-battle/hosts.json`. A machine without a binary is listed and skipped: build it there
   first (`-DOPTION_DEDICATED=ON` is enough on a machine without GUI libraries). Adjust names and
   `slots` in the hosts file; `discover` keeps your values for a host it already knows.
2. **Contestants.** `battle.py fetch` downloads the BaNaNaS AIs named in `battle.json`. A new
   PrismaticAI variant is a new `contestants` entry with its own `label` and `settings`.
3. **Edition.** Commit PrismaticAI and referee changes first, then run `battle.py prepare`. It
   refuses a dirty export. Use `--allow-dirty` only for a trial run: `check` refuses every record
   from such an edition.
4. **Run.** Only on a current instruction from the owner: a tournament holds every arena machine,
   and other sessions build on them (rule `project.long-runs-are-owned-bounded-and-stoppable`).
   Announce it on the board with the orchestrator's machine and how to stop it. `battle.py probe`
   shows each host's state and how many matches its floors admit. Then `battle.py run` (in the
   background; logs to stdout). `~/ttd-battle/PAUSED` on a host or `~/ttd-battle/STOP` beside the
   orchestrator stops new matches, and so does an owner's stop order. Open
   `http://127.0.0.1:8765/`. Watch the arenas, live matches, leaderboard, replays and event log.
   A replay that is not identical names its two builds: read both `host_version`s before
   concluding anything.
5. **Publish.** `battle.py report` writes the record and the README leaderboard. Then run
   `battle.py check` and `tools/gate.sh change`. Commit the record (`.json` and `.md`) and the
   README together. Name the record in any claim, and state its edition and match parameters.
6. **Learn.** A finding that holds beyond one record, for example whether replays are identical
   across a given pair of builds, becomes a knowledge record that cites the battle records it
   rests on.

# Output

A committed record pair under `research/results/`, a README leaderboard derived from it, a passing
`battle.py check`, and claims that name the record, its edition and the match parameters.
