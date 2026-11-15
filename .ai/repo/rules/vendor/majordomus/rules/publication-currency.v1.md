---
id: majordomus.publication-currency
version: 1
kind: rule
title: A session does not finish behind its own published site
description: A repository that publishes has a second tree — the one the public is being served — and no commit-level gate can measure it, because no change to any file can make it right or wrong. A session that lands work and leaves the publication owed has finished nothing the public can see, and every check stays green while that is true. The gates that measure a deployment are declared as such in the CI model, and finish asks them.
statement: A gate the CI model marks at-finish measures the deployment rather than the tree, so no path class can select it; finish runs it live and refuses the outcome completed while it reports the publication is not a projection of the trunk. It is measured, never recorded: evidence about a live site expires with the site and not with a file hash.
status: active
class: blocking
depends_on: [majordomus.completion-gates@1]
tags: [tasks, completion, deployment, evidence]

x-majordomus:
  validator: publication_currency
  category: gate
  policy_key: publication_current
  enforced_by: [finish]
  exit_code: 10
  tests: [test/cases/275_publication_is_current.sh, test/cases/578_an_unknown_gate_refuses_completed.sh]
---

# Rationale

`majordomus.completion-gates` made the validation pipeline's verdict a property of the task
rather than of a pull request. It derives which gates apply from the change set's own paths,
which is the right derivation for every gate that measures a tree — and it is structurally
incapable of reaching the ones that do not.

A repository that publishes has two trees. There is the one in the checkout, which every gate
in the model examines, and there is the one the public is actually being served, which none of
them can see. The second is not a function of the first: it is a function of whether a
deployment ran, whether it ran to completion, whether the hosting provider's own build of what
was pushed succeeded, and whether anything has landed since. **No edit to any file can make
that question come out differently**, so a path class can never select the gate that asks it,
and a gate in no class is asked only by a full plan — which, in a repository that plans its
full run on a push to its trunk, runs *before* the publication it would judge.

The result is a question nobody owns. Three instances measured in the Majordomus repository,
where this rule was written, each of which left every commit-level check green:

- an unmerged ref served as the public site for about half an hour, noticed by a person's eye;
- twenty pull requests landed in one afternoon and the site sat hours behind the trunk;
- the hosting provider's own build of a correctly pushed commit errored, and the previous
  commit was served for 27 minutes.

The moment this question is worth asking is not when a file changes. It is when a worker
says the work is done, because "done" and "the public cannot see it" is the contradiction the
finish contract exists to refuse.

**Why this is measured and not recorded.** `majordomus.completion-gates` discharges a gate
with a ledger line carrying the hash of the files the gate was run over, and the run stops
counting when that hash changes. That mechanism cannot hold this verdict, because its subject
is not in the tree: a publication check run ten minutes ago describes a site that may have
been superseded since, with every file in the repository untouched. Evidence whose subject
can change without the tree changing has no hash to expire against, so it is not stored. The
gate is run, live, at the moment of the claim.

# Required behaviour

**The CI model declares which gates measure a deployment.** A gate with `at-finish: true`
is one of them. Nothing is hardcoded here: a repository that publishes nothing marks no
gate, and the requirement then has nothing to run and says so.

**The policy selects the requirement.** `publication_current` in `verification.finish_requires`
is what turns it on, like every other line of the contract.

**`finish` asks it, and `check` does not.** `check` is run often and cheaply, and this
gate reaches the network and waits for a CDN; making it part of every `check` would be a
slow, flaky `check`, which is a `check` people stop running. The session-end hook writes a
handover rather than a finish, so a session that hands over is never asked either — for the
same reason an honest `blocked` is not: `handed_over` is a claim about unfinished work, and
this requirement is about a claim that the work is done.

**Only the outcome `completed` is refused.** A task reporting itself `partial`, `blocked`,
`failed` or `no_match` is being honest about unfinished work, and refusing that teaches a
worker to claim `completed` instead — the argument `majordomus.completion-gates` already
makes, applied here unchanged.

**A verdict that could not be reached is reported, never inferred.** The gate distinguishes
"the publication is not current" from "I could not find out" — no network, no published branch,
no hosting API. The first refuses. The second is reported by name, with its exit and reason,
and refuses `completed` too: a session on a train is not evidence that the site is stale, but
`completed` claims it is current, and a requirement that passes whenever it cannot measure is
decoration. It is never reported as the first, and an honest `partial` or `blocked` is never
refused over it. Silence is not green; it is named — the argument the Majordomus repository
states as `project.never-reported-is-not-green`, which the package cannot depend on because a
vendored rule must resolve inside the package it ships in.

# Failure behaviour

A violation is a `FAIL` finding under the category `gate`, naming the gate, the command that
was run and the reason the gate gave — which commit is published, which commit is on the
trunk, how long the publication has been owed. `finish` exits 10 and writes nothing, so the
task stays active and the record never says completed. The remediation is the deployment,
not a flag.

# Verification

`mj_validate_publication_currency` decides it, dispatched from `finish`, reading the gates
the CI model marks `at-finish` and running each one. `test/cases/275_publication_is_current.sh`
proves it against fixture repositories whose gate scripts are written by the case: one that
exits 0, one that exits 10, one that exits 12 (refused, and named as unreachable), one that
declares no such gate, and one finishing with an outcome other than `completed`;
`test/cases/578_an_unknown_gate_refuses_completed.sh` proves the unreachable refusal beside the
completion-gates reader's.
