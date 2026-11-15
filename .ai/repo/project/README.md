---
schema: context/v1
id: ai.repo.project
kind: context
title: Project model
description: Milestones as outcomes, issues as execution contracts, and a dependency graph that decides what is next.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
children:
  require_contract: false
---

# Project model

The canonical plan: `project.yaml` for the project itself, `milestones/` for the outcomes,
`issues/` for the execution contracts that reach them, and the dependency edges between
issues for the order they may be executed in. Nothing here is generated; everything
downstream is. The ready set, the diagrams and the projections all come from
`majordomus plan`, which reads these files and nothing else.

No status is stored. An issue records what happened to it — when it started, when it was
verified, when it completed, and its evidence — and the state is derived from those facts
and from its dependencies. A status field would be a second source of truth that drifts
the moment a dependency changes.

The directories below hold instances of these kinds rather than sections of their own, so
they owe no contract until this repository decides to describe them
(`children.require_contract: false`).

```bash
majordomus plan next          # the issues whose dependencies are satisfied
majordomus plan validate      # the graph: acyclic, every edge resolves
```
