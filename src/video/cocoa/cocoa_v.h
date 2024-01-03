/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cocoa_v.h The Cocoa video driver. */

#ifndef VIDEO_COCOA_H
#define VIDEO_COCOA_H

#include "../video_driver.hpp"
#include "../../core/geometry_type.hpp"


extern bool _cocoa_video_started;

@class OTTD_CocoaWindowDelegate;
@class OTTD_CocoaWindow;
@class OTTD_CocoaView;

class VideoDriver_Cocoa : public VideoDriver {
private:
	Dimension orig_res;       ///< Saved window size for non-fullscreen mode.
	bool refresh_sys_sprites; ///< System sprites need refreshing.

public:
	bool setup; ///< Window is currently being created.

	OTTD_CocoaWindow *window;    ///< Pointer to window object
	OTTD_CocoaView *cocoaview;   ///< Pointer to view object
	CGColorSpaceRef color_space; ///< Window color space

	OTTD_CocoaWindowDelegate *delegate; //!< Window delegate object

public:
	VideoDriver_Cocoa();

	void Stop() override;
	void MainLoop() override;

	void MakeDirty(int left, int top, int width, int height) override;
	bool AfterBlitterChange() override;

	bool ChangeResolution(int w, int h) override;
	bool ToggleFullscreen(bool fullscreen) override;

	void ClearSystemSprites() override;
	void PopulateSystemSprites() override;

	void EditBoxLostFocus() override;

	std::vector<int> GetListOfMonitorRefreshRates() override;

	/* --- The following methods should be private, but can't be due to Obj-C limitations. --- */

	void MainLoopReal();

	virtual void AllocateBackingStore(bool force = false) = 0;

protected:
	Rect dirty_rect;    ///< Region of the screen that needs redrawing.
	bool buffer_locked; ///< Video buffer was locked by the main thread.

	Dimension GetScreenSize() const override;
	float GetDPIScale() override;
	void InputLoop() override;
	bool LockVideoBuffer() override;
	void UnlockVideoBuffer() override;
	bool PollEvent() override;

	void GameSizeChanged();

	const char *Initialize();

	void UpdateVideoModes();

	bool MakeWindow(int width, int height);

	virtual NSView *AllocateDrawView() = 0;

	/** Get a pointer to the video buffer. */
	virtual void *GetVideoPointer() = 0;
	/** Hand video buffer back to the drawing backend. */
	virtual void ReleaseVideoPointer() {}

private:
	bool IsFullscreen();
};

class VideoDriver_CocoaQuartz : public VideoDriver_Cocoa {
private:
	int buffer_depth;     ///< Colour depth of used frame buffer
	void *pixel_buffer;   ///< used for direct pixel access
	void *window_buffer;  ///< Colour translation from palette to screen

	int window_width;     ///< Current window width in pixel
	int window_height;    ///< Current window height in pixel
	int window_pitch;

	uint32_t palette[256];  ///< Colour Palette

	void BlitIndexedToView32(int left, int top, int right, int bottom);
	void UpdatePalette(uint first_color, uint num_colors);

public:
	CGContextRef cgcontext;      ///< Context reference for Quartz subdriver

	VideoDriver_CocoaQuartz();

	const char *Start(const StringList &param) override;
	void Stop() override;

	/** Return driver name */
	const char *GetName() const override { return "cocoa"; }

	void AllocateBackingStore(bool force = false) override;

protected:
	void Paint() override;
	void CheckPaletteAnim() override;

	NSView *AllocateDrawView() override;

	void *GetVideoPointer() override { return this->buffer_depth == 8 ? this->pixel_buffer : this->window_buffer; }
};

class FVideoDriver_CocoaQuartz : public DriverFactoryBase {
public:
	FVideoDriver_CocoaQuartz() : DriverFactoryBase(Driver::DT_VIDEO, 8, "cocoa", "Cocoa Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_CocoaQuartz(); }
};

#endif /* VIDEO_COCOA_H */
