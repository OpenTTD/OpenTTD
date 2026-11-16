---
schema: adr/v1
id: adr-0022
kind: adr
title: Platform backends are drivers registered in factories and chosen at runtime, so one binary can run with a window, headless or silent
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - platform
  - video
  - sound
  - music
related:
  - file:.ai/repo/workflows/substrate/platform-and-files/README.md
  - file:.ai/repo/workflows/substrate/graphics-and-sprites/README.md
  - file:.ai/repo/workflows/substrate/sound-and-music/README.md
  - file:.ai/repo/workflows/substrate/core-loop/README.md
  - file:src/driver.h
provenance:
  origin: extracted
  derived_from:
    - file:src/driver.h
    - file:src/driver.cpp
    - file:src/video/CMakeLists.txt
    - issue:I0063
---

# 22. Platform backends are drivers registered in factories and chosen at runtime, so one binary can run with a window, headless or silent

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

OpenTTD runs on Windows, macOS, Linux, BSDs and as a dedicated server, with several possible
video, sound and music backends per platform, and must still start when a backend is missing.

## Decision

1. **Factory registry.** Each backend has a static `src/driver.h:DriverFactoryBase` instance that
   registers itself (by type, name and priority) in `DriverFactoryBase::GetDrivers`.
2. **Runtime selection.** `DriverFactoryBase::SelectDriver` picks the named driver (`-v`, `-s`,
   `-m`, or the config) or the highest-priority one that starts; null drivers always start.
3. **Build-time availability.** Which backends exist in a binary is decided by CMake conditions
   (`src/video/CMakeLists.txt` adds none of the windowed drivers under `OPTION_DEDICATED`).

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- The video driver owns the real-time pump that calls the game loop (core-loop map), so the
  driver choice changes timing and threading but must never change the simulation.
- Headless runs use the dedicated or null video driver; the null driver never drains the main
  thread queue (candidate `the-null-video-driver-never-drains-the-main-thread-queue`).
- A new backend is a new factory instance plus a CMake condition, not a change to callers.

## For this fork

- Research runs are headless (`-vnull`/dedicated) with null sound and music, and record which
  drivers ran in the manifest (ADR 0005); a GUI client's MCP view tools need a real video driver
  (ADR 0013).
- Maps: [platform and files](../workflows/substrate/platform-and-files/README.md),
  [graphics](../workflows/substrate/graphics-and-sprites/README.md),
  [sound and music](../workflows/substrate/sound-and-music/README.md).
