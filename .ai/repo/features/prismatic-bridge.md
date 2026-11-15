---
schema: feature/v1
id: prismatic-bridge
kind: feature
title: Prismatic bridge steering a Company AI
short_title: Bridge
headline: Prismatic decides, a standalone bridge relays over the admin network, and a normal Company AI acts in the game.
summary: The bridge (a sibling Python/FastAPI service) drives ai/PrismaticAI through rcon start_ai/reload_ai settings; this repository holds its design and phase record.
status: draft
weight: 40
areas: [agent-integration]
audiences: [prismatic-integrator]
rules: [project.game-state-mutates-only-through-commands]
docs: [research/prismatic-bridge/ARCHITECTURE.md, docs/admin_network.md]
adrs: [adr-0007]
related: [embedded-mcp-server]
tags: [bridge, prismatic, admin-network]
---

## What it does

Phases 1 and 2 work: the control round trip, and a real single-town bus route.

## What it does not do

Its decision contract (`/decision/pull`) is undefined, and a directive mid-run needs an AI
restart.
