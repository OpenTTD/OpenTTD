---
schema: knowledge/v1
id: the-null-video-driver-never-drains-the-main-thread-queue
kind: knowledge
class: constraint
title: "Under -v null, work queued with VideoDriver::QueueOnMainThread never runs, so an MCP screenshot writes no file"
description: "Before using a capture or any queued main-thread work in a null-video (regression or headless) run."
status: verified
epistemics: observed
date: 2026-11-17
tags: [core-loop, gui-and-console]
about: src/video/null_v.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/video/null_v.cpp
    - file:src/video/video_driver.cpp
    - file:src/video/dedicated_v.cpp
    - file:src/screenshot.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/core-loop/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/gui-and-console/README.md
---

# Statement

`DrainCommandQueue` is called from `src/video/video_driver.cpp:VideoDriver::Tick` and from the
dedicated driver's main loop (`src/video/dedicated_v.cpp`), but not from
`src/video/null_v.cpp:VideoDriver_Null::MainLoop`. The fork's `src/screenshot.cpp:QueueQuietScreenshot`
goes through that queue, so it silently never completes under the null driver.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote the-null-video-driver-never-drains-the-main-thread-queue` with evidence, or `majordomus knowledge reject the-null-video-driver-never-drains-the-main-thread-queue --reason ...`.

# Evidence

Re-verified against master at 5778b8909c (triage session ff4a0d12, 2026-11-17), read only:
- src/video/null_v.cpp:49 VideoDriver_Null::MainLoop runs GameLoop/InputLoop/UpdateWindows per tick and never calls DrainCommandQueue.
- DrainCommandQueue is called at src/video/video_driver.cpp:135 (VideoDriver::Tick) and src/video/dedicated_v.cpp:221 (dedicated MainLoop).
- src/screenshot.cpp:188-190 QueueQuietScreenshot posts through VideoDriver::QueueOnMainThread; its MCP caller is src/prismatic_mcp/prismatic_mcp_view.cpp:497.
Command: grep -n DrainCommandQueue src/video/null_v.cpp src/video/video_driver.cpp src/video/dedicated_v.cpp
