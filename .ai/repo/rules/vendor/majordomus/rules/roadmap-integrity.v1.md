---
id: majordomus.roadmap-integrity
version: 1
kind: rule
title: The roadmap is a projection, never a document
description: No document is a second authority for the roadmap, and no milestone types its own version. While a hand-written roadmap table exists, it can neither list a release no milestone was released in nor hide one that carried a milestone; a milestone's release (released_in) is inferred from the release records, never typed.
statement: No document is a second authority for the roadmap; a milestone's release is inferred from the release records, never typed, and a hand-written table can neither list a release no milestone was released in nor hide one that carried a milestone.
status: active
class: blocking
depends_on: [majordomus.define-done-first@1]
tags: [plan, roadmap]

x-majordomus:
  validator: roadmap
  category: roadmap
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [roadmap-derived]
  tests: [test/cases/49_roadmap_doctrine.sh, test/cases/3102_a_milestone_version_is_inferred.sh]
---

# Rationale

No document is a second authority for the roadmap, and no milestone types its own version. While a hand-written roadmap table exists, it can neither list a release no milestone was released in nor hide one that carried a milestone; a milestone's release (released_in) is inferred from the release records, never typed.

# Required behaviour

No document is a second authority for the roadmap; a milestone's release is inferred from the release records, never typed, and a hand-written table can neither list a release no milestone was released in nor hide one that carried a milestone.

A milestone record carries no `version`; `plan validate` refuses one as an unknown key. The
release a milestone shipped in is `released_in`: the first release record
(`.ai/repo/releases/v*.yaml`, the tracked statement of a `vMAJOR.MINOR.PATCH` tag, in version
order) whose `commit:` contains the merge of its last accepted issue — the DONE issue with the
latest `completed_at` — found through the commit that wrote that stamp, or failing that the
latest merge whose message names the issue. Records and not tags, because derived data must
not depend on refs outside the tree. Releases before the first record have none, so work
finished before it reads as the earliest release this layer records (v0.3.1 in Majordomus's
own repository). A milestone that is not DONE, or that no record's commit contains yet, reads
`unreleased`. Both engines answer it the same way (`plan roadmap`).

# Failure behaviour

A violation is a `FAIL` finding under the category `roadmap`, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_roadmap` decides it, dispatched from `doctor, watch`. The behavioural cases `test/cases/49_roadmap_doctrine.sh` and `test/cases/3102_a_milestone_version_is_inferred.sh` prove it, and CI runs them.
