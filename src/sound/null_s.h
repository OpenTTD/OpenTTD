/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file null_s.h Base for the sound of silence. */

#ifndef SOUND_NULL_H
#define SOUND_NULL_H

#include "sound_driver.hpp"

/** Implementation of the null sound driver. */
class SoundDriver_Null : public SoundDriver {
public:
	std::optional<std::string_view> Start(const StringList &) override { return std::nullopt; }

	void Stop() override { }
	std::string_view GetName() const override { return "null"; }
	bool HasOutput() const override { return false; }
};

/** Factory for the null sound driver. */
class FSoundDriver_Null : public DriverFactoryBase {
public:
	FSoundDriver_Null() : DriverFactoryBase(Driver::Type::Sound, 1, "null", "Null Sound Driver") {}
	std::unique_ptr<Driver> CreateInstance() const override { return std::make_unique<SoundDriver_Null>(); }
};

#endif /* SOUND_NULL_H */
