---
schema: skill/v1
id: consult-an-external-model
version: 1
title: Consult an external model with derived context, and verify what it says
description: Ask another model (ChatGPT through codex, or any provider) a question about this repository with the context the layer resolves for the paths involved, judge its citations mechanically, and record only what was verified in the source.
status: draft
tags: [consultation, review, evidence, knowledge]
related: [load-focused-context-for-a-change, derive-the-ai-layer-from-history]
inputs:
  - a question, and the repository paths it is about
  - a reachable provider (CONSULT_CMD, codex or llm)
outputs:
  - a local consultation record under .ai/local/cache/consultations/ with its reference check
  - for each finding verified in the source, a knowledge candidate, an ADR amendment or an issue; nothing for the rest
---

# Purpose

A second, independent reader finds what the first one missed: an adversarial review of a map or a
decision, a security read of fork code, a check of an inference before it becomes a record. This
skill makes that consultation generic and safe. The context is derived (never pasted by hand), the
answer is treated as the weakest kind of evidence, and every file and symbol it cites is checked
against the tree before anyone acts on it. It is not a way to write records: a model's statement is
never a record until a worker has verified it in the source.

# When to use

- You are unsure of a claim and no peer on the board (`majordomus peers`) owns the area.
- Before proposing an ADR or a candidate that rests on an inference about fork or upstream code.
- To review a subsystem map or a change set adversarially, independent of the worker who wrote it.

# Procedure

1. Name the paths the question is about. `tools/knowledge/consult ask "<question>" --path <p>... --dry-run`
   prints the exact prompt: the instructions, the commit, and the context documents
   `majordomus context resolve` gives for those paths, nearest first, within `--budget` lines.
2. Ask: drop `--dry-run`. The provider is found: `CONSULT_CMD` (prompt on stdin, answer on stdout),
   else `codex` (`codex exec -s read-only`, which can also read the repository itself), else `llm`;
   `--provider` chooses. The answer and its reference check are written to
   `.ai/local/cache/consultations/` (ignored by git, never published); `tools/knowledge/consult list`
   lists them.
3. Read the reference check first. An `UNRESOLVED path:Symbol` is a fabricated or stale citation:
   discount every claim resting on it. A resolving citation proves only that the symbol exists.
4. Verify each finding you intend to use, in the source, yourself (`git grep`, read the function). Keep
   the model's label ([verified]/[supported]/[inferred]/[unknown]) or lower it; never raise it
   without your own evidence.
5. Record only what survived: a knowledge candidate under `.ai/repo/knowledge/candidates/`
   (`status: candidate`, `provenance.origin: authored`, the files in `derived_from`), an amendment to
   a proposed ADR's Consequences, or an issue. Say in the record that an external review found it and
   that it was checked here. Then run `majordomus knowledge check` and `tools/knowledge/refs check`.
6. Never send `.ai/local/`, secrets, credentials or personal data in a question. The repository is
   public; the question and the derived context are what leave the machine.

# Output

The consultation record path, the count of cited and unresolved references, and for each finding:
verified-and-recorded (with the record), rejected (with the evidence), or left open (with why).
