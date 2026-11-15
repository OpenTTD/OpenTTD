---
schema: knowledge/v1
id: a-history-rewrite-strands-the-active-task
kind: knowledge
class: lesson
title: "Rewriting history under an active task leaves finish nothing to accept"
description: "Before rewriting fork history while a Majordomus task is open, or when finish refuses with 'diverged from recorded head'."
status: verified
epistemics: observed
date: 2026-11-15
tags: [majordomus, history, lifecycle, push]
route: project
provenance:
  origin: authored
  derived_from:
    - issue:I0020
    - file:.ai/repo/skills/close-a-task-after-a-history-rewrite/SKILL.md
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/fork-history-carries-issue-trailers.md
  - type: relates_to
    target: file:.ai/repo/adrs/0010-majordomus-supervises-this-fork-and-the-ai-layer-is-its-trac.md
---

# Lesson

A task records the head it started from. Task `t-20261115175003-e0e1` started at `75082b7`, and
the `Refs:` trailer rewrite of the same session replaced every fork commit. The recorded head
stopped being an ancestor of `HEAD`, and the blocking rule `majordomus.state-consistency` then
refused every `finish` outcome, `partial` included. `majordomus recover` found nothing to
recover, and the pre-push hook (`finish --check`) blocked the force-push the owner had approved.

The supported way out is `majordomus handover --derive --close`. It ends the task as
`handed_over`, which is not an acceptance, and the hook then passes on its own terms. The
procedure is the skill `close-a-task-after-a-history-rewrite`.

# How to avoid it

Rewrite history before `majordomus start`, or close the task first and start a new one after
the rewrite. A task that spans a rewrite cannot be finished, only handed over.
