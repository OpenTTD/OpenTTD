---
schema: context/v1
id: ai.repo.use-cases
kind: context
title: Use cases
description: The tasks people perform with the tool, each one a file with the commands, rules and claims it names and the scenario that proves it.
status: active
scope: subtree
providers: ["*"]
audience: [human, agent]
composition: extend
order: 100
---

# Use cases

A use case is a task somebody performs with the tool: one Markdown file with front matter
(the identity, the category, the commands, rules, claims, responsibilities and
applications it names, and a scenario) and a body (`# Situation`, `# Outcome`, and what
else the author says). The front-matter contract is `share/schemas/majordomus/use-case/use-case.v1.schema.json`.

The scenario is the proof: a fresh repository prepared by a named setup script, then real
invocations of the tool, each with its expected exit code and output. `majordomus usecase
run` executes it and records normalised evidence under the local half; a page that shows
what a command printed shows that evidence, never a pasted transcript.

`taxonomy.yaml` holds the categories, presentation only; membership is each use case's
own `category`, and every index, count and link is derived. `majordomus usecase coverage`
tallies every public command, guaranteed claim and MCP tool against the use cases that
name and run it; the policy's `use_cases.coverage` says which gaps fail. `majordomus
usecase scaffold --missing` writes a draft for each gap from what the tool already knows;
a draft never counts until it is made active.

What a use case must not carry: a command's description, a rule's text, a claim's
wording, captured output, a status somebody wrote by hand. Those are derived from the
objects it names and from the evidence.
