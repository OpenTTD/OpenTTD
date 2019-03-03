/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl_s.h Base fo playing sound via SDL. */

#ifndef SOUND_SDL_H
#define SOUND_SDL_H

#include "sound_driver.hpp"

/** Implementation of the SDL sound driver. */
class SoundDriver_SDL : public SoundDriver {
public:
	const char *Start(const char * const *param) override;

	void Stop() override;
	const char *GetName() const override { return "sdl"; }
};

/** Factory for the SDL sound driver. */
class FSoundDriver_SDL : public DriverFactoryBase {
public:
	FSoundDriver_SDL() : DriverFactoryBase(Driver::DT_SOUND, 5, "sdl", "SDL Sound Driver") {}
	Driver *CreateInstance() const override { return new SoundDriver_SDL(); }
};

#endif /* SOUND_SDL_H */
