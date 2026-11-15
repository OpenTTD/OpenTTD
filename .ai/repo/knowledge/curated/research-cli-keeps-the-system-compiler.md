---
schema: knowledge/v1
id: research-cli-keeps-the-system-compiler
kind: knowledge
class: lesson
title: "Forcing Homebrew LLVM on a current Xcode breaks the build; probe the system compiler first"
description: "When a research-profile configure fails with hundreds of 'unknown type name size_t' errors, or when choosing a toolchain on macOS."
status: verified
epistemics: observed
date: 2026-07-24
tags: [toolchain, macos, llvm, source-location]
about: tools/research/lib/common.sh
route: generator
provenance:
  origin: extracted
  derived_from:
    - commit:6f36288ff9cd5cd8e9a857c85575c6e75f87e350
    - file:tools/research/lib/common.sh
    - issue:I0007
relations:
  - type: documents
    target: file:.ai/repo/adrs/0006-the-research-cli-keeps-the-system-compiler-when-it-supports-.md
  - type: documents
    target: file:.ai/repo/project/issues/I0007.yaml
---

# Lesson

The `<source_location>` gap is a property of old Command Line Tools, not of macOS. Homebrew
LLVM's libc++ header paths can disagree with the current SDK, so an unconditional override
turns a working build into a broken one. The compile-only probe
(`cxx_supports_source_location`) decides; an explicit `CC`/`CXX` overrides the probe.

# Observed again

2026-11-15: `configure` printed "system compiler (c++) supports <source_location> -- using it,
no toolchain override needed", and the rebased tree built with it.
