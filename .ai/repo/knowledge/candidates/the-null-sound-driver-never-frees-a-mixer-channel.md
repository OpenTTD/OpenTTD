---
schema: knowledge/v1
id: the-null-sound-driver-never-frees-a-mixer-channel
kind: knowledge
class: fact
title: "Under the null sound driver no mixer channel is ever freed, so after eight effects every StartSound returns early"
description: "When instrumenting or counting sound effects in a headless (-s null) research or regression run."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [sound-and-music, research]
about: src/mixer.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/mixer.cpp
    - file:src/sound.cpp
    - file:src/sound/null_s.h
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/sound-and-music/README.md
---

# Statement

A mixer channel is closed only inside `src/mixer.cpp:MxMixSamples` (finished samples, or a stop
requested by `MxCloseAllChannels`), and `MxMixSamples` is called only by a real driver's audio
callback. `src/sound/null_s.h:SoundDriver_Null` starts nothing and never calls `MxInitialize`, so
the eight channels that `src/sound.cpp:StartSound` activates stay active for the whole process; from
then on `MxAllocateChannel` returns `nullptr` and `StartSound` returns before loading or playing.
A hook that counts played sounds in a headless run sees at most eight, and the first eight effects
are still decoded from the sound set.

# Review

Authored from a read of the source at f8b1d52e80 (issue I0066); awaiting the owner's review.
`majordomus knowledge promote the-null-sound-driver-never-frees-a-mixer-channel` with evidence, or `majordomus knowledge reject the-null-sound-driver-never-frees-a-mixer-channel --reason ...`.
