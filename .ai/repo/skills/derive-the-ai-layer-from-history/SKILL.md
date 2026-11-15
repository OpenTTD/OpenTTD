---
schema: skill/v1
id: derive-the-ai-layer-from-history
version: 1
title: Derive the .ai/ layer from the fork's history
description: Turn the fork's own commits into linked ADRs, plan issues, knowledge, features and why records, and prove every link resolves.
status: draft
tags: [majordomus, knowledge, adr, plan, traceability]
related: [close-a-task-after-a-history-rewrite]
inputs:
  - the upstream remote fetched (`git fetch upstream master --no-tags`)
  - an active task scoped to `.ai`
outputs:
  - ADRs, issues, knowledge records and features that cite full commit hashes
  - every Majordomus validator reporting zero failures
---

# Purpose

Keeps the governance record of this fork derived from what it did, never invented. A record
here cites the commits, files and rules it rests on. Anything git or the plan can compute is not
typed by hand: issue status, counts and dates.

# When to use

New fork commits have landed since the last derivation, or a section of `.ai/repo/` is empty
while the history already holds what it should say.

# Procedure

1. List the fork's own commits, not upstream's: `git log --reverse --format='%H %s'
   upstream/master..master`, plus each unmerged research branch measured from its own base
   (`git log master..<branch>`).
2. Group the commits by decision. A choice with rejected alternatives becomes an ADR under
   `.ai/repo/adrs/` (`majordomus adr propose`), with status `proposed`, because acceptance is
   the owner's. An execution contract becomes an issue under `.ai/repo/project/issues/`, named
   in the commits' `Refs:` trailers. A fact, lesson or constraint that no other file states
   becomes a curated knowledge record.
3. Cite full 40-character hashes as `commit:<sha>`. The knowledge validator refuses short ones.
4. Write YAML strings on one line. The plan parser does not read folded or wrapped scalars.
5. Link everything: ADR to rule and issue, knowledge `relations` to ADR and file, features and
   why records to the issues that deliver them.
6. Validate, each command run on its own: `majordomus plan validate`, `majordomus adr check`,
   `majordomus knowledge check`, `majordomus usecase validate`, `majordomus skills check`, then
   `majordomus doctor`.
7. Commit only on the owner's current instruction (rule
   `project.no-commit-without-current-instruction`), with `Refs:` trailers.

# Output

The new or updated records, the validators' zero-failure lines quoted in the task's evidence,
and one commit whose trailers name the issues it served.
