---
schema: knowledge/v1
id: mcp-exercise-coverage-is-two-percent
kind: knowledge
class: fact
title: "Every MCP tool is exposed, but about 2% are ever called, and a call counts as covered before its arguments are checked"
description: "Before claiming MCP coverage, reading openttd://mcp/coverage, or extending tools/research/mcp-coverage-check; the measured gap and the smallest generic way to close it."
status: verified
epistemics: observed
date: 2026-11-17
tags: [mcp, coverage, testing, probe]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-coverage-check
    - file:src/prismatic_mcp/prismatic_mcp_coverage.cpp
    - file:src/prismatic_mcp/prismatic_mcp_server.cpp
    - file:src/prismatic_mcp/prismatic_mcp_api.cpp
    - file:tools/research/mcp-human-e2e
    - file:research/prismatic-mcp/09-testing.md
relations:
  - type: relates_to
    target: file:.ai/repo/rules/project/mcp-surfaces-are-derived.v1.md
---

# Fact

Measured 2026-11-17 (read only, `openttd://mcp/coverage` through the panel; no probe run on the live game):

- **Exposure is complete:** `mcp-api-coverage` counts 146/146 commands, 87/87 console commands and 630/630
  Script API functions routed.
- **Exercise is not:** the :8800 server has 17 of 808 tools and resources called (2.1%), the GUI client
  23 of 837 (2.7%), three e2e servers 15 of 808 each. No scenario has ever called an `openttd.api.*` or
  `openttd.cmd.*` tool. Resources: 7 of 22 uncovered on the servers, 12 on the client.
- **Code:** panel JS lines 58.8% over the files the tests load (37% of the whole tree; `app.js`,
  `world.js`, `viz.js`, `tool.js`, `ui.js` load in no test); `tools/research/mcp-rhai` 58.4% lines
  (cargo-llvm-cov; the `bin/` entry points and `graph.rs` at 0%).

Why the existing probe cannot close it:

- It calls only tools with `readOnlyHint: true` (4 on a server, 11 on a client). All 630 `openttd.api.*`
  tools say `readOnlyHint: false` (`prismatic_mcp_api.cpp`), although without `execute` they run in test
  mode, unapproved, changing nothing (`prismatic_mcp_server.cpp`).
- `Coverage::Count` runs when a request arrives, before any argument check, so a call with
  wrong arguments counts as covered; the probe's `derived_args` ignores `prefixItems`, `minItems` and
  `enum`, so `--exercise` would turn api and cmd green while testing nothing.
- Plain `mcp-e2e` stacks never run the probe; `09-testing.md` and the exposure/spec matrices still quote
  counts from catalogues of 24 and 53 entries.

# How to apply it

Close it generically, deriving everything from `tools/list` (no tool named):

1. Annotate every `openttd.api.*` tool `x-readOnlyWithout: execute`, so the probe calls it in test mode;
   exclude the few with effects outside the command system (`ScriptAdmin.Send`, `ScriptWindow.*`,
   `ScriptController.Sleep/Break/Import`).
2. Count a `decision.validate` call against the tool it stands for (`command.X` -> `openttd.cmd.X`,
   `ai.X`, `game.set_pause`), and have the probe validate every `decision.list_action_types` entry:
   validation is a test run that queues no approval.
3. Count outcomes, not arrivals: called, answered without `isError`, validated; the gate requires the
   second or third.
4. Derive arguments that pass the schema (`prefixItems` to `minItems`, `minimum`, first `enum`, a real id
   for `x-kind: id` from the matching resource), so the game's own check gives the verdict.
5. Run `mcp-coverage-check --probe --report` at the end of every `mcp-e2e`.

Only `openttd.decision.submit` then needs a real call, which stays on the throwaway `--exercise` path.
Until this lands, report MCP coverage as exposure complete, exercise about 2%, never as "covered".
