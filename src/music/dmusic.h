/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dmusic.h Base of playing music via DirectMusic. */

#ifndef MUSIC_DMUSIC_H
#define MUSIC_DMUSIC_H

#include "music_driver.hpp"

/** Music player making use of DirectX. */
class MusicDriver_DMusic : public MusicDriver {
public:
	virtual ~MusicDriver_DMusic();

	const char *Start(const StringList &param) override;

	void Stop() override;

	void PlaySong(const MusicSongInfo &song) override;

	void StopSong() override;

	bool IsSongPlaying() override;

	void SetVolume(uint8_t vol) override;
	const char *GetName() const override { return "dmusic"; }
};

/** Factory for the DirectX music player. */
class FMusicDriver_DMusic : public DriverFactoryBase {
public:
	FMusicDriver_DMusic() : DriverFactoryBase(Driver::DT_MUSIC, 10, "dmusic", "DirectMusic MIDI Driver") {}
	Driver *CreateInstance() const override { return new MusicDriver_DMusic(); }
};

#endif /* MUSIC_DMUSIC_H */
