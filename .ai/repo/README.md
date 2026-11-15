---
schema: context/v1
id: ai.repo
kind: context
title: Tracked repository context
description: The tracked half of the layer and what each section under it is for.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 20
layout:
  lifecycle: canonical
  excludes: [cache, runtime]
---

# Tracked repository context

Everything in this directory is repository-specific canonical context, tracked in Git and
shared by every checkout. The sections, and what each one is for:

| section | holds |
|---|---|
| `policy.yaml` | the one provider-neutral policy: budgets, verification, retention, enforcement, projections |
| `profiles/` | execution profiles, one file each: capability, effort, verbosity, context toggles, verification |
| `rules/` | portable rule objects: the vendored Majordomus baseline and this repository's own rules |
| `prompts/` | reusable prompt assets rendered from durable state |
| `skills/` | reusable provider-neutral operational procedures |
| `workflows/` | multi-step processes a worker follows: the task lifecycle, taking work from the plan, continuity |
| `knowledge/` | declarations of where repository knowledge lives; curated notes, never copies of the repository |
| `adrs/` | accepted, durable architecture decisions |
| `project/` | the plan: milestones as outcome specifications, issues as execution contracts |
| `providers/` | optional: a provider adapter this repository overrides; absent means the tool's default |
| `templates/` | optional: record templates this repository customised |

Do not place machine-local sessions, caches, prompt history or active task state here.
Those belong under `../local/`, which Git ignores.
