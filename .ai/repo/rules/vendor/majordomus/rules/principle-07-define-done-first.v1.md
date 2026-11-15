---
id: majordomus.define-done-first
version: 1
kind: rule
title: Define done before executing
description: The finish contract is evaluated line by line, and nothing else counts as done.
statement: Know the contract a task must meet before starting it, and let the contract, not the worker's word, decide whether it is finished.
status: active
class: advisory
depends_on: []
tags: [principle]
---

# Rationale

When done means the worker said so, every task is done. A contract written first is what makes a refusal possible, and a refusal is the whole point of supervision.

# Required behaviour

Know the contract a task must meet before starting it, and let the contract, not the worker's word, decide whether it is finished.

# Failure behaviour

Nothing decides this rule directly, so a violation is reported by nobody; it is the rules that depend on it that decide what a machine can decide and stop or warn accordingly.

# Verification

The enforced rules whose `depends_on` names this one carry the validators, the tests and the CI wiring. `majordomus doctrine list` shows them.
