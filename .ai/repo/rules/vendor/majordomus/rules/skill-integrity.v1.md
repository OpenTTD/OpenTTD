---
id: majordomus.skill-integrity
version: 1
kind: rule
title: Skill integrity
description: Every skill the repository declares parses against the skill contract, names the directory it lives in, carries its sections, describes itself in terms no other skill uses, and every skill or example it refers to exists.
statement: A skill is canonical data under the skills section; it is valid against the skill schema, its id is its directory, its body carries the sections a reader relies on, its description tells it apart from every other skill, and every reference it makes resolves.
status: active
class: blocking
depends_on: [majordomus.minimum-sufficient-context@1]
tags: [skills]

x-majordomus:
  validator: skills
  category: skill
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [skill-catalogue, skill-check]
  tests: [test/cases/95_skills.sh]
---

# Rationale

A skill is loaded only when a task is about what it covers, so a broken one is not noticed until the moment a worker needs it. Validating every skill on every `doctor` run means the failure surfaces in the tree, with the file named, not in a session that loaded half a procedure. The catalogue every surface reads is derived from the same discovery the executable indexes, so a skill that exists for one interface exists for all of them.

# Required behaviour

Every file the source class `skill` discovers has front matter that satisfies `share/schemas/majordomus/skill/skill.v1.schema.json` (no unknown key, `schema: skill/v1`, an integer `version`, a `status` from the closed set), an `id` equal to its directory name, and a body with non-empty `# Purpose`, `# Procedure` and `# Output` sections. No two skills claim one id, and no two describe themselves the same way: a skill is chosen by what its description says it covers, so two skills that say the same thing cannot both be chosen and the one a worker wanted is unreachable. Descriptions are compared folded to lower case with runs of whitespace collapsed and trailing sentence punctuation dropped, so the difference has to be in what a description says rather than in how it is typed. Every `related` id names a skill in the catalogue, and every tracked example under a skill's `examples/` opens with a level-one heading.

# Failure behaviour

A violation is a `FAIL` finding under the category `skill`, naming the file and every reason, and the command that found it exits 10. Under `watch` the same violation is reported as drift and the command exits 11. A repository with no skills is reported as such, never as a pass over nothing.

# Verification

`mj_validate_skills` decides it, dispatched from `doctor, watch`; `majordomus skills check` runs the same examination on demand and prints its counts. The behavioural case `test/cases/95_skills.sh` proves it, and CI runs that case.
