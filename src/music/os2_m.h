/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file os2_m.h Base for OS2 music playback. */

#ifndef MUSIC_OS2_H
#define MUSIC_OS2_H

#include "music_driver.hpp"

/** OS/2's music player. */
class MusicDriver_OS2 : public MusicDriver {
public:
	const char *Start(const char * const *param) override;

	void Stop() override;

	void PlaySong(const MusicSongInfo &song) override;

	void StopSong() override;

	bool IsSongPlaying() override;

	void SetVolume(byte vol) override;
	const char *GetName() const override { return "os2"; }
};

/** Factory for OS/2's music player. */
class FMusicDriver_OS2 : public DriverFactoryBase {
public:
	FMusicDriver_OS2() : DriverFactoryBase(Driver::DT_MUSIC, 10, "os2", "OS/2 Music Driver") {}
	Driver *CreateInstance() const override { return new MusicDriver_OS2(); }
};

#endif /* MUSIC_OS2_H */
