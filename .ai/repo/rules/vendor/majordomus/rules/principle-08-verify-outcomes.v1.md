---
id: majordomus.verify-outcomes
version: 1
kind: rule
title: Verify outcomes, not activity
description: A test that ran and passed is evidence; a sentence saying it did is not.
statement: Accept a verification command's recorded exit code as evidence of completion, and accept nothing that merely describes verification.
status: active
class: advisory
depends_on: []
tags: [principle]
---

# Rationale

Activity is easy to report and impossible to check. An outcome that a command produced, with its exit code and duration recorded, is checkable by anyone later.

# Required behaviour

Accept a verification command's recorded exit code as evidence of completion, and accept nothing that merely describes verification.

# Failure behaviour

Nothing decides this rule directly, so a violation is reported by nobody; it is the rules that depend on it that decide what a machine can decide and stop or warn accordingly.

# Verification

The enforced rules whose `depends_on` names this one carry the validators, the tests and the CI wiring. `majordomus doctrine list` shows them.
