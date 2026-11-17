---
schema: knowledge/v1
id: the-null-sound-driver-never-frees-a-mixer-channel
kind: knowledge
class: fact
title: "Under the null sound driver no mixer channel is ever freed, so after eight effects every StartSound returns early"
description: "When instrumenting or counting sound effects in a headless (-s null) research or regression run."
status: verified
epistemics: observed
date: 2026-11-17
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

# Evidence

Re-verified against master at 5778b8909c (triage session ff4a0d12, 2026-11-17), read only:
- src/mixer.cpp:38,42 MixerChannelMask is uint8_t over _channels[8]; MxCloseChannel (:110) is called only inside MxMixSamples (:146, :167); MxCloseAllChannels (:120) only sets _stop_channels, which MxMixSamples consumes.
- MxMixSamples is called only from real drivers' callbacks (cocoa_s.cpp:40, sdl3_s.cpp:39, win32_s.cpp:52, xaudio2_s.cpp:70, allegro_s.cpp:37); MxInitialize likewise only from those drivers.
- src/sound/null_s.h:18 SoundDriver_Null::Start returns nullopt and does nothing else.
- src/mixer.cpp:175 MxAllocateChannel returns nullptr when no channel is free; src/sound.cpp:111-112 StartSound returns on nullptr.
