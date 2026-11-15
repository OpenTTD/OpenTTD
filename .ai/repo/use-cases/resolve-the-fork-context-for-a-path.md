---
id: resolve-the-fork-context-for-a-path
kind: use-case
title: 'Resolve what governs a path of the fork before touching it'
summary: 'Ask which context documents apply to an engine source file or a research file, and get the fork layer that names its rules, ADRs and knowledge.'
category: policy
status: active
target: advisory
weight: 10
actors: [agent, human]
difficulty: beginner
commands: [context]
applications: [govern-a-research-fork-of-a-large-codebase]
---

# Situation

The fork holds two kinds of files: the OpenTTD engine under `src/`, where upstream's
invariants still apply (command system, savegame versioning), and the fork's own research layer.
A worker that edits `src/command.cpp` with only the research layer in mind breaks lockstep
determinism, and one that edits `research/` with only the engine in mind skips the experiment
provenance. The path decides which half applies.

# What you run

- `context resolve src/command.cpp`: the chain for engine source ends in `fork.substrate`
- `context resolve research/README.md`: the chain for the research layer ends in `fork.research-layer`

# Scenario

```yaml
mode: live
given:
  - 'the fork layer with its three context documents tracked'
steps:
  - id: engine-source
    run: ['context', 'resolve', 'src/command.cpp']
    note: 'engine source resolves to the substrate document, which carries the upstream invariants'
    expect:
      exit: 0
      stdout_contains: ['ai.layer', 'fork.substrate']
      stdout_not_contains: ['fork.research-layer']
  - id: research-layer
    run: ['context', 'resolve', 'research/README.md']
    note: 'the research layer resolves to its own document, which carries the experiment rules'
    expect:
      exit: 0
      stdout_contains: ['ai.layer', 'fork.research-layer']
      stdout_not_contains: ['fork.substrate']
then:
  - 'the same layer root applies everywhere and the nearest document adds to it'
  - 'engine paths and research paths never resolve to the same chain'
```

# Outcome

The worker reads the rules that govern the path before editing it, and nobody has to remember
which half of the fork a file belongs to.
