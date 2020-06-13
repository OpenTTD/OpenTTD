/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl2_dirty_v.h Base of the SDL2 video driver. */

#ifndef VIDEO_SDL_H
#define VIDEO_SDL_H

#include "video_driver.hpp"

/** The SDL video driver. */
class VideoDriver_SDL_Dirty : public VideoDriver {
public:
	const char *Start(const StringList &param) override;

	void Stop() override;

	void MakeDirty(int left, int top, int width, int height) override;

	void MainLoop() override;

	bool ChangeResolution(int w, int h) override;

	bool ToggleFullscreen(bool fullscreen) override;

	bool AfterBlitterChange() override;

	void AcquireBlitterLock() override;

	void ReleaseBlitterLock() override;

	bool ClaimMousePointer() override;

	void EditBoxGainedFocus() override;

	void EditBoxLostFocus() override;

	const char *GetName() const override { return "sdl"; }
private:
	int PollEvent();
	bool CreateMainSurface(uint w, uint h, bool resize);

	/**
	 * This is true to indicate that keyboard input is in text input mode, and SDL_TEXTINPUT events are enabled.
	 */
	bool edit_box_focused;
};

/** Factory for the SDL video driver. */
class FVideoDriver_SDL_Dirty : public DriverFactoryBase {
public:
	FVideoDriver_SDL_Dirty() : DriverFactoryBase(Driver::DT_VIDEO, 0, "sdl_dirty", "SDL Video Driver with dirty visuals") {}
	Driver *CreateInstance() const override { return new VideoDriver_SDL_Dirty(); }
};

#endif /* VIDEO_SDL_H */
