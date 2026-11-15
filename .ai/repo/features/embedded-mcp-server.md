---
schema: feature/v1
id: embedded-mcp-server
kind: feature
title: Embedded loopback MCP server with approved, digest-bound mutation
short_title: MCP server
headline: An agent reads live game state over MCP and changes it only with an operator's approval of the exact arguments, through the command system.
summary: OPTION_PRISMATIC_MCP_SERVER (requires research instrumentation) serves Streamable HTTP on loopback from the game loop; the DecisionEnvelope gates every mutation.
status: draft
weight: 50
areas: [agent-integration, observability]
audiences: [prismatic-integrator, coding-agent]
rules: [project.game-state-mutates-only-through-commands, project.research-code-is-gated-and-labelled, project.savegame-fields-are-version-gated]
adrs: [adr-0008]
related: [research-instrumentation, prismatic-bridge]
tags: [mcp, security, approval]
---

## What it does

On the branch `research/prismatic-mcp-server`: live resources, bearer auth, Origin rejection,
`openttd.decision.validate`, and an approval-gated `submit` that pauses the game via
`Command<Commands::Pause>::Post`.

## What it does not do

It has not landed on `master` (issue I0017), and it exposes no company-scoped tool yet.
