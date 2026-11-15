---
id: majordomus.handover-integrity
version: 1
kind: rule
title: Handover integrity
description: The resolver runs and reports either the record for this worktree and branch or its clean absence; a malformed record is never silently skipped, and a record describing a history this checkout no longer has is reported.
statement: The record resolver reports the handover for this worktree and branch or its clean absence; a malformed record is never skipped silently.
status: active
class: blocking
depends_on: [majordomus.handovers-carry-state@1]
tags: [records, continuity]

x-majordomus:
  validator: resolver
  category: resolver
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [handover-record, divergence-label]
  tests: [test/cases/23_context.sh]
---

# Rationale

The resolver runs and reports either the record for this worktree and branch or its clean absence; a malformed record is never silently skipped, and a record describing a history this checkout no longer has is reported.

# Required behaviour

The record resolver reports the handover for this worktree and branch or its clean absence; a malformed record is never skipped silently.

# Failure behaviour

A violation is a `FAIL` finding under the category `resolver`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_resolver` decides it, dispatched from `doctor, watch`. The behavioural case `test/cases/23_context.sh` proves it, and CI runs that case.
