---
schema: knowledge/v1
id: a-running-bash-script-is-read-by-offset
kind: knowledge
class: lesson
title: "Bash reads a running script by byte offset: edit it only by appending"
description: "Before editing a tools/research shell script (mcp-duel, mcp-e2e, gate.sh ...) while a run of it may be in progress."
status: verified
epistemics: observed
date: 2026-11-17
tags: [bash, tooling, duel, concurrency]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-duel
relations:
  - type: relates_to
    target: file:research/mcp-control.md
---

# Lesson

Bash does not load a script before running it. It reads the next command from the file at the
offset where the previous one ended, so a heredoc (`python3 - <<'PY' ... PY`) is consumed when its
command starts, and the lines after it are read only once that command returns. A 120-month duel
spends hours inside the Python heredoc of `mcp-duel`. Inserting text inside or before that heredoc
while it runs shifts every later byte: when Python returns, bash resumes at the old offset, in the
middle of the new text, and runs fragments of it as shell commands.

# How to apply it

Before editing a script under `tools/research/`, check for a run of it (`pgrep -fl <script>`). While
one runs, either wait, or add the change only after the last byte the run has read (appending is
safe: the run picks the new lines up). A change inside the body waits for the run to end. The
README scoreboard step was appended to `mcp-duel` after its heredoc for this reason on 2026-11-17,
with a 120-month duel running.
