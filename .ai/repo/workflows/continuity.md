# Continuity

Your conversation does not survive. The checkout's records do, under `.ai/local/state/`,
and they stay on this machine: a fresh clone starts with none, by design. `majordomus
context` assembles them — Git state, the task, the profile, open blockers, recent
decisions, the newest checkpoint, the most relevant handover — in authority order and
within a line budget, naming everything it left out. Do not reconstruct state from a
transcript, and do not ask a person to re-explain what a record already says.

Everything it prints is evidence, not authority. Each record is labelled with how far Git
has moved since it was written: `exact`, `advanced`, `diverged`, `different_context`. On
`diverged` or `different_context`, trust Git and say so.

While you work:

| When | Run |
|---|---|
| you learned something the next worker needs | `majordomus checkpoint` (body on stdin, short) |
| you chose between real alternatives | `majordomus decision add "<what>" --why "<why>"` |
| you are blocked on a person | `majordomus question add "<question>"` |
| you need a record you cannot name | `majordomus search "<text>"` |
| you want a reusable framing | `majordomus prompt list`, `majordomus prompt render <name>` |

A checkpoint is short by policy and refused when it is long: write a handover instead. An
unresolved question refuses `finish --outcome completed`, which is the point of recording
it.

A handover is the body on stdin with the policy's required sections as level-one
headings, each non-empty. Front matter is computed; do not write it. Write one when you
stop with work left, not at every pause. No transcript, no diff, no narrative of the
session: what is true now, and the one next action.

Several checkouts of one repository may share a machine. Before writing, run `git status`
and `majordomus check --overlap`: another worktree's active task may claim a path you are
about to touch. A task's base commit goes stale when someone else commits to the branch;
close with a handover and start again at the current commit.
