---
schema: knowledge/v1
id: a-task-scope-must-cover-every-path-its-commit-touches
kind: knowledge
class: lesson
title: "A task's scope must cover every path its commits touch; finish and the pre-push hook refuse anything outside it"
description: "When starting a Majordomus task for work that spans C++, tools/research, docs and the .ai layer."
status: verified
epistemics: observed
date: 2026-11-16
tags: [majordomus, lifecycle, scope, pre-push]
about: .ai/repo/workflows/task-lifecycle.md
route: platform
provenance:
  origin: authored
  derived_from:
    - issue:I0056
---

# Lesson

A task for I0056 was started for a one-file C++ change with scope `src/prismatic_mcp,research`. The
same commit then also changed `tools/research/` (panel validation, `mcp-route-check`) and `.ai/` (the
issue and a rule). `majordomus finish` refused the task with "outside claimed scope". The pre-push
hook runs `finish --check`, so it refused the push as well. A new task cannot start while that one is
active, and widening the claim after the fact would hide the miss. The honest way out was
`majordomus handover --close` with a note that names the miss and the paths outside the scope. The
push then went through.

# How to apply

- Start each task with the scope the change will really need. In this fork a feature usually spans
  `src/prismatic_mcp`, `tools/research`, `research` and `.ai`, so claim all four up front.
- If the work grows beyond the claim, finish or close the current task first, then start a new one
  with the wider scope before committing the new paths.
- When a refusal does happen, report it and close with a note that names the paths outside the
  scope. Do not work around it.
