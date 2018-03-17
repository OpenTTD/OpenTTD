/* $Id$ */

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
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const MusicSongInfo &song);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
	/* virtual */ const char *GetName() const { return "os2"; }
};

/** Factory for OS/2's music player. */
class FMusicDriver_OS2 : public DriverFactoryBase {
public:
	FMusicDriver_OS2() : DriverFactoryBase(Driver::DT_MUSIC, 10, "os2", "OS/2 Music Driver") {}
	/* virtual */ Driver *CreateInstance() const { return new MusicDriver_OS2(); }
};

#endif /* MUSIC_OS2_H */
