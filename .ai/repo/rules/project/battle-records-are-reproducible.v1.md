---
id: project.battle-records-are-reproducible
version: 1
kind: rule
title: A battle record names everything that reproduces each match, comes from a committed edition, and the leaderboard is derived from it
description: Every research/results/battle-*.json names, for each match, the arena build, seed, start order and length, and for the edition a clean commit of PrismaticAI and the referee; its .md and the README leaderboard are derived from it. battle.py check enforces this in tools/gate.sh change.
statement: Publish AI-battle results only as records written by battle.py report from a committed edition, never edit a record or its derived Markdown by hand, and keep battle.py check passing.
status: active
class: blocking
depends_on: [project.docs-are-derived-and-current@1, project.never-report-unrun-validation-as-pass@1]
tags: [ai-battle, results, reproducibility, derived, enforcement]
---

# Rationale

A ranking that cannot be replayed is an anecdote. A match is a pure function of its arena build,
seed, start order and length within one edition (ADR 0035), so a record that names all of them
is reproducible by anyone with the same bundle. A record from a dirty edition names a commit that
does not contain the code that played. A leaderboard typed into a README is a second source of
truth (rule `project.docs-are-derived-and-current`).

# Required behaviour

- Write records with `tools/research/ai-battle/battle.py report`. It writes
  `research/results/battle-<utc>-<edition>.json`, the derived `.md` next to it, and the
  leaderboard region of `research/ai-battle/README.md`.
- Prepare the edition from committed code (`battle.py prepare` without `--allow-dirty`).
- Do not edit a record, its `.md`, or the region between `<!-- battle:begin -->` and
  `<!-- battle:end -->`.
- A claim about which AI is stronger names the record it comes from, and holds only for that
  record's edition and match parameters.

# Failure behaviour

`battle.py check` (run by `tools/gate.sh change`) exits 10 when a record is missing a field, comes
from a dirty edition, has a match that does not name its arena build, has ticks that do not match
its length, or has a leaderboard that is not derived from its matches. It exits 11 when a `.md`
or the README region is not what the newest record renders.

# Verification

`python3 tools/research/ai-battle/battle.py check`, and the offline tests in
`tools/research/test_ai_battle.py`.
