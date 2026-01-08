/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_m.h Libretro music driver header. */

#ifndef MUSIC_LIBRETRO_M_H
#define MUSIC_LIBRETRO_M_H

#include "music_driver.hpp"

/** The libretro music driver for running OpenTTD as a libretro core. */
class MusicDriver_Libretro : public MusicDriver {
public:
	std::optional<std::string_view> Start(const StringList &param) override;
	void Stop() override;
	void PlaySong(const MusicSongInfo &song) override;
	void StopSong() override;
	bool IsSongPlaying() override;
	void SetVolume(uint8_t vol) override;
	std::string_view GetName() const override { return "libretro"; }

private:
	bool playing = false;
	uint8_t volume = 127;
};

/** Factory for the libretro music driver. */
class FMusicDriver_Libretro : public DriverFactoryBase {
public:
	FMusicDriver_Libretro() : DriverFactoryBase(Driver::DT_MUSIC, 0, "libretro", "Libretro Music Driver") {}

	std::unique_ptr<Driver> CreateInstance() const override
	{
		return std::make_unique<MusicDriver_Libretro>();
	}
};

#endif /* MUSIC_LIBRETRO_M_H */
