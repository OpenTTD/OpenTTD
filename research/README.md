# Research / validation layer

This directory is part of this fork's own process scaffolding, not upstream OpenTTD material — see [AGENTS.md](../AGENTS.md) for the full context (this repo is a private research fork used as a validation target for the Prismatic platform, not staged for upstream contribution).

## Gating model

Four layers, increasing in cost. Match the layer to the size of the change — don't run `full` for a one-line doc fix, and don't call a source change "done" on `smoke` alone.

| Layer | Entry point | Cost | When |
|---|---|---|---|
| Smoke | `tools/gate.sh smoke` | cheapest | fast iteration while actively editing |
| Change | `tools/gate.sh change` | standard | before marking a task/experiment done |
| Full | `tools/gate.sh full` | expensive | significant changes, or establishing an experimental baseline |
| Research/evaluation | this directory | n/a | records what was actually done and validated for an experiment |

`tools/gate.sh --help` documents the exact commands each layer runs. All three are thin wrappers around this repo's own existing CMake/CTest toolchain (see `CLAUDE.md`) — there is no separate lint/format gate because no such tool (e.g. a `.clang-format` config) currently exists in this codebase; don't fake one.

## Recording an experiment

Copy [`experiment-template.md`](./experiment-template.md), fill it in, and keep it wherever is convenient for you (this directory is a reasonable default, but filled-in reports are not required to be committed — they're working notes, not gated artifacts). The template is the versioned, reusable part; a specific experiment's report is not.

The status taxonomy is deliberately fail-closed: if you're unsure whether something counts as a full PASS, it doesn't.

- **PASS** — every validation command that was run completed successfully, and the stated goal was actually met.
- **FAIL** — at least one required validation command failed, or the goal was not met.
- **PARTIAL** — some validation ran and passed, but coverage was incomplete (e.g. `full` skipped for a change that would have warranted it), or the goal was only partly achieved.
- **NOT RUN** — validation was not executed at all for this experiment.
- **NOT APPLICABLE** — the change has no meaningful validation command (e.g. pure documentation with no behavioral claim attached).

Never round a result up to PASS to make it look more finished than it is.
