---
schema: context/v1
id: fork.substrate
kind: context
title: Working in OpenTTD's own code
description: What binds a change to src/, the build system and the regression suite of the system under test.
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 200
tracks:
  - 'src/**'
  - 'regression/**'
  - 'CMakeLists.txt'
  - 'cmake/**'
---

# Working in OpenTTD's own code

`src/`, `regression/` and the build system are the system under test (ADR 0001). Upstream's
technical constraints bind every change here, and its contribution process does not.

## Rules that apply

- `project.game-state-mutates-only-through-commands`: every mutation goes through a registered
  command; handlers stay deterministic.
- `project.savegame-fields-are-version-gated`: no saved field is removed, reordered or
  renumbered.
- `project.research-code-is-gated-and-labelled`: research hooks compile only under
  `OPTION_RESEARCH_INSTRUMENTATION` (see `cmake/Options.cmake`).
- `project.never-report-unrun-validation-as-pass`: validate with `tools/gate.sh` at the tier the
  change needs.

## Decisions and knowledge

- ADR 0004 (research access to internals), ADR 0008 (embedded MCP server).
- Knowledge `gated-code-is-invisible-to-a-default-build`, `macos-missing-baseset-hangs-headless-runs`,
  `heightmap-flag-is-lowercase-g`.
- Architecture reference: the "Architecture" section of `CLAUDE.md`, `CODINGSTYLE.md`,
  `docs/savegame_format.md`, `docs/desync.md`.
