---
id: govern-a-research-fork-of-a-large-codebase
kind: application
title: 'Governing a private research fork of a large upstream codebase'
summary: 'A fork that follows a large upstream by rebasing, adds a research layer on top, and is worked on by agents whose changes must stay scoped, provable and reversible.'
weight: 10
status: active
fits_when:
  - 'The engine is upstream code whose invariants must survive, and the fork adds a layer of its own on top'
  - 'The fork follows upstream by rebasing, so commit hashes change and the record has to be re-proven'
  - 'Agents do most of the work and every claim of a passing validation must be backed by a run'
does_not_fit_when:
  - 'The work is meant for upstream contribution; upstream has its own process, and this layer is not it'
  - 'The fork never follows upstream and has no research layer; plain upstream tooling is enough'
use_cases: [resolve-the-fork-context-for-a-path, validate-the-derived-fork-record]
---

# Context

`korczis/OpenTTD` is a validation target for agent-driven engineering. The value of the fork is
that its changes are auditable. That holds only while the record of why they were made
survives every rebase, and the use cases here prove it still does.
