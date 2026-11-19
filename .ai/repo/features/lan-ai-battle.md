---
schema: feature/v1
id: lan-ai-battle
kind: feature
title: LAN AI battle between company AIs on every reachable machine
short_title: AI battle
headline: Every SSH-reachable machine runs headless free-for-all matches between PrismaticAI and BaNaNaS AIs, the leaderboard is a derived record, and a browser dashboard shows the run live.
summary: tools/research/ai-battle/battle.py discovers arenas on the local /24, exports PrismaticAI and a read-only referee Game Script from a commit, schedules null-driver matches onto every arena without end, replays some on another arena to measure cross-machine determinism, and writes research/results/battle-*.json from which the README leaderboard is derived.
status: draft
weight: 45
areas: [agent-integration]
audiences: [prismatic-integrator]
rules: [project.battle-records-are-reproducible, project.prismatic-ai-passes-the-headless-fitness-check, project.docs-are-derived-and-current, project.a-tuning-claim-needs-held-start-pairs]
docs: [research/ai-battle/README.md]
adrs: [adr-0035, adr-0021, adr-0025]
related: [prismatic-bridge, research-experiment-cli, layered-validation-gate]
tags: [ai-battle, prismatic-ai, determinism, cluster, dashboard]
---

## What it does

`discover` scans the local /24 for SSH and writes the arenas it finds (with an OpenTTD binary) to
a local hosts file. `prepare` builds the arena bundle: OpenGFX, the BaNaNaS contestants `fetch`
downloaded, and PrismaticAI and the `BattleReferee` Game Script exported from one commit. `run`
rsyncs the bundle to each arena and fills its slots with 20-year free-for-all matches
(`-v null:ticks=N`, one isolated `HOME` per match). It reads the referee's quarterly lines over
SSH, kills a stalled match, replays every few matches on another arena, and serves the dashboard
on loopback. `report` writes the record, and `check` keeps the record and the README leaderboard
derived in `tools/gate.sh change`.

The orchestrator also watches itself. Every contestant of every match carries live `anomalies`
(`fleet_drop`, `squirrel_errors`, `no_vehicles`), and a new one is an `ALERT` in the event log
and on the dashboard. `fitness` runs the working tree's PrismaticAI in one short deterministic
match and fails on a crash or a collapsing fleet, in `tools/gate.sh change` whenever the bot or
the tooling changes (rule `project.prismatic-ai-passes-the-headless-fitness-check`). The API
(`/api/state`, `/api/stream` server-sent events, `/api/matches/{id}`, OpenAPI with Swagger UI at
`/docs`) serves the dashboard and any other client. PrismaticAI enters as default, solo-max, and
self-steward: solo-max plus the opt-in in-bot steward that keeps its lines growing without the
panel (knowledge `prismatic-ai-without-a-steward-keeps-one-train-per-line`).

## What it does not do

It does not hand the panel's planner lines to PrismaticAI. The bot plays alone, so a battle
result is not a held-start duel and does not justify changing a default (rule
`project.a-tuning-claim-needs-held-start-pairs`). It does not build OpenTTD on an arena; a machine
without a binary is listed and skipped. It compares nothing across editions or match parameters.
The dashboard is not a page of the panel shell.
