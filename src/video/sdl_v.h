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

#ifdef __EMSCRIPTEN__
void em_loop(void *arg);
#endif

/** The SDL video driver. */
class VideoDriver_SDL : public VideoDriver {
#ifdef __EMSCRIPTEN__
	friend void em_loop(void *arg);
#endif

public:
	const char *Start(const char * const *param) override;

	void Stop() override;

	void MakeDirty(int left, int top, int width, int height) override;

	void MainLoop() override;

	bool ChangeResolution(int w, int h) override;

	void LoopOnce();

	bool ToggleFullscreen(bool fullscreen) override;

	bool AfterBlitterChange() override;

	void AcquireBlitterLock() override;

	void ReleaseBlitterLock() override;

	bool ClaimMousePointer() override;

	const char *GetName() const override { return "sdl"; }
private:
	int PollEvent();
	bool CreateMainSurface(uint w, uint h);
	void SetupKeyboard();

	uint32 cur_ticks;
	uint32 last_cur_ticks;
	uint32 next_tick;
	std::thread draw_thread;
	std::unique_lock<std::recursive_mutex> draw_lock;
};

/** Factory for the SDL video driver. */
class FVideoDriver_SDL : public DriverFactoryBase {
public:
	FVideoDriver_SDL() : DriverFactoryBase(Driver::DT_VIDEO, 5, "sdl", "SDL Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_SDL(); }
};

#endif /* VIDEO_SDL_H */
