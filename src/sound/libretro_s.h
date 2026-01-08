/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_s.h Libretro sound driver header. */

#ifndef SOUND_LIBRETRO_S_H
#define SOUND_LIBRETRO_S_H

#include "sound_driver.hpp"

/** The libretro sound driver for running OpenTTD as a libretro core. */
class SoundDriver_Libretro : public SoundDriver {
public:
	std::optional<std::string_view> Start(const StringList &param) override;
	void Stop() override;
	std::string_view GetName() const override { return "libretro"; }
};

/** Factory for the libretro sound driver. */
class FSoundDriver_Libretro : public DriverFactoryBase {
public:
	FSoundDriver_Libretro() : DriverFactoryBase(Driver::DT_SOUND, 0, "libretro", "Libretro Sound Driver") {}

	std::unique_ptr<Driver> CreateInstance() const override
	{
		return std::make_unique<SoundDriver_Libretro>();
	}
};

#endif /* SOUND_LIBRETRO_S_H */
