/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl_v.h Base of the SDL video driver. */

#ifndef VIDEO_SDL_H
#define VIDEO_SDL_H

#include "video_driver.hpp"

/** The SDL video driver. */
class VideoDriver_SDL : public VideoDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);

	/* virtual */ bool AfterBlitterChange();

	/* virtual */ void AcquireBlitterLock();

	/* virtual */ void ReleaseBlitterLock();

	/* virtual */ bool ClaimMousePointer();

	/* virtual */ const char *GetName() const { return "sdl"; }
private:
	int PollEvent();
	bool CreateMainSurface(uint w, uint h);
	void SetupKeyboard();
};

/** Factory for the SDL video driver. */
class FVideoDriver_SDL : public DriverFactoryBase {
public:
	FVideoDriver_SDL() : DriverFactoryBase(Driver::DT_VIDEO, 5, "sdl", "SDL Video Driver") {}
	/* virtual */ Driver *CreateInstance() const { return new VideoDriver_SDL(); }
};

#endif /* VIDEO_SDL_H */
