/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32_v.h Base of the Windows video driver. */

#ifndef VIDEO_WIN32_H
#define VIDEO_WIN32_H

#include "video_driver.hpp"

/** Base class for Windows video drivers. */
class VideoDriver_Win32Base : public VideoDriver {
public:
	void MakeDirty(int left, int top, int width, int height) override;

	void MainLoop() override;

	bool ChangeResolution(int w, int h) override;

	bool ToggleFullscreen(bool fullscreen) override;

	void AcquireBlitterLock() override;

	void ReleaseBlitterLock() override;

	bool ClaimMousePointer() override;

	void EditBoxLostFocus() override;

protected:
	Dimension GetScreenSize() const override;
	float GetDPIScale() override;
	void InputLoop() override;
	bool LockVideoBuffer() override;
	void UnlockVideoBuffer() override;
	void Paint() override;
	void PaintThread() override;
	void CheckPaletteAnim() override;

	bool MakeWindow(bool full_screen);

private:
	std::unique_lock<std::recursive_mutex> draw_lock;

	static void PaintThreadThunk(VideoDriver_Win32Base *drv);

	friend LRESULT CALLBACK WndProcGdi(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
/** The GDI video driver for windows. */
class VideoDriver_Win32GDI : public VideoDriver_Win32Base {
public:
	const char *Start(const StringList &param) override;

	void Stop() override;

	bool AfterBlitterChange() override;

	const char *GetName() const override { return "win32"; }
};

/** The factory for Windows' video driver. */
class FVideoDriver_Win32GDI : public DriverFactoryBase {
public:
	FVideoDriver_Win32GDI() : DriverFactoryBase(Driver::DT_VIDEO, 10, "win32", "Win32 GDI Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_Win32GDI(); }
};

#endif /* VIDEO_WIN32_H */
