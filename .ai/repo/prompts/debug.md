---
name: debug
description: frame a defect so that the fix is proven, not asserted
profile: debugging
---

# Frame a defect so that the fix is proven, not asserted

Task {{TASK_ID}} on branch {{BRANCH}} at {{HEAD}}: {{TASK}}

Scope: {{SCOPE}}

Open questions that block acceptance:
{{OPEN_QUESTIONS}}

Work in this order and do not skip a step:

1. Reproduce the defect and record the exact command and output.
2. Isolate the cause. State what you ruled out and how.
3. Write the failing test first. It must fail for the recorded reason.
4. Fix the cause, not the symptom.
5. Prove it: the new test passes, the suite passes, and you name both commands.

Record any choice a later reader would question with
`majordomus decision add "<what>" --why "<why>"`. If you are blocked on someone else,
`majordomus question add "<question>"` and stop; do not guess.
