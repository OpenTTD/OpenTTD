/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file null_s.h Base for the sound of silence. */

#ifndef SOUND_NULL_H
#define SOUND_NULL_H

#include "sound_driver.hpp"

/** Implementation of the null sound driver. */
class SoundDriver_Null : public SoundDriver {
public:
	const char *Start(const StringList &) override { return nullptr; }

	void Stop() override { }
	const char *GetName() const override { return "null"; }
	bool HasOutput() const override { return false; }
};

/** Factory for the null sound driver. */
class FSoundDriver_Null : public DriverFactoryBase {
public:
	FSoundDriver_Null() : DriverFactoryBase(Driver::DT_SOUND, 1, "null", "Null Sound Driver") {}
	Driver *CreateInstance() const override { return new SoundDriver_Null(); }
};

#endif /* SOUND_NULL_H */
