---
schema: context/v1
id: fork.process
kind: context
title: Working on the fork's own process, site and history
description: What binds a change to the fork's documentation, its site, its CI workflows and its history.
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 200
tracks:
  - 'README.md'
  - 'AGENTS.md'
  - 'CLAUDE.md'
  - 'GEMINI.md'
  - 'zola/**'
  - '.github/workflows/gh-pages.yml'
---

# Working on the fork's own process, site and history

## Rules that apply

- `project.no-upstream-interaction`: upstream is fetch-only.
- `project.no-commit-without-current-instruction`: covers rebases and force pushes too.
- `project.personal-tooling-stays-uncommitted`: `.claude/`, `.aiad/`, `.agents/`, `.codex/`.

## Decisions and knowledge

- ADR 0001 (private research target), ADR 0003 (one-way adaptation from Prismatic), ADR 0009
  (Zola site), ADR 0010 (Majordomus supervision), ADR 0011 (rebase onto upstream).
- Following upstream: `.ai/repo/workflows/upstream-sync.md`.
- Knowledge `fork-history-carries-issue-trailers`, `upstream-rebase-2026-11-15`.
- Commit messages end with `Refs: I00NN` for the issue they serve. The plan is under
  `.ai/repo/project/`; `majordomus plan next` names the next issue.
