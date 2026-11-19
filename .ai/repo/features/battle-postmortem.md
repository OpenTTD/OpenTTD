---
schema: feature/v1
id: battle-postmortem
kind: feature
title: Cross-match post-mortem of the LAN AI battle
short_title: Battle post-mortem
headline: After each round, a Rust tool analyses every finished match in parallel, names why each contestant lost against fixed thresholds, counts PrismaticAI's last places, and proposes settings for the next edition.
summary: tools/research/mcp-rhai battle-postmortem reads the battle's machine store (battle.sqlite, read-only) or an edition's JSON match records, analyses each match on rayon's pool (src/postmortem.rs), writes research/results/postmortem-<edition>.json and the .md derived from it, and fills DuckDB tables (analyses, contestants, proposals) that join with the store; tools/gate.sh change runs its check.
status: draft
weight: 46
areas: [agent-integration]
audiences: [prismatic-integrator]
rules: [project.prismatic-ai-is-never-last, project.battle-records-are-reproducible, project.docs-are-derived-and-current, project.tooling-language-preference, project.a-tuning-claim-needs-held-start-pairs]
docs: [research/battle-postmortem.md]
adrs: [adr-0035, adr-0033]
related: [lan-ai-battle, event-bus-and-store, layered-validation-gate]
tags: [ai-battle, prismatic-ai, post-mortem, duckdb, rayon, self-tuning]
---

## What it does

For each contestant of each finished match, the tool computes rank, value share of the winner,
peak and final fleet, idle cash share, income per vehicle, stations per vehicle, and the quarter
the gap to the winner opens. It names five causes, each a test against a named constant:
`stalls_small`, `fleet_decays`, `hoards_cash`, `low_yield` and `idle_stations`. Per edition, it
aggregates them per contestant and lists every PrismaticAI contestant that finished last. It
turns the causes seen in at least half of a contestant's matches into a settings proposal.

## Why

`battle.py` ranks contestants and writes a post-mortem per match. The question the owner asks is
"where does PrismaticAI lose, and what do we change next round". That needs the causes across
every match of a round, stated against thresholds anyone can check, and kept from round to round.
The proposals are the self-tuning input. The battle's owner accepts one by adding it as a
contestant, which makes a new edition. When a proposal equals a contestant that already exists,
the cause is the bot's behaviour, not its settings.

## Boundaries

It reads and proposes only. It opens the store read-only, never edits `battle.json` or the bot,
and makes no claim about defaults (rule `project.a-tuning-claim-needs-held-start-pairs`).
`battle.py check` enforces that PrismaticAI is never last (rule `project.prismatic-ai-is-never-last`).
The post-mortem is the diagnosis when that rule fails.
