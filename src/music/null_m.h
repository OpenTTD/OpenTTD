/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file null_m.h Base for the silent music playback. */

#ifndef MUSIC_NULL_H
#define MUSIC_NULL_H

#include "music_driver.hpp"

/** The music player that does nothing. */
class MusicDriver_Null : public MusicDriver {
public:
	std::optional<std::string_view> Start(const StringList &) override { return std::nullopt; }

	void Stop() override { }

	void PlaySong(const MusicSongInfo &) override { }

	void StopSong() override { }

	bool IsSongPlaying() override { return true; }

	void SetVolume(uint8_t) override { }
	std::string_view GetName() const override { return "null"; }
};

/** Factory for the null music player. */
class FMusicDriver_Null : public DriverFactoryBase {
public:
	FMusicDriver_Null() : DriverFactoryBase(Driver::Type::Music, 1, "null", "Null Music Driver") {}
	std::unique_ptr<Driver> CreateInstance() const override { return std::make_unique<MusicDriver_Null>(); }
};

#endif /* MUSIC_NULL_H */
