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
	Dimension orig_res;   ///< Saved window size for non-fullscreen mode.

	int window_width;     ///< Current window width in pixel
	int window_height;    ///< Current window height in pixel
	int window_pitch;

	int buffer_depth;     ///< Colour depth of used frame buffer
	void *pixel_buffer;   ///< used for direct pixel access
	void *window_buffer;  ///< Colour translation from palette to screen

	static const int MAX_DIRTY_RECTS = 100;

	Rect dirty_rects[MAX_DIRTY_RECTS]; ///< dirty rectangles
	uint num_dirty_rects;  ///< Number of dirty rectangles
	uint32 palette[256];  ///< Colour Palette

public:
	bool active;          ///< Whether the window is visible
	bool setup;

	OTTD_CocoaWindow *window;    ///< Pointer to window object
	OTTD_CocoaView *cocoaview;   ///< Pointer to view object
	CGColorSpaceRef color_space; ///< Window color space
	CGContextRef cgcontext;      ///< Context reference for Quartz subdriver

	OTTD_CocoaWindowDelegate *delegate; //!< Window delegate object

public:
	VideoDriver_Cocoa();

	const char *Start(const StringList &param) override;
	void Stop() override;
	void MainLoop() override;

	void MakeDirty(int left, int top, int width, int height) override;
	bool AfterBlitterChange() override;

	bool ChangeResolution(int w, int h) override;
	bool ToggleFullscreen(bool fullscreen) override;

	void EditBoxLostFocus() override;

	/** Return driver name */
	const char *GetName() const override { return "cocoa"; }

	/* --- The following methods should be private, but can't be due to Obj-C limitations. --- */

	/** Main game loop. */
	void GameLoop(); // In event.mm.

	void AllocateBackingStore();

protected:
	Dimension GetScreenSize() const override;

private:
	NSPoint GetMouseLocation(NSEvent *event);
	bool MouseIsInsideView(NSPoint *pt);
	CGPoint PrivateLocalToCG(NSPoint *p);
	bool PollEvent(); // In event.mm.
	void MouseMovedEvent(int x, int y); // In event.mm.

	bool IsFullscreen();
	void GameSizeChanged();

	void UpdateVideoModes();

	bool MakeWindow(int width, int height);

	void UpdatePalette(uint first_color, uint num_colors);
	void CheckPaletteAnim();

	void Draw(bool force_update = false);
	void BlitIndexedToView32(int left, int top, int right, int bottom);
};

class FVideoDriver_Cocoa : public DriverFactoryBase {
public:
	FVideoDriver_Cocoa() : DriverFactoryBase(Driver::DT_VIDEO, 10, "cocoa", "Cocoa Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_Cocoa(); }
};

#endif /* VIDEO_COCOA_H */
