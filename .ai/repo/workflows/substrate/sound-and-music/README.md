---
schema: context/v1
id: fork.substrate.sound-and-music
kind: context
title: "Sound and music: sound and music drivers, the mixer thread, sound effects, sound loaders, base sound and music sets"
description: "Read before changing a sound or music driver, the mixer, a sound loader or resampler, how a game event plays a sound effect, the music player, or base sound/music set handling."
status: active
scope: directory
providers: ["*"]
audience: [human, agent]
composition: extend
order: 210
tracks:
  - 'src/sound/**'
  - 'src/music/**'
  - 'src/sound.cpp'
  - 'src/sound_func.h'
  - 'src/sound_type.h'
  - 'src/mixer.*'
  - 'src/music.cpp'
  - 'src/music_gui.cpp'
  - 'src/soundloader*'
  - 'src/soundresampler*'
  - 'src/base_media_sounds.h'
  - 'src/base_media_music.h'
  - 'src/driver.*'
---

# Sound and music

Read at f8b1d52e80.

## Purpose and ownership

Owns audio output: the `SoundDriver` and `MusicDriver` families chosen from the driver registry, the
software mixer that sound drivers pull samples from, the sound effect table and its lazy decoding,
the playing of effects at a tile, vehicle or screen position, and the music player with its
playlists. Base set discovery is shared with graphics
([graphics and sprites](../graphics-and-sprites/README.md)); NewGRF sound slots and the vehicle
sound callback are in `src/newgrf_sound.cpp` ([NewGRF](../newgrf/README.md)). Audio is output only:
it never enters game state and may differ on every peer (ADR 0016). No fork hunk
(`git log --oneline upstream/master..HEAD -- src/sound src/music src/mixer.cpp src/sound.cpp` is empty).

## Entry points

- `src/driver.h:DriverFactoryBase` and `src/driver.cpp:DriverFactoryBase::SelectDriverImpl` - one
  registry for video, sound and music; each backend registers a static factory with a priority.
- `src/sound/sound_driver.hpp:SoundDriver` (`MainLoop`, `HasOutput`, `GetInstance`);
  `src/music/music_driver.hpp:MusicDriver` (`PlaySong`, `StopSong`, `IsSongPlaying`, `SetVolume`).
- Null drivers: `src/sound/null_s.h:SoundDriver_Null` (`HasOutput` false) and
  `src/music/null_m.h:MusicDriver_Null` (`IsSongPlaying` always true).
- `src/mixer.cpp:MxMixSamples` (called by the driver's audio callback), `MxAllocateChannel`,
  `MxSetChannelRawSrc`, `MxActivateChannel`, `MxCloseAllChannels`, `MxSetMusicSource`, `MxInitialize`.
- `src/sound.cpp:SndPlayTileFx`, `SndPlayVehicleFx`, `SndPlayFx`, `SndClickBeep`, `SndConfirmBeep`;
  private `StartSound` and `SndPlayScreenCoordFx`.
- `src/sound.cpp:InitializeSound` (opens the bank of the used set), `SndCopyToPool`, `ChangeSoundSet`.
- `src/soundloader.cpp:LoadSound` / `LoadSoundData`: providers `src/soundloader_type.h:SoundLoader`
  (raw, wav, opus) and `src/soundresampler_type.h:SoundResampler` (soxr), both
  `src/provider_manager.h:PriorityBaseProvider`.
- `src/music_gui.cpp:MusicLoop`, `MusicSystem` (playlists, `Shuffle`, `Play`, `CheckStatus`),
  `InitializeMusic`; `src/music.cpp:GetMusicCatEntryData` reads songs out of CAT files.
- `src/music/midifile.hpp:MidiFile` (`LoadFile`, `LoadMpsData`, `GetSMFFile` writes a temporary SMF
  for drivers that only play files).
- Sets: `src/base_media_sounds.h:BaseSounds`, `src/base_media_music.h:BaseMusic` (`NUM_SONGS_AVAILABLE`,
  `MusicSongInfo`); `src/music.cpp:DetermineBestSet`, `src/sound.cpp:DetermineBestSet`.

## Lifecycle

- Startup (`src/openttd.cpp:openttd_main`): after the video driver and `HandleBootstrap`,
  `BaseSounds::FindSets`/`SetSetByName`, `BaseMusic::FindSets`/`SetSetByName`, then
  `DriverFactoryBase::SelectDriver` for `Driver::Type::Sound` and `Driver::Type::Music` (`-s`, `-m`
  or the config's `_ini_sounddriver`/`_ini_musicdriver`). `-D` forces both to `null`.
- Autoprobe tries priorities 10 down to 1 and starts the first driver whose `Start` succeeds; a named
  driver that fails to start is a `UserError`.
- A sound driver's `Start` calls `MxInitialize(rate)` and opens a platform callback that calls
  `MxMixSamples` on the audio thread; the music stream of `fluidsynth` is mixed in there through
  `MxSetMusicSource`.
- Per world `src/misc.cpp:InitializeGame` calls `InitializeSound` and `InitializeMusic`; NewGRF
  loading rebuilds the sound pool via `SndCopyToPool` (`src/newgrf_sound.cpp`).
- A game event calls `SndPlay*Fx` -> `StartSound`: allocate a free channel, decode on first use
  (`LoadSound`, resampled to `MxGetRate`), set volume and pan, activate.
- Per frame `src/openttd.cpp:GameLoop` calls `SoundDriver::MainLoop` (only allegro overrides it)
  and `MusicLoop`, which advances the playlist when the driver says the song ended.

## Invariants

- **[verified]** Null sound and music drivers have priority 1, so autoprobe falls back to them; the
  null video driver has priority 0 and is never probed (`src/sound/null_s.h`, `src/music/null_m.h`,
  `src/video/null_v.h`, loop bounds in `SelectDriverImpl`).
- **[verified]** The mixer has eight channels tracked by atomic bitmasks; the game side allocates and
  activates, only `MxMixSamples` closes a finished channel, and `MxCloseAllChannels` only requests a
  stop (`src/mixer.cpp`). The music callback is serialised by `_music_stream_mutex`.
- **[verified]** A playing channel holds a `shared_ptr` to its sample data, so `ChangeSoundSet` may
  replace pool entries while sounds play; NewGRF-overridden slots (`grf_container_ver != 0`) are kept.
- **[verified]** A sound that fails to decode is marked by clearing `SoundEntry::file` and is never
  retried (`src/sound.cpp:SetBankSource`, `StartSound`).
- **[verified]** Effects are only heard through a viewport that covers the position; volume comes
  from that viewport's zoom (`_vol_factor_by_zoom`) and pan from the screen x position
  (`SndPlayScreenCoordFx`). `SndPlay*Fx` itself ignores the sound settings; callers check
  `_settings_client.sound.*`.
- **[verified]** Music shuffling uses `InteractiveRandom`, never the game `Random`
  (`src/music_gui.cpp:MusicSystem::Shuffle`, `PlaylistAdd`).
- **[supported]** Sound is client-side: the `SndPlay*Fx` callers in simulation code run on every peer
  but their only effect is local audio; a condition that mixes a client sound setting with `Random()`
  must draw first (knowledge `evaluate-random-before-client-settings`).
- **[inferred]** `PlayVehicleSound` skips the NewGRF sound callback when the client disabled vehicle
  sounds, so that callback must stay free of side effects or peers would diverge.

## Extension points

- New sound driver: `SoundDriver` subclass whose `Start` calls `MxInitialize` and feeds the platform
  buffer from `MxMixSamples`, plus a `DriverFactoryBase` with `Driver::Type::Sound` and a priority;
  add it under `if(NOT OPTION_DEDICATED)` in `src/sound/CMakeLists.txt` with a `CONDITION`.
- New music driver: same pattern with `MusicDriver`; use `MidiFile::GetSMFFile` if it plays files, or
  `MxSetMusicSource` to render PCM into the mixer.
- New sample format: a `SoundLoader` with a static instance (priority sorted, raw is the fallback);
  a resampler is a `SoundResampler` (`src/soundresampler_soxr.cpp`).

## Traps and forbidden operations

- Never let a sound driver, the mixer or a sound setting change game state or the game random
  stream; never read audio state (channel use, song position) in a command or tick handler.
- Under `-s null` nothing ever calls `MxMixSamples`, so no channel is ever closed: after eight
  effects `MxAllocateChannel` returns `nullptr` and every later `StartSound` returns early. Counting
  sounds in a headless run measures this, not the game (knowledge
  `the-null-sound-driver-never-frees-a-mixer-channel`).
- `MxSetChannelRawSrc` assumes one extra padding sample; `LoadSoundData` appends it. A loader that
  bypasses `LoadSoundData` breaks the linear resampler.
- `MxInitialize` clears the music source (its rate may have changed); **[inferred]** a music driver
  mixing into the mixer must call `MxSetMusicSource` again after a sound driver restart.
- A named sounds or music set that is not found falls back to the best available set with a
  warning; when no set is found at all, startup is a `UserError`. The shipped
  `media/baseset/no_sound.obs` and `media/baseset/no_music.obm` are the silent fallback sets, and the
  main menu warns about the fallback sound set only when `HasOutput` is true
  (`src/openttd.cpp:SwitchToMode`).
- `extmidi` forks an external player (`src/music/extmidi.cpp`); `dmusic` runs its own playback
  thread behind `_thread_mutex`.

## Tests and validation

- No Catch2 test covers sound or music. Regression ctest cases run `-snull -mnull`
  (`cmake/scripts/Regression.cmake`), which only proves the null drivers load.
- Tier: `tools/gate.sh change`; a driver change needs a real launch with `-s <driver>` / `-m <driver>`
  on the platform that builds it (`openttd -h` lists drivers and sets).
- Debug: `-d driver=<n>` (probe and start of every driver; the mixer logs nothing), `-d grf=<n>` (each
  `LoadSound` with rate, bits and length), `-d misc=<n>`. Mixer cost is the framerate window's
  `Sound` element (`src/framerate_type.h:PerformanceElement`).

## Decisions, rules and knowledge

- ADR 0022 (drivers from a factory registry, null drivers make headless runs possible), ADR 0028
  (base sound and music sets, no proprietary data), ADR 0016 (only commands cross the wire; audio
  stays local), ADR 0027 (`add_files` conditions per backend).
- Rules: `project.game-state-mutates-only-through-commands`,
  `project.never-report-unrun-validation-as-pass`.
- Knowledge: `the-null-sound-driver-never-frees-a-mixer-channel`,
  `evaluate-random-before-client-settings`, `macos-missing-baseset-hangs-headless-runs`.
- Related maps: [core loop](../core-loop/README.md), [NewGRF](../newgrf/README.md),
  [graphics and sprites](../graphics-and-sprites/README.md).

## Typical tasks

- To play a sound for a new event: call `SndPlayTileFx`/`SndPlayVehicleFx` behind the matching
  `_settings_client.sound.*` check, after any `Random()` draw, from code every peer runs; pick an ID
  from `src/sound_type.h:SoundFx`.
- To debug silence: `-d driver=3` to see which driver started, check `HasOutput`, the sound set in
  `openttd -h`, and whether a viewport covers the source.
- To add a sample format: implement `SoundLoader::Load`, keep `LoadSoundData`'s asserts (mono, 8 or
  16 bit, non-zero rate) true.
- To change the music player: start at `MusicSystem` in `src/music_gui.cpp`; keep randomness on
  `InteractiveRandom`.
