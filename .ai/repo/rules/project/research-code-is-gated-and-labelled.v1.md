---
id: project.research-code-is-gated-and-labelled
version: 1
kind: rule
title: Research-only code is compile-gated, default-off, labelled at the point of change and removable
description: Access to internals is allowed for scoped research, and the cost is containment. Research code compiles only under a default-OFF CMake option, carries a RESEARCH-ONLY, UNSTABLE or DEBUG-ONLY label, and never bleeds into a default or release build.
statement: Put research-only code behind a default-OFF CMake option, label it RESEARCH-ONLY, UNSTABLE or DEBUG-ONLY where it is written, keep it to what the experiment needs, and keep default and release builds free of it.
status: active
class: blocking
depends_on: []
tags: [research, instrumentation, containment]
---

# Rationale

ADR 0004 opens the internals to research. What keeps that from becoming permanent debug
output is the compile gate: an absent symbol can be proven absent (`strings` on a
baseline-release binary), and a runtime flag cannot.

# Required behaviour

- Gate: `OPTION_RESEARCH_INSTRUMENTATION` (defines `OTTD_RESEARCH_INSTRUMENTATION`), or an
  option that requires it, such as `OPTION_PRISMATIC_MCP_SERVER`.
- Label: the doc comment says what the code exposes, whether it can affect behaviour, and its
  observer effects.
- A diagnostic interface binds to loopback only unless there is an explicit reason and
  protection for anything else.
- Verify: a change to gated code is built with the gate ON, because a default build does not
  compile it: `tools/gate.sh research` (I0022). `tools/gate.sh change` names that tier when the
  change touches a file carrying a research guard.

# Failure behaviour

Ungated research code, or a research symbol in a baseline-release binary, blocks the change.

# Related

ADR 0004, ADR 0008, `cmake/Options.cmake`, `src/console_cmds.cpp`.
