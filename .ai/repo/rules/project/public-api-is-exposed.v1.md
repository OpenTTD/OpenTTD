---
id: project.public-api-is-exposed
version: 1
kind: rule
title: OpenTTD's public C++ API is exposed through MCP and OpenAPI, derived and measured
description: Every game-state command (the Commands enum), every console command, and every static function of the Script API is reachable through MCP and documented in the panel's OpenAPI document, by derivation from the sources and the live catalogue, never by a hand-written list. Coverage is measured from the sources by tools/research/mcp-rhai's mcp-api-coverage.
statement: Expose every Commands enum entry as openttd.cmd.<Name> (<Name> its Cmd* function without the prefix, as its DEF_CMD_TRAIT declares), every Script API static function as openttd.api.<Class>.<Function>, and every console command through the derived console surface, each with its OpenAPI route; keep mcp-api-coverage at 100% in every dimension.
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
  derives the three dimensions from `src/command_type.h`, `src/console_cmds.cpp` and the Script
  API the build publishes to scripts (the `DefSQStaticMethod`/`DefSQAdvancedStaticMethod` of
  `build*/generated/script/api/{ai,game}/*.sq.hpp`, generated from `src/script/api/script_*.hpp`,
  plus the controllers' `*_controller.sq.hpp`), and checks `tools/list` and `/openapi.json`. Exit
  10 below 100%.
- Commands go through the decision path (validate, submit, approval, `Command<>::Post`), never a
  direct pool write.
- Script API functions that change state are commands underneath and follow the same path;
  read-only ones are tools or resources.

# Current state (2026-11-17, master 11aa8f41db)

Measured against a panel on this build (seed 31337): commands 146/146, Script API 630/630,
console 87/90. The rule is met for commands and the Script API; console is short by three
commands that only some builds or settings have (below).

- Commands: met. Every entry is `openttd.cmd.<Name>`, derived from the command table
  (`src/prismatic_mcp/prismatic_mcp_commands.cpp`), approval-gated, with its OpenAPI route. Every
  parameter type has a JSON spelling derived from it (`prismatic_mcp_params.hpp`): integers, enums,
  ids, bools, strings, a text, a vehicle list, a cargo source, an order (its wire fields), and any
  `std::variant` of those (`null` or `{index, value}`).
- Script API: met. Every published static function is `openttd.api.Script<Class>.<Function>`
  (`src/prismatic_mcp/prismatic_mcp_api.cpp`), answered by private GS and AI script engines that
  register the API as GameInstance and AIInstance do; the catalogue and parameter types are read
  back from them. A call runs in test mode and changes nothing; `execute` is approval-gated and
  posts in async mode. See `research/mcp-api.md`.
- The Script API's denominator changed from 660 to 630 on 2026-11-17, and no coverage was lost.
  660 counted every `static` line of `src/script/api/script_*.hpp`, which includes what scripts
  never see: `ScriptObject`'s protected statics, mode callbacks, internal helpers such as
  `_BuildRoadInternal`. 630 is what the build's squirrel export publishes to AI and game scripts.
- Console: the three missing commands are gated by design: `resettile` is compiled only into
  `_DEBUG` builds, and `reload_newgrfs` and `newgrf_profile` are hidden by their hook until
  `newgrf_developer_tools` is on.
- `tools/research/mcp-live-check` exercises catalogue commands end to end: `Pause`, `CreateGoal`
  (a text) and `CustomNewsItem` (a variant). Issue I0036 carries the rest.

# Failure behaviour

`mcp-api-coverage` below 100% blocks a claim that the API is exposed. There is no Majordomus
validator for it; the check and review enforce it.
