---
id: majordomus.justified-escalation
version: 1
kind: rule
title: Escalate capability and effort only when justified
description: The profile sets the default capability and effort; escalation is recorded, never assumed.
statement: Run at the profile's capability class and effort, and when you raise either, record that you did and why.
status: active
class: advisory
depends_on: []
tags: [principle]
---

# Rationale

Reasoning at maximum by default is a cost without a decision behind it. A profile is the decision; an escalation is a second decision that leaves a record.

# Required behaviour

Run at the profile's capability class and effort, and when you raise either, record that you did and why.

# Failure behaviour

Nothing decides this rule directly, so a violation is reported by nobody; it is the rules that depend on it that decide what a machine can decide and stop or warn accordingly.

# Verification

The enforced rules whose `depends_on` names this one carry the validators, the tests and the CI wiring. `majordomus doctrine list` shows them.
