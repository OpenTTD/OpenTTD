---
id: project.personal-tooling-stays-uncommitted
version: 1
kind: rule
title: The imported personal agent tooling stays local and uncommitted
description: .claude/, .aiad/, .agents/ and .codex/ hold personal commands, agents, skills and hooks imported for reference. They are excluded through .git/info/exclude, never .gitignore, and never committed; a principle worth keeping is written once into the shared layer instead.
statement: Never commit .claude/, .aiad/, .agents/ or .codex/; exclude them through .git/info/exclude, and write any principle worth keeping from them once into AGENTS.md or .ai/.
status: active
class: blocking
depends_on: []
tags: [tooling, privacy, layout]
---

# Rationale

The exclusion is personal to this machine, not a rule every clone should inherit, which is why
it lives in `info/exclude`. Copying from those trees would create a second source of truth
that goes stale first.

# Required behaviour

Stage paths explicitly (`git add <path>`), never `git add -A` at the root.

# Failure behaviour

A staged path under one of the four directories blocks the commit.

# Related

`AGENTS.md` (`.claude/` and `.aiad/`), ADR 0010.
