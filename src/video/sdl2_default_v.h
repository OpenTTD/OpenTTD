/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl2_default_v.h Default backend of the SDL2 video driver. */

#ifndef VIDEO_SDL2_DEFAULT_H
#define VIDEO_SDL2_DEFAULT_H

#include "sdl2_v.h"

/** The SDL video driver using default SDL backend. */
class VideoDriver_SDL_Default : public VideoDriver_SDL_Base {
public:
	const char *GetName() const override { return "sdl"; }

protected:
	bool AllocateBackingStore(int w, int h, bool force = false) override;
	void *GetVideoPointer() override;
	void Paint() override;

	void ReleaseVideoPointer() override {}

private:
	void UpdatePalette();
	void MakePalette();
};

/** Factory for the SDL video driver. */
class FVideoDriver_SDL_Default : public DriverFactoryBase {
public:
	FVideoDriver_SDL_Default() : DriverFactoryBase(Driver::DT_VIDEO, 5, "sdl", "SDL Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_SDL_Default(); }
};

#endif /* VIDEO_SDL2_DEFAULT_H */
