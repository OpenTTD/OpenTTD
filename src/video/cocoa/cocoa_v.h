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
	bool fullscreen_on_mainloop; ///< Switch to fullscreen once the main loop is running?
	Dimension orig_res;          ///< Saved window size for non-fullscreen mode.

public:
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

private:
	friend class WindowQuartzSubdriver;

	void GameSizeChanged();
};

class FVideoDriver_Cocoa : public DriverFactoryBase {
public:
	FVideoDriver_Cocoa() : DriverFactoryBase(Driver::DT_VIDEO, 10, "cocoa", "Cocoa Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_Cocoa(); }
};


/**
 * Generic display driver for cocoa
 * On grounds to not duplicate some code, it contains a few variables
 * which are not used by all device drivers.
 */
class CocoaSubdriver {
public:
	int device_width;     ///< Width of device in pixel
	int device_height;    ///< Height of device in pixel
	int device_depth;     ///< Colour depth of device in bit

	int window_width;     ///< Current window width in pixel
	int window_height;    ///< Current window height in pixel
	int window_pitch;

	int buffer_depth;     ///< Colour depth of used frame buffer
	void *pixel_buffer;   ///< used for direct pixel access
	void *window_buffer;  ///< Colour translation from palette to screen
	CGColorSpaceRef color_space; //< Window color space
	id window;            ///< Pointer to window object

#	define MAX_DIRTY_RECTS 100
	Rect dirty_rects[MAX_DIRTY_RECTS]; ///< dirty rectangles
	int num_dirty_rects;  ///< Number of dirty rectangles
	uint32 palette[256];  ///< Colour Palette

	bool active;          ///< Whether the window is visible
	bool setup;

	id cocoaview;         ///< Pointer to view object

	/* Separate driver vars for Quarz
	 * Needed here in order to avoid much code duplication */
	CGContextRef cgcontext;    ///< Context reference for Quartz subdriver

	/* Driver methods */
	/** Initialize driver */
	virtual ~CocoaSubdriver() {}

	/** Draw window
	 * @param force_update Whether to redraw unconditionally
	 */
	virtual void Draw(bool force_update = false) = 0;

	/** Mark dirty a screen region
	 * @param left x-coordinate of left border
	 * @param top  y-coordinate of top border
	 * @param width width or dirty rectangle
	 * @param height height of dirty rectangle
	 */
	virtual void MakeDirty(int left, int top, int width, int height) = 0;

	/** Update the palette */
	virtual void UpdatePalette(uint first_color, uint num_colors) = 0;

	virtual uint ListModes(OTTD_Point *modes, uint max_modes) = 0;

	/** Change window resolution
	 * @param w New window width
	 * @param h New window height
	 * @return Whether change was successful
	 */
	virtual bool ChangeResolution(int w, int h, int bpp) = 0;

	/** Are we in fullscreen mode
	 * @return whether fullscreen mode is currently used
	 */
	virtual bool IsFullscreen() = 0;

	/** Toggle between fullscreen and windowed mode
	 * @return whether switch was successful
	 */
	virtual bool ToggleFullscreen(bool fullscreen) { return false; };

	/** Return the width of the current view
	 * @return width of the current view
	 */
	virtual int GetWidth() = 0;

	/** Return the height of the current view
	 * @return height of the current view
	 */
	virtual int GetHeight() = 0;

	/** Return the current pixel buffer
	 * @return pixelbuffer
	 */
	virtual void *GetPixelBuffer() = 0;

	/** Convert local coordinate to window server (CoreGraphics) coordinate
	 * @param p local coordinates
	 * @return window driver coordinates
	 */
	virtual CGPoint PrivateLocalToCG(NSPoint *p) = 0;

	/** Return the mouse location
	 * @param event UI event
	 * @return mouse location as NSPoint
	 */
	virtual NSPoint GetMouseLocation(NSEvent *event) = 0;

	/** Return whether the mouse is within our view
	 * @param pt Mouse coordinates
	 * @return Whether mouse coordinates are within view
	 */
	virtual bool MouseIsInsideView(NSPoint *pt) = 0;

	/** Return whether the window is active (visible)
	 * @return whether the window is visible or not
	 */
	virtual bool IsActive() = 0;

	/** Whether the window was successfully resized
	 * @return whether the window was successfully resized
	 */
	virtual bool WindowResized() { return false; };
};

extern CocoaSubdriver *_cocoa_subdriver;

CocoaSubdriver *QZ_CreateWindowQuartzSubdriver(int width, int height, int bpp);

uint QZ_ListModes(OTTD_Point *modes, uint max_modes, CGDirectDisplayID display_id, int display_depth);

#endif /* VIDEO_COCOA_H */
