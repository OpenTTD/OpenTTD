---
id: project.no-upstream-interaction
version: 1
kind: rule
title: Nothing from this fork reaches upstream OpenTTD
description: korczis/OpenTTD is a personal research fork, public on GitHub; upstream's contribution policy closes LLM-generated work, and the research goal never needs a contribution. A push, issue or pull request against OpenTTD/OpenTTD is out of bounds, and the upstream remote is fetch-only.
statement: Never push to, open an issue against, or open a pull request against OpenTTD/OpenTTD; keep every commit, branch and experiment in korczis/OpenTTD, and keep the upstream remote fetch-only.
status: active
class: blocking
depends_on: []
tags: [governance, upstream, fork]
---

# Rationale

Upstream's `CONTRIBUTING.md` closes issues and pull requests that were written by a language
model and blocks repeat submitters. The fork exists to study agent-driven engineering on an
independent codebase (ADR 0001), so a contribution would breach upstream's policy and serve
no research purpose.

# Required behaviour

- `upstream` points at `https://github.com/OpenTTD/OpenTTD.git` with its push URL set to
  `no-push-to-upstream`; `git remote -v` shows both lines.
- `origin` (`korczis/OpenTTD`) is the only remote anything is pushed to.
- Upstream's contribution process is read as background, never followed as this fork's
  process.

# Failure behaviour

A push or a forge action aimed at upstream is refused by the worker before it is attempted.
There is no tool validator: the push URL guard is mechanical, the rest is review.

# Related

ADR 0001, ADR 0011, `AGENTS.md` (Ground rules).
