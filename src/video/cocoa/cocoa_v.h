/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cocoa_v.h The Cocoa video driver. */

#ifndef VIDEO_COCOA_H
#define VIDEO_COCOA_H

#include <AvailabilityMacros.h>

#include "../video_driver.hpp"

class VideoDriver_Cocoa: public VideoDriver {
public:
	/* virtual */ const char *Start(const char * const *param);

	/* virtual */ void Stop();

	/* virtual */ void MakeDirty(int left, int top, int width, int height);

	/* virtual */ void MainLoop();

	/* virtual */ bool ChangeResolution(int w, int h);

	/* virtual */ bool ToggleFullscreen(bool fullscreen);

	/* virtual */ const char *GetName() const { return "cocoa"; }
};

class FVideoDriver_Cocoa: public VideoDriverFactory<FVideoDriver_Cocoa> {
public:
	static const int priority = 10;
	/* virtual */ const char *GetName() { return "cocoa"; }
	/* virtual */ const char *GetDescription() { return "Cocoa Video Driver"; }
	/* virtual */ Driver *CreateInstance() { return new VideoDriver_Cocoa(); }
};


/**
 * Generic display driver for cocoa
 * On grounds to not duplicate some code, it contains a few variables
 * which are not used by all device drivers.
 */
class CocoaSubdriver {
public:
	int device_width;
	int device_height;
	int device_depth;

	int window_width;
	int window_height;
	int window_pitch;

	int buffer_depth;
	void *pixel_buffer;   // used for direct pixel access
	void *window_buffer;  // has colour translation from palette to screen
	id window;            // pointer to window object

#	define MAX_DIRTY_RECTS 100
	Rect dirty_rects[MAX_DIRTY_RECTS];
	int num_dirty_rects;
	uint32 palette[256];

	bool active;
	bool setup;

	id cocoaview;         // pointer to view object

	/* Separate driver vars for Quarz
	 * Needed here in order to avoid much code duplication */
	CGContextRef cgcontext;

	/* Driver methods */
	virtual ~CocoaSubdriver() {}

	virtual void Draw(bool force_update = false) = 0;
	virtual void MakeDirty(int left, int top, int width, int height) = 0;
	virtual void UpdatePalette(uint first_color, uint num_colors) = 0;

	virtual uint ListModes(OTTD_Point *modes, uint max_modes) = 0;

	virtual bool ChangeResolution(int w, int h) = 0;

	virtual bool IsFullscreen() = 0;
	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
	virtual void *GetPixelBuffer() = 0;

	/* Convert local coordinate to window server (CoreGraphics) coordinate */
	virtual CGPoint PrivateLocalToCG(NSPoint *p) = 0;

	virtual NSPoint GetMouseLocation(NSEvent *event) = 0;
	virtual bool MouseIsInsideView(NSPoint *pt) = 0;

	virtual bool IsActive() = 0;

	virtual void SetPortAlphaOpaque() { return; };
	virtual bool WindowResized() { return false; };
};

extern CocoaSubdriver *_cocoa_subdriver;

CocoaSubdriver *QZ_CreateFullscreenSubdriver(int width, int height, int bpp);

#ifdef ENABLE_COCOA_QUICKDRAW
CocoaSubdriver *QZ_CreateWindowQuickdrawSubdriver(int width, int height, int bpp);
#endif

#ifdef ENABLE_COCOA_QUARTZ
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
CocoaSubdriver *QZ_CreateWindowQuartzSubdriver(int width, int height, int bpp);
#endif
#endif

void QZ_GameSizeChanged();

void QZ_GameLoop();

uint QZ_ListModes(OTTD_Point *modes, uint max_modes, CGDirectDisplayID display_id, int display_depth);

/** Category of NSCursor to allow cursor showing/hiding */
@interface NSCursor (OTTD_QuickdrawCursor)
+ (NSCursor *) clearCocoaCursor;
@end

/** Subclass of NSWindow to cater our special needs */
@interface OTTD_CocoaWindow : NSWindow {
	CocoaSubdriver *driver;
}

- (void)setDriver:(CocoaSubdriver*)drv;

- (void)miniaturize:(id)sender;
- (void)display;
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag;
- (void)appDidHide:(NSNotification*)note;
- (void)appWillUnhide:(NSNotification*)note;
- (void)appDidUnhide:(NSNotification*)note;
- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag;
@end

/** Subclass of NSView to fix Quartz rendering and mouse awareness */
@interface OTTD_CocoaView : NSView {
	CocoaSubdriver *driver;
	NSTrackingRectTag trackingtag;
}
- (void)setDriver:(CocoaSubdriver*)drv;
- (void)drawRect:(NSRect)rect;
- (BOOL)isOpaque;
- (BOOL)acceptsFirstResponder;
- (BOOL)becomeFirstResponder;
- (void)setTrackingRect;
- (void)clearTrackingRect;
- (void)resetCursorRects;
- (void)viewWillMoveToWindow:(NSWindow *)win;
- (void)viewDidMoveToWindow;
- (void)mouseEntered:(NSEvent *)theEvent;
- (void)mouseExited:(NSEvent *)theEvent;
@end

/** Delegate for our NSWindow to send ask for quit on close */
@interface OTTD_CocoaWindowDelegate : NSObject {
	CocoaSubdriver *driver;
}

- (void)setDriver:(CocoaSubdriver*)drv;

- (BOOL)windowShouldClose:(id)sender;
@end


#endif /* VIDEO_COCOA_H */
