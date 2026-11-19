---
id: project.prismatic-ai-passes-the-headless-fitness-check
version: 1
kind: rule
title: A change to PrismaticAI or the battle tooling passes the headless fitness check before it is committed
description: tools/research/ai-battle/battle.py fitness runs the working tree's PrismaticAI in one short, deterministic null-driver match against a reference AI and fails on a Squirrel error from any PrismaticAI, an unfinished match, or a collapsing fleet of a contestant held healthy; tools/gate.sh change runs it whenever tools/research/prismatic-ai or tools/research/ai-battle changes.
statement: Do not commit a change under tools/research/prismatic-ai or tools/research/ai-battle unless battle.py fitness passes, or says NOT RUN because no arena cache exists on this machine and you say so.
status: active
class: blocking
depends_on: [project.never-report-unrun-validation-as-pass@1, project.battle-records-are-reproducible@1]
tags: [prismatic-ai, ai-battle, regression, fitness, enforcement]
---

# Rationale

PrismaticAI ran in the LAN AI battle with its fleet shrinking by a third to a half in every match,
and no test noticed (knowledge `prismatic-ai-without-a-steward-keeps-one-train-per-line`). The
bot's behaviour is tested by duels, which take wall-clock minutes per game month and run in no
gate. A null-driver match runs six game years in about ten seconds and is deterministic for one
tree and one binary, so it can run on every change to the bot.

# Required behaviour

- Run `tools/gate.sh change`, which runs `battle.py fitness` when the bot or the battle tooling
  changed, or run `battle.py fitness` directly. Do it before committing a change under
  `tools/research/prismatic-ai` or `tools/research/ai-battle`.
- A contestant whose health a change is meant to keep goes into `fitness.must_be_healthy` in
  `tools/research/ai-battle/battle.json`. Do not remove one to make a change pass.
- When the check says NOT RUN (no `build/openttd`, or no arena cache from `battle.py fetch` and
  `battle.py prepare`), report it as not run (rule `project.never-report-unrun-validation-as-pass`).

# Failure behaviour

`battle.py fitness` exits 10 in these cases:

- the match did not finish its final year,
- any PrismaticAI logged a Squirrel error,
- a contestant held healthy ended below `fitness.rules.fleet_keep` of its fleet peak (when that
  peak was at least `fleet_min_peak` vehicles).

The gate fails with it.

# Verification

`python3 tools/research/ai-battle/battle.py fitness`; the offline tests of the verdict in
`tools/research/test_ai_battle.py` (`Health`).
