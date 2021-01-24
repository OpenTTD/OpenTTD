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

class VideoDriver_Cocoa : public VideoDriver {
private:
	Dimension orig_res;   ///< Saved window size for non-fullscreen mode.

	int device_width;     ///< Width of device in pixel
	int device_height;    ///< Height of device in pixel

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

	id window;            ///< Pointer to window object
	id cocoaview;         ///< Pointer to view object
	CGColorSpaceRef color_space; ///< Window color space
	CGContextRef cgcontext;      ///< Context reference for Quartz subdriver

public:
	VideoDriver_Cocoa();

	const char *Start(const StringList &param) override;

	/** Stop the video driver */
	void Stop() override;

	/** Mark dirty a screen region
	 * @param left x-coordinate of left border
	 * @param top  y-coordinate of top border
	 * @param width width or dirty rectangle
	 * @param height height of dirty rectangle
	 */
	void MakeDirty(int left, int top, int width, int height) override;

	/** Programme main loop */
	void MainLoop() override;

	/** Change window resolution
	 * @param w New window width
	 * @param h New window height
	 * @return Whether change was successful
	 */
	bool ChangeResolution(int w, int h) override;

	/** Set a new window mode
	 * @param fullscreen Whether to set fullscreen mode or not
	 * @return Whether changing the screen mode was successful
	 */
	bool ToggleFullscreen(bool fullscreen) override;

	/** Callback invoked after the blitter was changed.
	 * @return True if no error.
	 */
	bool AfterBlitterChange() override;

	/**
	 * An edit box lost the input focus. Abort character compositing if necessary.
	 */
	void EditBoxLostFocus() override;

	/** Return driver name
	 * @return driver name
	 */
	const char *GetName() const override { return "cocoa"; }

	/* --- The following methods should be private, but can't be due to Obj-C limitations. --- */

	/** Main game loop. */
	void GameLoop(); // In event.mm.

	/** Resize the window.
	 * @return whether the window was successfully resized
	 */
	bool WindowResized();

	/** Convert local coordinate to window server (CoreGraphics) coordinate
	 * @param p local coordinates
	 * @return window driver coordinates
	 */
	CGPoint PrivateLocalToCG(NSPoint *p);

protected:
	Dimension GetScreenSize() const override;

private:
	bool PollEvent(); // In event.mm.
	void MouseMovedEvent(int x, int y); // In event.mm.
	void WarpCursor(int x, int y); // In event.mm.

	void GameSizeChanged();
	void UpdateVideoModes();

	/**
	 * This function copies 8bpp pixels from the screen buffer in 32bpp windowed mode.
	 *
	 * @param left The x coord for the left edge of the box to blit.
	 * @param top The y coord for the top edge of the box to blit.
	 * @param right The x coord for the right edge of the box to blit.
	 * @param bottom The y coord for the bottom edge of the box to blit.
	 */
	void BlitIndexedToView32(int left, int top, int right, int bottom);

	void GetDeviceInfo();
	bool SetVideoMode(int width, int height, int bpp);

	/** Draw window
	 * @param force_update Whether to redraw unconditionally
	 */
	void Draw(bool force_update = false);

	/** Update the palette */
	void UpdatePalette(uint first_color, uint num_colors);

	/** Are we in fullscreen mode
	 * @return whether fullscreen mode is currently used
	 */
	bool IsFullscreen();

	/** Return the mouse location
	 * @param event UI event
	 * @return mouse location as NSPoint
	 */
	NSPoint GetMouseLocation(NSEvent *event);

	/** Return whether the mouse is within our view
	 * @param pt Mouse coordinates
	 * @return Whether mouse coordinates are within view
	 */
	bool MouseIsInsideView(NSPoint *pt);

	void CheckPaletteAnim();
};

class FVideoDriver_Cocoa : public DriverFactoryBase {
public:
	FVideoDriver_Cocoa() : DriverFactoryBase(Driver::DT_VIDEO, 10, "cocoa", "Cocoa Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_Cocoa(); }
};

#endif /* VIDEO_COCOA_H */
