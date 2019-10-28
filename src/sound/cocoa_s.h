/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cocoa_s.h Base for Cocoa sound handling. */

#ifndef SOUND_COCOA_H
#define SOUND_COCOA_H

#include "sound_driver.hpp"

class SoundDriver_Cocoa : public SoundDriver {
public:
	const char *Start(const char * const *param) override;

	void Stop() override;
	const char *GetName() const override { return "cocoa"; }
};

class FSoundDriver_Cocoa : public DriverFactoryBase {
public:
	FSoundDriver_Cocoa() : DriverFactoryBase(Driver::DT_SOUND, 10, "cocoa", "Cocoa Sound Driver") {}
	Driver *CreateInstance() const override { return new SoundDriver_Cocoa(); }
};

#endif /* SOUND_COCOA_H */
