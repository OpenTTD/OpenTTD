---
name: review
description: review the working diff against the claimed scope and the finish contract
---

# Review the working diff against the claimed scope and the finish contract

Review the uncommitted and committed work of task {{TASK_ID}} in {{REPOSITORY}}
on branch {{BRANCH}} (working tree {{WORKING_TREE}}).

Claimed scope: {{SCOPE}}

Decisions recorded during this task:
{{DECISIONS}}

Report, in this order, and nothing else:

1. Anything outside the claimed scope.
2. Any behaviour change with no test.
3. Any claim in the diff's own comments or documentation that no test proves.
4. Any decision visible in the diff that is not in the list above.
5. Defects, most severe first, each with the file, the line and how it fails.

Say "no findings" where there are none. Do not restate what the diff does.
