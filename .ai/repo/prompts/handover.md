---
name: handover
description: produce a continuation record body for the current task
---

# Produce a continuation record body for the current task

Write the body of a handover for task {{TASK_ID}} ({{TASK}}) as of {{NOW}},
branch {{BRANCH}}, head {{HEAD}}.

Latest checkpoint:
{{CHECKPOINT}}

Write exactly these level-one sections, in this order, and put nothing else in the body:

# Objective
# Current State
# Next Action
# Completed
# Decisions
# Verification
# Risks
# Open Work

Rules the writer enforces and will reject you for breaking:

- No front matter, and no `head:`, `branch:` or other identity fields. Those are computed.
- No transcript, no narrative of the session, no diffs.
- `Current State` describes what is true in the files right now and is checkable.
- `Next Action` is one action, not a plan.
- `Verification` names commands that actually ran, with their exit codes.

Pipe the result into `majordomus handover`.
