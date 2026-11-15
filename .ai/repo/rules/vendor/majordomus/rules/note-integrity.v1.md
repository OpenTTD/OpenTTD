---
id: majordomus.note-integrity
version: 1
kind: rule
title: Note integrity
description: Every outcome needs a note carrying the sections that outcome requires, and no transcript.
statement: Every outcome needs a note carrying the sections that outcome requires, and never a transcript.
status: active
class: blocking
depends_on: [majordomus.handovers-carry-state@1]
tags: [finish, records]

x-majordomus:
  validator: note
  category: note
  enforced_by: [finish]
  policy_key: note_present
  exit_code: 10
  claims: [handover-record, no-transcripts]
  tests: [test/cases/06_finish.sh]
---

# Rationale

Every outcome needs a note carrying the sections that outcome requires, and no transcript.

# Required behaviour

Every outcome needs a note carrying the sections that outcome requires, and never a transcript.

# Failure behaviour

A violation is a `FAIL` finding under the category `note`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_note` decides it, dispatched from `finish`. The behavioural case `test/cases/06_finish.sh` proves it, and CI runs that case.
