---
schema: knowledge/v1
id: auto-approve-is-the-owners-switch
kind: knowledge
class: convention
title: "Auto-approve of MCP decisions is switched on only by the owner, never inferred by an agent"
description: "When launching tools/research/mcp-e2e, scripting the panel, or building an agent loop over the MCP approval flow."
status: verified
epistemics: decided
date: 2026-11-15
tags: [mcp, approval, oversight, agents]
about: tools/research/mcp-rhai/src/panel.rs
route: enforcement
provenance:
  origin: authored
  derived_from:
    - issue:I0031
    - issue:I0032
    - file:tools/research/mcp-rhai/src/panel.rs
    - commit:60aef3caa5529bbe769e8c5c423615aefb7d196a
relations:
  - type: relates_to
    target: file:.ai/repo/adrs/0008-an-agent-reaches-the-game-through-an-embedded-loopback-mcp-s.md
  - type: supports
    target: file:.ai/repo/rules/project/game-state-mutates-only-through-commands.v1.md
---

# Convention

The approval of an MCP decision is the owner's act: the panel's Approve button or `/approve` in
the `~` console. The panel's auto-approve (`--auto-approve`, the window's switch,
`POST /api/auto-approve`) hands that act to the panel, so only the owner turns it on. An agent
never turns it on, not even on an inference from "full auto". A script or agent loop never
approves its own submissions: `ttd::set_pause` waits for the approval, and a peer session's
harness refused a loop that called `/api/approve` on its own decisions.

# What happened

On 2026-11-15 I relaunched the :8800 stack with `--auto-approve` on an inference, and session
openttd-59 raised it. The owner was asked: "When I relaunched the :8800 stack I passed
--auto-approve on my own, inferring it from your "full auto" and from your earlier toggle in the
panel. I have now switched it OFF. Should the running game approve its pending MCP decisions
automatically?" The answer was "Keep it off (Recommended)". `mcp-e2e` starts with auto-approve
off unless the owner passes the flag.
