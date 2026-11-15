---
id: majordomus.externalise-decisions
version: 1
kind: rule
title: Externalise decisions and durable state
description: A decision you make goes into a record, dated, with what was rejected and why; a question for a person goes into the questions store.
statement: Record every choice between real alternatives with its reason, and every blocker that waits on a person, in the stores the commands read; never leave either in prose only you can find.
status: active
class: advisory
depends_on: []
tags: [principle]
---

# Rationale

A decision that exists only in a transcript is re-litigated by the next worker. A question that exists only in a handover paragraph does not block anything. Both become gates only when they are written where a command reads them.

# Required behaviour

Record every choice between real alternatives with its reason, and every blocker that waits on a person, in the stores the commands read; never leave either in prose only you can find.

# Failure behaviour

Nothing decides this rule directly, so a violation is reported by nobody; it is the rules that depend on it that decide what a machine can decide and stop or warn accordingly.

# Verification

The enforced rules whose `depends_on` names this one carry the validators, the tests and the CI wiring. `majordomus doctrine list` shows them.
