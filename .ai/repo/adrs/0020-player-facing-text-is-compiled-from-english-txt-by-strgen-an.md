---
schema: adr/v1
id: adr-0020
kind: adr
title: Player-facing text is compiled from english.txt by strgen, and translations arrive from eints and are never edited by hand
status: proposed
date: 2026-11-16
tags:
  - upstream-architecture
  - strings
  - translation
  - build
related:
  - file:.ai/repo/workflows/substrate/strings-and-languages/README.md
  - file:.ai/repo/workflows/substrate/build-and-packaging/README.md
  - file:docs/eints.md
  - file:src/lang/CMakeLists.txt
provenance:
  origin: extracted
  derived_from:
    - file:docs/eints.md
    - file:src/lang/CMakeLists.txt
    - file:src/strgen/strgen.cpp
    - issue:I0063
---

# 20. Player-facing text is compiled from english.txt by strgen, and translations arrive from eints and are never edited by hand

This decision is upstream OpenTTD's, read out of the code at the commit this fork is based on; the fork did not make it and records it so that its maps, rules and research can point at one statement of it (ADR 0001: the substrate is the system under test).

## Context

OpenTTD ships dozens of languages with plural forms, genders and cases, and its strings carry
typed parameters. Translators are a separate community working in a web tool.

## Decision

1. **English is the master.** `src/lang/english.txt` is the only language file a code change
   edits; `strgen` turns it into the `StringID` table header and each language file into a
   compiled `.lng` (custom commands in `src/lang/CMakeLists.txt`).
2. **Translations are synchronised, not authored here.** Eints, OpenTTD's web translator,
   validates translations against the English parameters and commits them daily
   (`docs/eints.md`).
3. **Strings are IDs with parameters.** Code passes `StringID` values and parameters; the text is
   resolved at draw time in the player's language.

## Alternatives rejected

Upstream's deliberation is not in this repository, so no rejected alternative is recorded here and none is invented. What the code rules out is listed under Consequences.

## Consequences

- Never hand-edit a non-English language file; the next eints sync overwrites it.
- Changing a string's parameters or meaning needs a new string or follows the eints rules in
  `docs/eints.md`, or every translation breaks silently.
- A `StringID` value is not stable across builds; never persist it.

## For this fork

- The fork's research strings, if any, go into `english.txt` behind the same gate as their code,
  and the fork ships no translations of them (ADR 0001: nothing goes upstream).
- Map: [strings and languages](../workflows/substrate/strings-and-languages/README.md).
