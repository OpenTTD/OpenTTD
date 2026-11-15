---
id: validate-the-derived-fork-record
kind: use-case
title: 'Prove the record derived from the fork history still holds together'
summary: 'Run the knowledge and skill validators over the layer derived from the commits and confirm every reference resolves.'
category: knowledge
status: active
target: advisory
weight: 20
actors: [agent, reviewer]
difficulty: intermediate
commands: [knowledge, skills]
applications: [govern-a-research-fork-of-a-large-codebase]
---

# Situation

The milestones, issues, ADRs, knowledge records and skills of this fork were derived from its
commits and cite them by full hash. An upstream rebase or a trailer rewrite changes every hash,
and a record citing a hash that no longer exists is worse than no record. The record has to be
proven again after every rewrite, not trusted.

# What you run

- `knowledge check`: every curated record is valid and every reference resolves
- `skills check`: every repository skill parses and every related id resolves

# Scenario

```yaml
mode: live
given:
  - 'the fork layer derived from the history and committed'
steps:
  - id: knowledge
    run: ['knowledge', 'check']
    note: 'every curated record resolves'
    expect:
      exit: 0
      stdout_contains: [' 0 failing']
  - id: skills
    run: ['skills', 'check']
    note: 'every skill parses and its references resolve'
    expect:
      exit: 0
      stdout_contains: ['failures: 0']
then:
  - 'a rewrite that strands a cited commit fails here, not in a reader'
```

`plan validate` and `adr check` read nothing but the layer as well, yet they cannot run in a
live scenario: the command registry classifies `plan` and `adr` as state-mutating as whole
commands, because `plan start` and `adr propose` write. Run them on their own after a rewrite.

# Outcome

After a rebase, the derived record either still holds or names the record that broke. The skill
`derive-the-ai-layer-from-history` fixes what it names.
