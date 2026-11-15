---
schema: adr/v1
id: adr-0006
kind: adr
title: The research CLI keeps the system compiler when it supports source_location and falls back to Homebrew LLVM only when it does not
status: proposed
date: 2026-07-24
tags:
  - toolchain
  - macos
  - build
related:
  - file:tools/research/lib/common.sh
  - file:tools/research/research
  - file:research/tooling.md
  - file:CLAUDE.md
provenance:
  origin: extracted
  derived_from:
    - commit:6f36288ff9cd5cd8e9a857c85575c6e75f87e350
    - commit:19b32cafd6c8742b0eeb266017541b07403fc75e
    - file:tools/research/lib/common.sh
    - file:research/tooling.md
---

# 6. The research CLI keeps the system compiler when it supports source_location and falls back to Homebrew LLVM only when it does not

## Context

`src/stdafx.h` includes `<source_location>`, which older Command Line Tools on macOS lack.
The research CLI first forced Homebrew's LLVM whenever brew was present and `CC`/`CXX` were
unset. On a current Xcode, whose system compiler already has the header, that override broke
the build: Homebrew LLVM's libc++ header paths did not match the macOS SDK, producing hundreds
of `unknown type name 'size_t'` errors, while `tools/gate.sh` built fine with the system
compiler.

## Decision

`configure` probes the system compiler first with a compile-only test
(`cxx_supports_source_location`) and falls back to Homebrew LLVM only when the probe fails.
An explicit `CC`/`CXX` always wins.

## Alternatives rejected

- **Always use Homebrew LLVM on macOS.** Breaks current Xcode installs.
- **Never override.** Breaks older CLT-only installs.

## Consequences

- On this machine, configure selects `/usr/bin/c++` with no override (observed again on
  2026-11-15).
- The toolchain decision is recorded in each experiment's manifest with the rest of the build
  facts.
