/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl2_v.h Base of the SDL2 video driver. */

#ifndef VIDEO_SDL_H
#define VIDEO_SDL_H

#include "video_driver.hpp"

/** The SDL video driver. */
class VideoDriver_SDL : public VideoDriver {
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

protected:
	Dimension GetScreenSize() const override;
	void InputLoop() override;
	bool LockVideoBuffer() override;
	void UnlockVideoBuffer() override;
	void Paint() override;
	void PaintThread() override;
	void CheckPaletteAnim();

private:
	int PollEvent();
	void LoopOnce();
	void MainLoopCleanup();
	bool CreateMainSurface(uint w, uint h, bool resize);
	bool CreateMainWindow(uint w, uint h);

#ifdef __EMSCRIPTEN__
	/* Convert a constant pointer back to a non-constant pointer to a member function. */
	static void EmscriptenLoop(void *self) { ((VideoDriver_SDL *)self)->LoopOnce(); }
#endif

	/**
	 * This is true to indicate that keyboard input is in text input mode, and SDL_TEXTINPUT events are enabled.
	 */
	bool edit_box_focused;

	int startup_display;
	std::thread draw_thread;
	std::unique_lock<std::recursive_mutex> draw_lock;

	static void PaintThreadThunk(VideoDriver_SDL *drv);
};

/** Factory for the SDL video driver. */
class FVideoDriver_SDL : public DriverFactoryBase {
public:
	FVideoDriver_SDL() : DriverFactoryBase(Driver::DT_VIDEO, 5, "sdl", "SDL Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_SDL(); }
};

#endif /* VIDEO_SDL_H */
