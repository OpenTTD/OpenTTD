---
schema: knowledge/v1
id: a-command-test-pass-must-not-draw-random
kind: knowledge
class: constraint
title: "A command handler draws Random() only in its execute pass; a draw in the test pass shifts the shared stream and desyncs"
description: "When writing or reviewing any Cmd* handler that uses randomness."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [commands, map-and-tiles]
about: src/terraform_cmd.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/terraform_cmd.cpp
    - file:src/command.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/commands/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/map-and-tiles/README.md
---

# Statement

`src/terraform_cmd.cpp:CmdTerraformLand` guards its `RandomRange` call with
`flags.Test(DoCommandFlag::Execute)`, and its comment says a test-run roll "will cause a desync":
test passes run a different number of times on different peers (estimates, GUI previews, the
network sender). Draw only under `DoCommandFlag::Execute`.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote a-command-test-pass-must-not-draw-random` with evidence, or `majordomus knowledge reject a-command-test-pass-must-not-draw-random --reason ...`.
