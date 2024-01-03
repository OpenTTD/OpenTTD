/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file null_m.h Base for the silent music playback. */

#ifndef MUSIC_NULL_H
#define MUSIC_NULL_H

#include "music_driver.hpp"

/** The music player that does nothing. */
class MusicDriver_Null : public MusicDriver {
public:
	const char *Start(const StringList &) override { return nullptr; }

	void Stop() override { }

	void PlaySong(const MusicSongInfo &) override { }

	void StopSong() override { }

	bool IsSongPlaying() override { return true; }

	void SetVolume(byte) override { }
	const char *GetName() const override { return "null"; }
};

/** Factory for the null music player. */
class FMusicDriver_Null : public DriverFactoryBase {
public:
	FMusicDriver_Null() : DriverFactoryBase(Driver::DT_MUSIC, 1, "null", "Null Music Driver") {}
	Driver *CreateInstance() const override { return new MusicDriver_Null(); }
};

#endif /* MUSIC_NULL_H */
