---
schema: adr/v1
id: adr-0005
kind: adr
title: An experiment is driven by the research CLI and its build profile, runtime flags and evidence are recorded in a versioned manifest
status: proposed
date: 2026-07-24
tags:
  - research
  - reproducibility
  - evidence
related:
  - file:tools/research/research
  - file:research/tooling.md
  - file:tools/research/schema/experiment-manifest.schema.json
  - file:tools/research/test/research_test.sh
  - file:.ai/repo/adrs/0002-validation-runs-in-three-cost-tiers-behind-one-gate-script-a.md
provenance:
  origin: extracted
  derived_from:
    - commit:8c4cc4fc322f036c032aa6241073f5f0e20a3b5e
    - commit:19b32cafd6c8742b0eeb266017541b07403fc75e
    - commit:84be696a48cbe847ce3a72a6fae28c26e387b861
    - file:tools/research/research
    - file:research/tooling.md
    - file:tools/research/schema/experiment-manifest.schema.json
---

# 5. An experiment is driven by the research CLI and its build profile, runtime flags and evidence are recorded in a versioned manifest

## Context

An experiment whose build options, compiler flags and runtime flags live only in a
terminal's scrollback cannot be reproduced or compared. Running the same agent twice must
produce comparable records, and the comparison between an instrumented and a plain build must
be part of the record, not a memory.

## Decision

An explicit experiment runs through `tools/research/research`:

- `init --experiment-id ID` creates an evidence bundle under `build-research-runs/<id>/` with a
  versioned JSON manifest (`tools/research/schema/experiment-manifest.schema.json`);
- `configure` and `build --profile P` use one of three profiles: `research-debug` (Debug,
  instrumentation ON), `baseline-release` (RelWithDebInfo, asserts and instrumentation OFF) and
  `research-asan` (configure-only);
- `run` captures the `research_status` demonstrator from a headless dedicated server;
- `validate --gate` wraps the gate tiers and the manifest completeness check;
- `report` finalises the manifest and writes `summary.md` from what was recorded.

Every command records itself in `commands.jsonl`; a build always records its profile, build
directory, build type and research mode, even against a directory an earlier experiment
configured. The CLI refuses to silently reconfigure a build directory that does not match its
profile.

## Alternatives rejected

- **Hand-run cmake and ctest with a notes file.** It does not record what actually ran.
- **One build directory for every profile.** It mixes configurations and loses the ON/OFF
  comparison.

## Consequences

- The CLI has its own suite: `tools/research/test/research_test.sh` (28/28 when it landed).
- Every command needs `--experiment-id`, including `build`; a bare `build` exits 2.
- The headless run feeds commands on stdin after a fixed delay, and that start-up race is a
  documented limitation: under heavy load the `research_status` line can be lost.
