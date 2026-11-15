---
id: project.never-report-unrun-validation-as-pass
version: 1
kind: rule
title: A validation step that did not run is reported as NOT RUN, and an ambiguous result is not a pass
description: The fork's reports use the fail-closed taxonomy of research/README.md (PASS, FAIL, PARTIAL, NOT RUN, NOT APPLICABLE), with the exact commands and exit statuses that ran.
statement: Report every validation layer with the command that ran and its exit status, mark a layer that did not run as NOT RUN, and never round PARTIAL or ambiguous up to PASS.
status: active
class: blocking
depends_on: []
tags: [validation, reporting, evidence]
---

# Rationale

The fork measures how reliable agent-driven engineering is. A report that claims more than
was run destroys the measurement it was meant to provide (ADR 0002).

# Required behaviour

- Name the tier (`tools/gate.sh smoke|change|full`, `tools/research/research validate --gate`)
  and its exit status.
- A debug or instrumented result is never presented as release-build behaviour.
- A failing gate is reported as it is; an override exists only at the owner's request, with
  the reason attached. No `--no-verify`, and no "fixed in a follow-up".

# Failure behaviour

A claim without the command behind it is withdrawn and the step is reported as NOT RUN.

# Related

ADR 0002, ADR 0005, `research/README.md`, `research/experiment-template.md`.
