/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl2_v.h Base of the SDL2 video driver. */

#ifndef VIDEO_SDL_H
#define VIDEO_SDL_H

#include <condition_variable>

#include "video_driver.hpp"

/** The SDL video driver. */
class VideoDriver_SDL_Base : public VideoDriver {
public:
	VideoDriver_SDL_Base(bool uses_hardware_acceleration = false) : VideoDriver(uses_hardware_acceleration), sdl_window(nullptr), buffer_locked(false) {}

	std::optional<std::string_view> Start(const StringList &param) override;

	void Stop() override;

	void MakeDirty(int left, int top, int width, int height) override;

	void MainLoop() override;

	bool ChangeResolution(int w, int h) override;

	bool ToggleFullscreen(bool fullscreen) override;

	bool AfterBlitterChange() override;

	bool ClaimMousePointer() override;

	void EditBoxGainedFocus() override;

	void EditBoxLostFocus() override;

	std::vector<int> GetListOfMonitorRefreshRates() override;

	std::string_view GetInfoString() const override { return this->driver_info; }

protected:
	struct SDL_Window *sdl_window; ///< Main SDL window.
	Palette local_palette; ///< Current palette to use for drawing.
	bool buffer_locked; ///< Video buffer was locked by the main thread.
	Rect dirty_rect; ///< Rectangle encompassing the dirty area of the video buffer.
	std::string driver_info; ///< Information string about selected driver.

	Dimension GetScreenSize() const override;
	void InputLoop() override;
	bool LockVideoBuffer() override;
	void UnlockVideoBuffer() override;
	void CheckPaletteAnim() override;
	bool PollEvent() override;

	/** Indicate to the driver the client-side might have changed. */
	void ClientSizeChanged(int w, int h, bool force);

	/** (Re-)create the backing store. */
	virtual bool AllocateBackingStore(int w, int h, bool force = false) = 0;
	/** Get a pointer to the video buffer. */
	virtual void *GetVideoPointer() = 0;
	/** Hand video buffer back to the painting backend. */
	virtual void ReleaseVideoPointer() = 0;
	/** Create the main window. */
	virtual bool CreateMainWindow(uint w, uint h, uint flags = 0);

private:
	void LoopOnce();
	void MainLoopCleanup();
	bool CreateMainSurface(uint w, uint h, bool resize);
	std::optional<std::string_view> Initialize();

#ifdef __EMSCRIPTEN__
	/* Convert a constant pointer back to a non-constant pointer to a member function. */
	static void EmscriptenLoop(void *self) { ((VideoDriver_SDL_Base *)self)->LoopOnce(); }
#endif

	/**
	 * This is true to indicate that keyboard input is in text input mode, and SDL_TEXTINPUT events are enabled.
	 */
	bool edit_box_focused;

	int startup_display;
};

#endif /* VIDEO_SDL_H */
