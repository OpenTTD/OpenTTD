---
schema: skill/v1
id: close-a-task-after-a-history-rewrite
version: 1
title: Close a task after a history rewrite
description: End a Majordomus task whose recorded head a rebase or filter-branch removed from the history, without bypassing a hook.
status: draft
tags: [majordomus, history, lifecycle, push]
related: [derive-the-ai-layer-from-history]
inputs:
  - an active task whose `majordomus check` reports "diverged from recorded head"
  - the owner's explicit approval of the rewrite and of the force-push
outputs:
  - the task closed as handed_over with a derived handover
  - the owner's choice recorded as a decision, the blocking question resolved
  - the rewritten branch pushed with --force-with-lease and every hook run
---

# Purpose

Closes a task that `finish` can no longer accept because the commit it started from is no longer
in the history. It is not a way to accept work. The task ends `handed_over`, and the
acceptance it did not get stays visible. Knowledge record `a-history-rewrite-strands-the-active-task`
explains why it happens.

# When to use

`majordomus check` shows `FAIL state … diverged from recorded head` and `finish` refuses every
outcome, after a rewrite the owner approved (an upstream rebase, a trailer rewrite). Do not use it
to escape any other refusal.

# Procedure

1. Confirm the cause: `git merge-base --is-ancestor <recorded head> HEAD` exits non-zero, and
   `majordomus recover` reports nothing to recover.
2. Ask the owner how the task should end, and offer `handover --close`, leaving it blocked, or a
   one-off `--no-verify`. Never choose for them.
3. Record the answer verbatim: `majordomus decision add "<choice>" --why "<reason>" --rejected
   "<alternatives>" --evidence "<question, answer, session id, time>"`. Resolve any open
   question with `majordomus question resolve <n> --answer "<answer>"`.
4. `majordomus handover --derive --close`. It writes the handover from the task record, the
   ledger and git, and marks the task `handed_over`.
5. `majordomus finish --check` must print `already handed_over` and exit 0.
6. Push with a lease on the state that was approved: `git push --force-with-lease=<branch>:<old
   sha> origin <branch>`. The pre-push hook runs, and no `--no-verify` is used.

# Output

A handover file under `.ai/local/state/handovers/`, a decision entry naming the owner's answer, no
unresolved question for the task, and the remote branch at the rewritten head.
