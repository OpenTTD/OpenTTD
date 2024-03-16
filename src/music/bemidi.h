/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bemidi.h Base of BeOS Midi support. */

#ifndef MUSIC_BEMIDI_H
#define MUSIC_BEMIDI_H

#include "music_driver.hpp"

/* For BMidiSynthFile */
#include <MidiSynthFile.h>

/** The midi player for BeOS. */
class MusicDriver_BeMidi : public MusicDriver {
public:
	const char *Start(const StringList &param) override;

	void Stop() override;

	void PlaySong(const MusicSongInfo &song) override;

	void StopSong() override;

	bool IsSongPlaying() override;

	void SetVolume(uint8_t vol) override;
	const char *GetName() const override { return "bemidi"; }

private:
	BMidiSynthFile *midi_synth_file = nullptr;
	double current_volume = 1.0;
	bool just_started = false;
};

/** Factory for the BeOS midi player. */
class FMusicDriver_BeMidi : public DriverFactoryBase {
public:
	FMusicDriver_BeMidi() : DriverFactoryBase(Driver::DT_MUSIC, 10, "bemidi", "BeOS MIDI Driver") {}
	Driver *CreateInstance() const override { return new MusicDriver_BeMidi(); }
};

#endif /* MUSIC_BEMIDI_H */
