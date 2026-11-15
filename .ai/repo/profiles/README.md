---
schema: context/v1
id: ai.repo.profiles
kind: context
title: Profiles
description: How much context, effort and ceremony a task gets, declared per working mode.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Profiles

A profile is one working mode: how much context a task of that kind is assembled with,
how much effort it is worth, how the worker reports, and what it must produce before it
may finish. One YAML file per mode, its `name` equal to the file name; the policy names
the default (`profiles.default`), and a task carries the profile it was started with.

The keys are closed — a key nothing reads is an error rather than a silent no-op.
`context.*` switches the sections the context builder assembles, and `requires.*` is what
`majordomus finish` refuses to skip. Nothing here decides what a worker does; it decides
how much the tool spends on helping.

Add one by writing the file and running `majordomus doctor`, which parses every profile
and resolves the default.
