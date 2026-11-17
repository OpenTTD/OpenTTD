---
schema: knowledge/v1
id: a-panel-registers-before-its-game-is-up
kind: knowledge
class: lesson
title: "A panel announces itself before its game's MCP server is up, so a catalogue read once at discovery lacks every resource route"
description: "When a client caches what an instance offers (hub, Rhai, a bot): read it again, never only at discovery."
status: verified
epistemics: observed
date: 2026-11-17
tags: [prismatic-mcp, hub, discovery]
about: tools/research/mcp-rhai/src/hub.rs
route: platform
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-rhai/src/panel.rs
    - commit:60aef3caa5529bbe769e8c5c423615aefb7d196a
    - file:tools/research/mcp-rhai/src/hub.rs
    - commit:36c66c3ac749411fe1e716c4ee34befe81791d78
relations:
  - type: relates_to
    target: file:research/mcp-hub.md
---

# Statement

The panel (then the Python server.py, removed in 60aef3caa5; now `tools/research/mcp-rhai/src/panel.rs`) writes its registry record (`registry_write`) as it starts, and
derives its OpenAPI routes from the live MCP catalogue, which is empty of resources until the game's
MCP server answers. The hub read each instance's catalogue once, on discovery, so a panel found early
stayed without `/api/resources/*` for the life of the hub, and the audit answered "no stations
operation". Since 36c66c3ac7 the hub reads a catalogue again after `CATALOGUE_TTL_CYCLES`
(`tools/research/mcp-rhai/src/hub.rs`) and announces a catalogue that changed.

# Evidence

Re-verified against master at 5778b8909c (triage session ff4a0d12, 2026-11-17), read only:
- tools/research/mcp-rhai/src/panel.rs:1316 registry_write, called at :1669 as the panel starts.
- tools/research/mcp-rhai/src/hub.rs:375 CATALOGUE_TTL_CYCLES = 15 and :418 re-reads a catalogue once it is that many cycles old: the fix the record names (36c66c3ac7) is in place, and the lesson (a catalogue read once at discovery can be empty) stands.
