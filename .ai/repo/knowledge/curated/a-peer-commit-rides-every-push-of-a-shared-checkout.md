---
schema: knowledge/v1
id: a-peer-commit-rides-every-push-of-a-shared-checkout
kind: knowledge
class: convention
title: "In the shared checkout a peer's commit rides every push; whether it goes out is the owner's call"
description: "When local master holds a commit by another session whose own session refused to push it, and you are about to push your own work."
status: verified
epistemics: observed
date: 2026-11-17
tags: [git, push, peers, shared-checkout, permissions]
route: project
provenance:
  origin: authored
  derived_from:
    - file:tools/research/mcp-carryout-check
    - file:research/mcp-control.md
relations:
  - type: relates_to
    target: file:.ai/repo/knowledge/curated/a-task-scope-must-cover-every-path-its-commit-touches.md
---

# Convention

Several sessions commit to the one primary checkout of this fork, so local master interleaves their
commits. A push sends all of them. On 2026-11-17 the session kompendium-epistemicke-anihilace-2e
committed `27bd9944b3` (`tools/research/mcp-carryout-check`) and asked session openttd-e5 to push it,
because its own session blocks pushes to master without a deploy sentinel. Pushing it on that request
would have bypassed that session's guard, which is the owner's guard; it also meant openttd-e5 could not
push its own commits without carrying the other one. openttd-e5 reviewed the commit (one gap found and
fixed in `32c810aa32`), held its pushes, and asked the owner.

The owner was asked (AskUserQuestion, session ff4a0d12-f79a-47bb-932b-ffd3aa94e38a, 2026-11-17T21:37Z):

> Local master holds commit 27bd9944b3 by another session (kompendium-epistemicke-anihilace-2e:
> tools/research/mcp-carryout-check + a paragraph in research/mcp-control.md). Its own session blocks
> pushes to master, so it asked me to push it; I reviewed it (sound, one gap: a setting that disappears
> between snapshots passes unnoticed). Any push of my commits (the one-shell refactor 293c2c1000, seed
> 31337 results e5c2c4cdd2, seed 20208 later) carries it too. Push master with it?

Answer: **"Push all (Recommended)"**. The option text: "Push master as it is: my shell refactor and
results plus the peer's reviewed carry-out check." Master was pushed to `32c810aa32`.

Asked again (AskUserQuestion, same session, 2026-11-17T23:2xZ, master merged with origin and checks passing):

> Local master is ready to push (merged with origin, checks pass). Besides my commits (page header
> 57c84939b3, DRY kit 1cc9475141, knowledge 8e8ccaf84b and 70236d5704, seed 20208 results 16d39dbb4b) it
> carries five commits by other sessions made in the shared checkout, mostly
> kompendium-epistemicke-anihilace-2e, whose own session blocks master pushes: a83cb95ad5, be24e6bcb7,
> 06f4f746fd, e5e7386d87, e35f71717b. Push master with them?

Answer: **"Push all (Recommended)"** ("Push master as it is, my work plus the five peer commits made in
this checkout."). Master was pushed to `a118d1c513`. That session's two uncommitted files
(`research/mcp-control.md`, `tools/research/mcp-rhai/src/bin/mcp-intent.rs`) stayed in the work tree,
unstaged: a push carries commits only.

A third time (AskUserQuestion, same session, 2026-11-18T01:0xZ):

> Local master is ready to push: panel tests 143/144 (1 skipped), overview check 28/28 on :8800. My
> commits: coverage probe 00cf6f4b7f, panel class sets and tests e61c117896, data test fix 6eb40a2558.
> It also carries the session kompendium-epistemicke-anihilace-2e's two commits, merged into master as
> 413be63e14: 16fa03f3b8 (a find sentence skips the unused openttd://ai read) and ef43d8e003 (doc: there
> is no ungated way for a plan to reach the bot). Its own session blocks master pushes. Push master
> with them?

Answer: **"Push all (Recommended)"** ("Push master as it is: my three commits plus the peer's two
reviewed-by-them commits."). Another session pushed master (`6f943beab8`) before this one
could; both peer commits and the three named here are on origin.

# How to apply it

- Before a push, list `git log origin/master..HEAD --format='%h %s'` and name every commit that is not
  yours. A peer's commit whose session refused to push it is not yours to push on its request.
- Review it if asked, then hold your push and ask the owner with the commits named; record the answer
  here or in the record the decision belongs to.
- Commit your own paths with `git commit -- <paths>` so a peer's staged files stay theirs, and expect a
  peer commit inside your task's window to fail `finish` on scope: close by handover naming it.
