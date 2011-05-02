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
class SoundDriver_SDL: public SoundDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();
	/* virtual */ const char *GetName() const { return "sdl"; }
};

/** Factory for the SDL sound driver. */
class FSoundDriver_SDL: public SoundDriverFactory<FSoundDriver_SDL> {
public:
	static const int priority = 5;
	/* virtual */ const char *GetName() { return "sdl"; }
	/* virtual */ const char *GetDescription() { return "SDL Sound Driver"; }
	/* virtual */ Driver *CreateInstance() { return new SoundDriver_SDL(); }
};

#endif /* SOUND_SDL_H */
