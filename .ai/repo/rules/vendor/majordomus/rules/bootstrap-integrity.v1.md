---
id: majordomus.bootstrap-integrity
version: 1
kind: rule
title: Bootstrap integrity
description: The path from a human reader to the AI layer is unbroken — README.md names AGENTS.md, every generated instruction file points at .ai/README.md, and none of them carries a rule of its own.
statement: A person reaches the AI layer through README.md and AGENTS.md, every generated instruction file points at .ai/README.md, and none of them carries a rule of its own.
status: active
class: blocking
depends_on: [majordomus.minimum-sufficient-context@1]
tags: [bootstrap, projection]

x-majordomus:
  validator: bootstrap
  category: bootstrap
  enforced_by: [doctor, watch]
  exit_code: 10
  claims: [projection-generation, context-budget, bootstrap-chain]
  tests: [test/cases/03_update.sh, test/cases/899_a_bootstrap_names_only_rules_in_force.sh]
---

# Rationale

The path from a human reader to the AI layer is unbroken — README.md names AGENTS.md, every generated instruction file points at .ai/README.md, and none of them carries a rule of its own.

# Required behaviour

A person reaches the AI layer through README.md and AGENTS.md, every generated instruction file points at .ai/README.md, and none of them carries a rule of its own.

A generated instruction file names only rules in force. Naming a rule the repository does not have is carrying a rule of its own: it exists in that file and nowhere else. Every rule id the generated content names — the whole file in file mode, the region in region mode; `<namespace>.<slug>`, optionally `@<version>`, outside a fenced block and not part of a path, file name, host or address — resolves in the effective rule set, the set `majordomus rules list` lists, where a deprecated rule is not in force. Each id that does not resolve is a finding under the category `rule-refs` naming the file, the line and the id, graded by whether `majordomus update` fixes it:

- `WARN`, with `majordomus update` as its reproduce command, when the content still matches its stamp and what update renders from the current template no longer names the id: a stale bootstrap an older template wrote. An upgrade never refuses a commit for a file update rewrites.
- `FAIL` when the current template names the id itself, so update would write it again, or when the content no longer matches its stamp, so somebody wrote the line and update will not replace it.

# Failure behaviour

A violation is a `FAIL` finding under the category `bootstrap` (`rule-refs` for a rule id that does not resolve), and the command that found it exits 10; a stale bootstrap that `majordomus update` would correct is a `WARN` and does not change the exit code. Under `watch` the same violation is reported as drift and the command exits 11.

# Verification

`mj_validate_bootstrap` decides it, dispatched from `doctor, watch`. The behavioural cases `test/cases/03_update.sh` and `test/cases/899_a_bootstrap_names_only_rules_in_force.sh` (the rule ids and both grades) prove it, and CI runs them.
