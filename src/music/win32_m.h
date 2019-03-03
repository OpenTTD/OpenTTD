/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32_m.h Base for Windows music playback. */

#ifndef MUSIC_WIN32_H
#define MUSIC_WIN32_H

#include "music_driver.hpp"

/** The Windows music player. */
class MusicDriver_Win32 : public MusicDriver {
public:
	const char *Start(const char * const *param) override;

	void Stop() override;

	void PlaySong(const MusicSongInfo &song) override;

	void StopSong() override;

	bool IsSongPlaying() override;

	void SetVolume(byte vol) override;
	const char *GetName() const override { return "win32"; }
};

/** Factory for Windows' music player. */
class FMusicDriver_Win32 : public DriverFactoryBase {
public:
	FMusicDriver_Win32() : DriverFactoryBase(Driver::DT_MUSIC, 5, "win32", "Win32 Music Driver") {}
	Driver *CreateInstance() const override { return new MusicDriver_Win32(); }
};

#endif /* MUSIC_WIN32_H */
