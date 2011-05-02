/* $Id$ */

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

/** The midi player for BeOS. */
class MusicDriver_BeMidi: public MusicDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
	/* virtual */ const char *GetName() const { return "bemidi"; }
};

/** Factory for the BeOS midi player. */
class FMusicDriver_BeMidi: public MusicDriverFactory<FMusicDriver_BeMidi> {
public:
	static const int priority = 10;
	/* virtual */ const char *GetName() { return "bemidi"; }
	/* virtual */ const char *GetDescription() { return "BeOS MIDI Driver"; }
	/* virtual */ Driver *CreateInstance() { return new MusicDriver_BeMidi(); }
};

#endif /* MUSIC_BEMIDI_H */
