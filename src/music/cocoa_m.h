/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cocoa_m.h Base of music playback via CoreAudio. */

#ifndef MUSIC_MACOSX_COCOA_H
#define MUSIC_MACOSX_COCOA_H

#include "music_driver.hpp"

class MusicDriver_Cocoa : public MusicDriver {
public:
	std::optional<std::string_view> Start(const StringList &param) override;

	void Stop() override;

	void PlaySong(const MusicSongInfo &song) override;

	void StopSong() override;

	bool IsSongPlaying() override;

	void SetVolume(uint8_t vol) override;
	std::string_view GetName() const override { return "cocoa"; }
};

class FMusicDriver_Cocoa : public DriverFactoryBase {
public:
	FMusicDriver_Cocoa() : DriverFactoryBase(Driver::Type::Music, 10, "cocoa", "Cocoa MIDI Driver") {}
	std::unique_ptr<Driver> CreateInstance() const override { return std::make_unique<MusicDriver_Cocoa>(); }
};

#endif /* MUSIC_MACOSX_COCOA_H */
