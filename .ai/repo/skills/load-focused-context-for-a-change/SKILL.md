---
schema: skill/v1
id: load-focused-context-for-a-change
version: 1
title: Load focused context for a change in the substrate or the fork
description: Resolve, from the paths a task will touch, the subsystem map, rules, decisions, knowledge and tests that apply, without reading the .ai/ tree or the subsystem end to end.
status: draft
tags: [context, navigation, knowledge, tests]
related: [derive-the-ai-layer-from-history]
inputs:
  - the paths the task will change, or a symbol or concept to find them from
outputs:
  - the effective context chain for each path, the decisions and records it names, the tests and gate tier to run
---

# Purpose

The `.ai/` layer is not loaded whole. Every major `src/` subsystem and the fork's MCP module has a
map (`.ai/repo/workflows/substrate/<subsystem>/README.md`) whose `tracks` attach it to the files it
owns, so the path you will edit selects the map you need. This skill turns "I need to change X"
into the few documents, symbols and tests that bear on X. It does not replace reading the code the
map points to; it tells you which code to read first.

# When to use

At the start of any task that changes `src/`, `cmake/`, `regression/` or the fork's research
tooling, and again when the task's scope moves to another subsystem.

# Procedure

1. **Find the paths.** If you only have a concept, search the layer, then the code:
   `majordomus knowledge search <word>` (records, rules, ADRs, documents),
   then `git grep -n <Symbol> -- src/` for the definition.
2. **Resolve the context of each path.** `majordomus context resolve <path>` prints the chain that
   applies, root first; `majordomus context explain <path>` says why each document is in or out.
   Read the subsystem map it names: its Entry points, Invariants and Traps sections first.
3. **Load only the rules, decisions and records the map names.** `majordomus rules show <id>`,
   `majordomus adr show <id>`, and the knowledge record files it cites. For a record,
   `majordomus knowledge related <id>` lists what links to it.
4. **Find what else cites the files you will change.**
   `tools/knowledge/refs index --out .ai/local/cache/knowledge-refs.json` (rebuildable, never
   tracked) maps each cited file to the records and symbols citing it.
5. **Pick the tests before writing code.** The map's "Tests and validation" section names the
   `src/tests/*.cpp` cases, the `regression/` scripts and the `tools/gate.sh` tier. Code under
   `OPTION_RESEARCH_INSTRUMENTATION` or `OPTION_PRISMATIC_MCP_SERVER` needs `tools/gate.sh
   research`: the default build never compiles it (knowledge `gated-code-is-invisible-to-a-default-build`).
6. **Before finishing, see what your change made stale.**
   `tools/knowledge/refs impact HEAD` (the working tree) or
   `tools/knowledge/refs impact "$(git merge-base HEAD master)..HEAD"` (a branch) lists the records
   that cite a file you changed and the symbols to re-check; `majordomus adr affected --worktree`
   does the same for decisions. Update a record your change falsified, or record why it still
   holds; `tools/knowledge/refs check` must pass.

# Output

For each path: the context chain, the decisions and rules that bind it, the tests and gate tier
to run, and after the change, the impacted records you re-read with what you concluded.

# Examples

- A new MCP tool that reads vehicles: `majordomus context resolve src/prismatic_mcp/prismatic_mcp_server.cpp`
  gives the MCP module map (registration, the approval boundary) and the substrate chain;
  `majordomus context resolve src/vehicle_base.h` gives the vehicles map (pools, who may mutate).
- A savegame field: `majordomus context resolve src/saveload/vehicle_sl.cpp` gives the save/load map
  and rule `project.savegame-fields-are-version-gated`.
