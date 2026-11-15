---
schema: knowledge/v1
id: gated-code-is-invisible-to-a-default-build
kind: knowledge
class: lesson
title: "A default build and its 98/98 tests say nothing about research code; verify with the option ON"
description: "When rebasing onto upstream, merging, or changing a header the gated code includes."
status: verified
epistemics: decided
date: 2026-11-15
tags: [research, instrumentation, verification, upstream]
about: cmake/Options.cmake
route: enforcement
provenance:
  origin: authored
  derived_from:
    - issue:I0021
    - file:cmake/Options.cmake
    - file:src/console_cmds.cpp
relations:
  - type: supports
    target: file:.ai/repo/rules/project/research-code-is-gated-and-labelled.v1.md
  - type: documents
    target: file:.ai/repo/workflows/upstream-sync.md
---

# Lesson

`research_status` and the MCP server compile only under `OTTD_RESEARCH_INSTRUMENTATION` (and
`OTTD_PRISMATIC_MCP_SERVER`). The 2026-11-15 rebase brought in 317 upstream commits that changed
internal APIs (`StringID` became a strong type, among others), and the one file both sides
touched was `src/console_cmds.cpp`, inside the gated block. A green `tools/gate.sh change`
could not have shown a break there. The research-debug profile did, and it built and ran.

# Observed again

The same day, rebasing the MCP series (`research/prismatic-mcp-server`, issue I0017) onto that
tree applied all five commits without a conflict in `src/`, and the result still did not compile
with `OPTION_PRISMATIC_MCP_SERVER=ON`: upstream had replaced `Debug(net, 1, ...)` with
`Debug(Facility::Net, Severity::..., ...)`. `tools/gate.sh change` built and passed 98/98 on it all
along; `tools/gate.sh research` failed with 20 errors until the calls were ported.
