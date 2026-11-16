---
schema: adr/v1
id: adr-0015
kind: adr
title: The fork does not run upstream's CI workflows on push; Pages deploys from Surfaces alone
status: proposed
date: 2026-11-16
tags:
  - ci
  - pages
  - fork
provenance:
  origin: authored
  derived_from:
    - file:.ai/repo/adrs/0001-this-fork-is-a-private-research-target-and-never-interacts-w.md
    - file:.ai/repo/adrs/0002-validation-runs-in-three-cost-tiers-behind-one-gate-script-a.md
    - file:.ai/repo/adrs/0012-every-published-surface-is-derived-from-tracked-markdown-git.md
    - issue:I0019
---

# 15. The fork does not run upstream's CI workflows on push; Pages deploys from Surfaces alone

## Context

The fork inherits upstream's GitHub workflows. Three of them run on every push to master:
"CI - Build", "CodeQL" and "Update 'work: needs rebase' label status". Validation in this fork is
local, in `tools/gate.sh` (ADR 0002). Upstream's CI checks upstream's own contribution process,
which this fork does not take part in (ADR 0001). Meanwhile the fork's own Pages workflow,
`Surfaces` (ADR 0012), waited in the queue behind those runs. Several sessions push to master, and
each newer push cancelled the waiting `Surfaces` run through the `pages` concurrency group, so
Pages stopped deploying. On 2026-11-16 four deploys in a row ended cancelled.

An agent's attempt to cancel stale runs, and then to disable the workflows, was refused by its
harness. Disabling CodeQL weakens oversight, so the decision belongs to the owner.

## Decision

The owner disabled the three push-triggered upstream workflows on `korczis/OpenTTD` on 2026-11-16
(`gh workflow disable`, state `disabled_manually`). Release, preview and checker workflows that do
not run on push stay as they are. `Surfaces` is the only workflow that runs on a push to master, and
it can also be started by hand (`gh workflow run surfaces.yml -R korczis/OpenTTD`).

## Alternatives rejected

- Cancelling superseded runs by hand: each new push recreates the queue, and the harness refused it.
- Running Pages only on dispatch or a schedule: pushes would no longer cancel the deploy, but
  publishing would no longer follow each change, and runners would still be busy with upstream CI.
- Batching pushes: several sessions push to master independently, so it cannot be enforced.

## Consequences

- Upstream's build matrix (Linux, macOS, Windows, MinGW, Emscripten) and CodeQL scanning no longer
  run on the fork's pushes. The fork's evidence is `tools/gate.sh` (smoke, change, research, full)
  and the research checks, as ADR 0002 already says.
- To bring them back: `gh workflow enable "<name>" -R korczis/OpenTTD`. A rebase onto upstream
  (ADR 0011) does not re-enable them, because the state lives on GitHub, not in the files.
- Pages deploys again on every push to master, and a session that wants a deploy right away can
  dispatch `Surfaces`.
