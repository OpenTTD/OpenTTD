---
schema: context/v1
id: fork.research-layer
kind: context
title: Working in the fork's research and gate tooling
description: What binds a change to research/, tools/gate.sh and tools/research/, the fork's experimental scaffolding.
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 200
tracks:
  - 'research/**'
  - 'tools/**'
---

# Working in the fork's research and gate tooling

The experimental-infrastructure kind of change (AGENTS.md, "four kinds of change"). The
procedure for an experiment is `.ai/repo/workflows/research-experiment.md`.

## Rules that apply

- `project.never-report-unrun-validation-as-pass`
- `project.research-code-is-gated-and-labelled`
- `project.no-commit-without-current-instruction`

## Decisions and knowledge

- ADR 0002 (cost tiers), ADR 0005 (research CLI and manifest), ADR 0006 (toolchain probe),
  ADR 0007 (bridge design in `research/prismatic-bridge/`).
- Knowledge `dedicated-server-stdin-startup-race`, `research-cli-keeps-the-system-compiler`,
  `bridge-control-is-coarse-ai-restart`.
- Verify a change here with `bash tools/research/test/research_test.sh` and
  `tools/gate.sh change`.
