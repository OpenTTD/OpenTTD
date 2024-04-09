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
	std::optional<std::string_view> Start(const StringList &param) override;

	void Stop() override;

	void PlaySong(const MusicSongInfo &song) override;

	void StopSong() override;

	bool IsSongPlaying() override;

	void SetVolume(uint8_t vol) override;
	std::string_view GetName() const override { return "fluidsynth"; }
};

/** Factory for the fluidsynth driver. */
class FMusicDriver_FluidSynth : public DriverFactoryBase {
public:
	FMusicDriver_FluidSynth() : DriverFactoryBase(Driver::DT_MUSIC, 5, "fluidsynth", "FluidSynth MIDI Driver") {}
	Driver *CreateInstance() const override { return new MusicDriver_FluidSynth(); }
};

#endif /* MUSIC_FLUIDSYNTH_H */
