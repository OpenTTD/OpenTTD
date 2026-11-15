---
id: majordomus.completion-gates
version: 1
kind: rule
title: Completion is gated
description: A task reaches the outcome completed only when no validation gate its own change set selects is known to be failing, and a gate's verdict discharges nothing once the files that select it have changed since the run.
statement: The gates a change must pass are derived from the repository's CI model and the change's own paths; a gate known to be failing, or whose recorded run no longer describes the tree, refuses the outcome completed; a gate that has never reported is recorded as unverified and never as passing; and no surface may reach that judgement a second time.
status: active
class: blocking
depends_on: [majordomus.obligation-closure@1]
tags: [tasks, evidence, completion, ci]

x-majordomus:
  validator: completion_gates
  category: gate
  policy_key: gates_passed
  enforced_by: [check, finish]
  exit_code: 10
  tests: [test/cases/131_completion_gates.sh, test/cases/578_an_unknown_gate_refuses_completed.sh]
---

# Rationale

`majordomus.obligation-closure` made a task owe what its `requires` declares, and it made
that evidence expire. What it left untouched is everything the *validation pipeline* knows.
A repository's gates — the suites, the linters, the projection checks, the probes — were a
property of a pull request and not of a task, so a task could be finished with the crate's
own check red, or with the behavioural suite never having been run at all, and the record
would say completed.

Two measurements decide the shape of this rule rather than the shape a first draft would
take.

**A verdict that never arrived reads exactly like one that passed.** On 2026-09-10, in the
Majordomus repository, where this rule was written, four runs on its trunk had every Linux job
finished — five of them red — while three macOS jobs had been queued for between three and
seven hours, so the verdict those failures were supposed to reach had not run. The trunk
carried the defects overnight with nothing showing red anywhere.
`majordomus.never-reported-is-not-green` states the principle; this rule is where it becomes a
status a machine holds, distinct from both `pass` and `unknown`.

**A gate's own output can be flattered.** A coverage gate that counts its test code in its
own denominator reports a higher percentage every time tests are added, whether or not
anything is better covered. So nothing here reads what a gate printed: it reads the gate's
exit status and the hash of the files the gate was run over. That is a claim a worker cannot
improve by writing more code.

# Required behaviour

**Applicability is derived.** Which gates a task must pass is computed from the repository's
CI model — the gates it declares and the path classes that select them — applied to the
task's own change set, which is every path between the commit the task started at and the
working tree, uncommitted files included. No list of "source implies tests" is written
anywhere: the classes already say which paths select which gates, and a class edited today
changes the answer today. A gate the change cannot affect is `exempt`, which is a different
finding from one that cannot be judged.

**A gate run is evidence, and it expires.** A run is recorded against the task as a ledger
line naming the gate, its exit status and the hash of the files that select it — the union
of the paths of the classes that name the gate, and everything for a gate every plan
selects. The run discharges the gate only while that hash still equals the recomputed one.
Evidence that cannot expire is a claim about the past presented as a claim about the
present, which is the argument `majordomus.obligation-closure` already makes about
obligations, applied to gates for the same reason.

**Only a verdict that was read can back `completed`.** With the outcome `completed`, a
required gate that reported a non-zero exit, one whose run no longer describes the tree, and
one that cannot run because something it depends on is in either state, each refuse. So does
a checkout that cannot read the verdict at all — a CI model that is declared and does not
parse, a reader that is not built, that errors or answers nothing, no `jq`: that is `unknown`,
and an unknown that let `completed` through would be a check that passes whenever it cannot
look. A repository that declares no CI model has no gate to be unknown about and refuses
nothing. A gate that has never reported does not refuse either — the executable did reach
that verdict, `queued` — it is reported as unverified, by name, with the rule that says why silence is
not green. An outcome other than `completed` refuses nothing at all — a task reporting
itself blocked is being honest, and refusing that teaches a worker to claim completed
instead.

**One judgement.** The status of every gate is computed once, by the executable, and read by
every surface: the command line through `check` and `finish`, the HTTP API, MCP, and any
dashboard. A surface that recomputes it is how two surfaces come to disagree about whether
the same work is finished.

# Failure behaviour

A violation is a `FAIL` finding under the category `gate`, naming the gate, its status, the
reason in the vocabulary above — the exit status it reported, or both hashes when the
evidence has gone stale — and the command that would settle it. `finish` exits 10 and writes
nothing. A checkout that cannot reach the judgement at all (an unreadable model, no built
reader, a reader that fails) reports `unknown` by name and refuses `completed` with that
finding; `check` and every other outcome report it without refusing.

# Verification

`mj_validate_completion_gates` decides it, dispatched from `check, finish`, reading the
`gates.completion` capability rather than judging for itself. The behavioural case
`test/cases/131_completion_gates.sh` proves the verdicts, and
`test/cases/578_an_unknown_gate_refuses_completed.sh` proves that an unreadable verdict refuses
`completed` and nothing else; CI runs both.
