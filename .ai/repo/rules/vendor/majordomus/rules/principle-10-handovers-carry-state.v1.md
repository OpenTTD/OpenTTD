---
id: majordomus.handovers-carry-state
version: 1
kind: rule
title: Handovers transfer state, not transcripts
description: A handover says what is true now and the one next action; never conversation history.
statement: Write a handover as the current state and the next action, with computed front matter, and never paste a transcript, a diff or a narrative of the session into any record.
status: active
class: advisory
depends_on: []
tags: [principle]
---

# Rationale

A transcript is the past as one worker experienced it. The next worker needs the present as the repository has it. A record with required sections and computed identity fields is the difference.

# Required behaviour

Write a handover as the current state and the next action, with computed front matter, and never paste a transcript, a diff or a narrative of the session into any record.

# Failure behaviour

Nothing decides this rule directly, so a violation is reported by nobody; it is the rules that depend on it that decide what a machine can decide and stop or warn accordingly.

# Verification

The enforced rules whose `depends_on` names this one carry the validators, the tests and the CI wiring. `majordomus doctrine list` shows them.
