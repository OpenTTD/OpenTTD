---
schema: knowledge/v1
id: fork-history-carries-issue-trailers
kind: knowledge
class: convention
title: "Every fork commit names the issue it served in a Refs trailer"
description: "When writing a commit on this fork, or tracing a commit to the plan."
status: verified
epistemics: decided
date: 2026-11-15
tags: [git, traceability, plan, history]
route: project
provenance:
  origin: authored
  derived_from:
    - issue:I0020
    - file:.ai/repo/project/project.yaml
relations:
  - type: relates_to
    target: file:.ai/repo/adrs/0010-majordomus-supervises-this-fork-and-the-ai-layer-is-its-trac.md
---

# Convention

A commit message ends with one `Refs: I00NN` trailer per issue it served, in the trailer block
with the `Co-Authored-By` lines. On 2026-11-15 the 24 fork commits on top of `upstream/master`
were rewritten with `git filter-branch --msg-filter` to add them. Every tree stayed
byte-identical (`master^{tree}` unchanged), and the pre-rewrite head is the tag
`archive/master-pre-trailers-2026-11-15`.

The MCP series was rebased onto master later the same day and its new commits carry `Refs:`
trailers for the issues they served; its old heads are under `archive/`.
