/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file extmidi.h Base support for playing music via an external application. */

#ifndef MUSIC_EXTERNAL_H
#define MUSIC_EXTERNAL_H

#include "music_driver.hpp"

class MusicDriver_ExtMidi : public MusicDriver {
private:
	char **params;
	char song[MAX_PATH];
	pid_t pid;

	void DoPlay();
	void DoStop();

public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void PlaySong(const char *filename);

	/* virtual */ void StopSong();

	/* virtual */ bool IsSongPlaying();

	/* virtual */ void SetVolume(byte vol);
	/* virtual */ const char *GetName() const { return "extmidi"; }
};

class FMusicDriver_ExtMidi : public DriverFactoryBase {
public:
	FMusicDriver_ExtMidi() : DriverFactoryBase(Driver::DT_MUSIC, 3, "extmidi", "External MIDI Driver") {}
	/* virtual */ Driver *CreateInstance() const { return new MusicDriver_ExtMidi(); }
};

#endif /* MUSIC_EXTERNAL_H */
