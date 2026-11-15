---
id: majordomus.ledger-integrity
version: 1
kind: rule
title: Ledger integrity
description: Every line of the append-only ledger is a well-formed event; the one durable record nothing else can reconstruct stays readable.
statement: Every line of the append-only ledger is a well-formed event.
status: active
class: blocking
depends_on: [majordomus.sessions-are-workers@1]
tags: [records, ledger]

x-majordomus:
  validator: ledger
  category: records
  enforced_by: [check, doctor, watch]
  exit_code: 10
  claims: [ledger-integrity]
  tests: [test/cases/22_history.sh]
---

# Rationale

Every line of the append-only ledger is a well-formed event; the one durable record nothing else can reconstruct stays readable.

# Required behaviour

Every line of the append-only ledger is a well-formed event.

# Failure behaviour

A violation is a `FAIL` finding under the category `records`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_ledger` decides it, dispatched from `check, doctor, watch`. The behavioural case `test/cases/22_history.sh` proves it, and CI runs that case.
