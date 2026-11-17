---
schema: knowledge/v1
id: dedicated-console-reads-one-buffered-line-per-select
kind: knowledge
class: lesson
title: "A dedicated server's console runs one line per select(): lines written together wait in the stream buffer"
description: "When a tool writes several console commands to a dedicated server's stdin in quick succession (the panel, a script, a ~ plan answer)."
status: verified
epistemics: observed
date: 2026-11-16
tags: [console, dedicated-server, stdin, panel]
about: src/video/dedicated_v.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - issue:I0069
    - file:src/video/dedicated_v.cpp
    - file:tools/research/mcp-rhai/src/panel.rs
    - commit:60aef3caa5529bbe769e8c5c423615aefb7d196a
---

# Lesson

`DedicatedHandleKeyInput()` (`src/video/dedicated_v.cpp`) runs a console line only when
`InputWaiting()` reports input, which is `select()` on the stdin descriptor, and then reads the line
with `std::getline(std::cin, ...)`. When several lines arrive together, the first `getline` pulls
all of them into the stream's buffer and runs one. `select()` then sees an empty descriptor, so the
other lines wait in the buffer until something else is written.

The panel's answer to a `~` plan request is several `echo` lines. Its first line showed at once,
the rest minutes later, each one released only by the next unrelated console write.

# How to apply

- Write console commands one line at a time, with a gap longer than one game-loop pass. The panel's
  `Game.console()` serialises its writes under a lock and keeps 0.25 s between them.
- A test of a console round trip reads the answer from the panel's event feed (`console.line`
  events). The 150-line log tail is not enough, because a building bot scrolls lines out of it
  within seconds.
