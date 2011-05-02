/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file libtimidity.h Base for LibTimidity music playback. */

#ifndef MUSIC_LIBTIMIDITY_H
#define MUSIC_LIBTIMIDITY_H

#include "music_driver.hpp"

/** Music driver making use of libtimidity. */
class MusicDriver_LibTimidity: public MusicDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
	/* virtual */ const char *GetName() const { return "libtimidity"; }
};

/** Factory for the libtimidity driver. */
class FMusicDriver_LibTimidity: public MusicDriverFactory<FMusicDriver_LibTimidity> {
public:
	static const int priority = 5;
	/* virtual */ const char *GetName() { return "libtimidity"; }
	/* virtual */ const char *GetDescription() { return "LibTimidity MIDI Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_LibTimidity(); }
};

#endif /* MUSIC_LIBTIMIDITY_H */
