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
class MusicDriver_LibTimidity : public MusicDriver {
public:
	const char *Start(const char * const *param) override;

	void Stop() override;

	void PlaySong(const MusicSongInfo &song) override;

	void StopSong() override;

	bool IsSongPlaying() override;

	void SetVolume(byte vol) override;
	const char *GetName() const override { return "libtimidity"; }
};

/** Factory for the libtimidity driver. */
class FMusicDriver_LibTimidity : public DriverFactoryBase {
public:
	FMusicDriver_LibTimidity() : DriverFactoryBase(Driver::DT_MUSIC, 5, "libtimidity", "LibTimidity MIDI Driver") {}
	Driver *CreateInstance() const override { return new MusicDriver_LibTimidity(); }
};

#endif /* MUSIC_LIBTIMIDITY_H */
