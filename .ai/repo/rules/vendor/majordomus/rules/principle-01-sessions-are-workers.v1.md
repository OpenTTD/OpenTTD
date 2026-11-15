---
id: majordomus.sessions-are-workers
version: 1
kind: rule
title: Sessions are workers, not memory
description: Durable state lives in records the repository keeps, never in a conversation.
statement: Treat every session as a worker that reads state from records and writes state to records; nothing that only a conversation remembers is state.
status: active
class: advisory
depends_on: []
tags: [principle]
---

# Rationale

A session ends and takes everything it merely remembered with it. The environments this tool was distilled from kept gigabytes of session notes standing in for a database, recovered by a hand-written runbook. State that survives is state that was written down deliberately, by a command, where the next worker reads it.

# Required behaviour

Treat every session as a worker that reads state from records and writes state to records; nothing that only a conversation remembers is state.

# Failure behaviour

Nothing decides this rule directly, so a violation is reported by nobody; it is the rules that depend on it that decide what a machine can decide and stop or warn accordingly.

# Verification

The enforced rules whose `depends_on` names this one carry the validators, the tests and the CI wiring. `majordomus doctrine list` shows them.
