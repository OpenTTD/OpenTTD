---
id: majordomus.task-on-the-mesh
version: 1
kind: rule
title: A task on a repository whose mesh is enabled works on the mesh
description: When the repository declares its mesh enabled, an active task's scope is claimed on the mesh — `start` brings the checkout's shared server up and claims it, `check` and `finish` claim it again when a server answers — and a task completed without that claim is refused.
statement: On a repository whose tracked mesh declaration is enabled, a task is not completed until its scope has been claimed on the mesh as the session task-<id>; partial, blocked, no_match and failed are never refused for it.
status: active
class: blocking
depends_on: []
tags: [mesh, coordination, session, doctrine]

x-majordomus:
  validator: task_on_the_mesh
  category: mesh
  enforced_by: [check, finish]
  exit_code: 10
  claims: []
  tests: [test/cases/3068_a_task_off_the_mesh_is_refused.sh, test/cases/3067_a_task_is_claimed_on_the_mesh.sh, test/cases/3151_a_task_is_claimed_when_it_starts.sh]
---

# Rationale

A repository enables its mesh so that every worker on every machine sees what every other
one is building. A protocol that holds only while each worker remembers to claim what it
works on is advice, and it fails exactly when it matters: in the Majordomus repository, on
2026-10-15, a new session reached the mesh only as a nameless connection, a task's scope
reached it only when the worker remembered, and a milestone written five days earlier to
change that had never reached a pull request. `start` claims the task's scope on the mesh
when the checkout's shared server answers; this rule is what makes that claim more than a
convenience.

# Required behaviour

When a tracked declaration under `.ai/repo/mesh/` says `enabled: true`, an active task's
record carries `mesh_claim:` — the key of its scope's advisory claim as the mesh session
`task-<id>`. `start` writes it: it first ensures the checkout's shared server — the same
bounded `serve ensure` the provider's start event runs, under the same switches
(`session.ensure_server_on_start`, `MAJORDOMUS_RUNTIME=off`), said on stderr — because a fresh
linked worktree has no server until something brings one up. A start that still could not
claim says so as a WARN finding naming why and the repair. `check` claims the scope of the
running task when the record has none and a server answers, and `finish` does the same before
the contract is judged, so a server that came up after the task started is all a task needs;
`finish --check` is a question and claims nothing.

# Failure behaviour

`check` reports the missing claim under `mesh`. `finish --outcome completed` refuses it, and
the refusal is recorded like every other, naming this rule. `partial`, `blocked`, `no_match`
and `failed` are not refused: a worker whose server never came up still has a way out. A
repository that declares no enabled mesh is not judged at all.
