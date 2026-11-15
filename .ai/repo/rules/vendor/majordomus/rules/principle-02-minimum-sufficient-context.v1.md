---
id: majordomus.minimum-sufficient-context
version: 1
kind: rule
title: Load minimum sufficient context
description: The profile says what to load; discover everything that may apply and load only what does.
statement: Orient from the assembled context and the registered sections the task needs, never by reading the whole repository or pasting every file into the prompt.
status: active
class: advisory
depends_on: []
tags: [principle]
---

# Rationale

An always-loaded contract that oscillated between nothing and a thousand lines, across hundreds of hand edits, was pinned only by a hard budget with a failing check. Context is a budget, and discoverability is not eager loading.

# Required behaviour

Orient from the assembled context and the registered sections the task needs, never by reading the whole repository or pasting every file into the prompt.

# Failure behaviour

Nothing decides this rule directly, so a violation is reported by nobody; it is the rules that depend on it that decide what a machine can decide and stop or warn accordingly.

# Verification

The enforced rules whose `depends_on` names this one carry the validators, the tests and the CI wiring. `majordomus doctrine list` shows them.
