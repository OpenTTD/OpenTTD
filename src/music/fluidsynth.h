/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fluidsynth.h Base for FluidSynth music playback. */

#ifndef MUSIC_FLUIDSYNTH_H
#define MUSIC_FLUIDSYNTH_H

#include "music_driver.hpp"

/** Music driver making use of FluidSynth. */
class MusicDriver_FluidSynth : public MusicDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const MusicSongInfo &song);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
	/* virtual */ const char *GetName() const { return "fluidsynth"; }
};

/** Factory for the fluidsynth driver. */
class FMusicDriver_FluidSynth : public DriverFactoryBase {
public:
	FMusicDriver_FluidSynth() : DriverFactoryBase(Driver::DT_MUSIC, 5, "fluidsynth", "FluidSynth MIDI Driver") {}
	/* virtual */ Driver *CreateInstance() const { return new MusicDriver_FluidSynth(); }
};

#endif /* MUSIC_FLUIDSYNTH_H */
