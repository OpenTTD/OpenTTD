---
name: continue
description: resume a task from durable state instead of from someone's memory
---

# Resume a task from durable state instead of from someone's memory

You are continuing work that another session started. Nothing about the previous
conversation survives; everything below was recorded deliberately.

{{CONTEXT}}

Before changing anything:

1. Verify the recorded state against the repository as it is now. The `task_record`
   label above tells you how far git has moved since the record was written; if it says
   `diverged` or `different_context`, trust git and say so.
2. Do the one thing named as the next action. If the records do not name one, say that
   rather than inventing scope.
3. Stay inside the scope shown above. Touching anything else fails `majordomus check`.
4. Checkpoint with `majordomus checkpoint` before you stop, and hand over with
   `majordomus handover` if the work is not finished.
