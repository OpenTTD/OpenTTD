---
id: project.public-api-is-exposed
version: 1
kind: rule
title: OpenTTD's public C++ API is exposed through MCP and OpenAPI, derived and measured
description: Every game-state command (the Commands enum), every console command, and every static function of the Script API is reachable through MCP and documented in the panel's OpenAPI document, by derivation from the sources and the live catalogue, never by a hand-written list. Coverage is measured from the sources by tools/research/mcp-rhai's mcp-api-coverage.
statement: Expose every Commands enum entry as openttd.cmd.<Name>, every Script API static function as openttd.api.<Class>.<Function>, and every console command through the derived console surface, each with its OpenAPI route; keep mcp-api-coverage at 100% in every dimension.
status: active
class: blocking
depends_on: [project.mcp-surfaces-are-derived@1, project.game-state-mutates-only-through-commands@1]
tags: [mcp, openapi, api, coverage, derivation]
---

# Rationale

The owner's requirement: everything with a public API in C++ is exposed, dynamically, on MCP and
Swagger at least, and this is enforced. A surface nobody measures is claimed, not shown.

# Required behaviour

- Measure: `tools/research/mcp-rhai/target/release/mcp-api-coverage --repo . --panel <panel>`. It
  derives the three dimensions from `src/command_type.h`, `src/console_cmds.cpp` and
  `src/script/api/script_*.hpp`, and checks `tools/list` and `/openapi.json`. Exit 10 below 100%.
- Commands go through the decision path (validate, submit, approval, `Command<>::Post`), never a
  direct pool write.
- Script API functions that change state are commands underneath and follow the same path;
  read-only ones are tools or resources.

# Current state (2026-11-15, master 9176fdfcdb)

The rule is not met, and is recorded as not met: commands 0/146, console 83/86 (three commands
are compiled only into some builds), Script API 0/660. The command-table catalogue
(`openttd.cmd.*`, 125 of 146 exposable) is on branch `research/mcp-full-auto`, not merged. Issue
I0036 carries the rest.

# Failure behaviour

`mcp-api-coverage` below 100% blocks a claim that the API is exposed. There is no Majordomus
validator for it; the check and review enforce it.
