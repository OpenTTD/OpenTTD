---
schema: knowledge/v1
id: a-language-pack-must-match-the-binarys-english-txt
kind: knowledge
class: fact
title: "A .lng file built from a different english.txt is skipped without an error; with none valid the game refuses to start"
description: "When running a binary away from its build directory, mixing lang/ directories, or diagnosing \"No available language packs\"."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [strings-and-languages, platform-and-files]
about: src/strings.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/strgen/strgen_base.cpp
    - file:src/strgen/strgen.cpp
    - file:src/strings.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/strings-and-languages/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/platform-and-files/README.md
---

# Statement

strgen writes `LANGUAGE_PACK_VERSION` into `table/strings.h` and every `.lng` header; it is a hash
over each string's ID, name and counted commands (`src/strgen/strgen_base.cpp:StringData::Version`),
so adding, renaming or re-parameterising any English string changes it.
`src/strings.cpp:LanguagePackHeader::IsValid` rejects a header with another version, and
`src/strings.cpp:InitializeLanguagePacks` logs the rejected file only at `-d misc=3` and stops with
"No available language packs (invalid versions?)" when nothing valid is left on any search path.
Ship or point a binary at the `lang/` directory built with it.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0063); awaiting the owner's review.
`majordomus knowledge promote a-language-pack-must-match-the-binarys-english-txt` with evidence, or `majordomus knowledge reject a-language-pack-must-match-the-binarys-english-txt --reason ...`.
