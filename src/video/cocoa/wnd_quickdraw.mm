/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/******************************************************************************
 *                             Cocoa video driver                             *
 * Known things left to do:                                                   *
 *  List available resolutions.                                               *
 ******************************************************************************/

#ifdef WITH_COCOA
#ifdef ENABLE_COCOA_QUICKDRAW

#define MAC_OS_X_VERSION_MIN_REQUIRED MAC_OS_X_VERSION_10_3
#include "../../stdafx.h"
#include "../../os/macosx/macos.h"

#define Rect  OTTDRect
#define Point OTTDPoint
#import <Cocoa/Cocoa.h>
#undef Rect
#undef Point

#include "../../debug.h"
#include "../../rev.h"
#include "../../core/geometry_type.hpp"
#include "cocoa_v.h"
#include "../../core/math_func.hpp"
#include "../../gfx_func.h"
#include "../../functions.h"

/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */


class WindowQuickdrawSubdriver;


/* Subclass of NSWindow to fix genie effect and support resize events  */
@interface OTTD_QuickdrawWindow : NSWindow {
	WindowQuickdrawSubdriver *driver;
}

- (void)setDriver:(WindowQuickdrawSubdriver*)drv;

- (void)miniaturize:(id)sender;
- (void)display;
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag;
- (void)appDidHide:(NSNotification*)note;
- (void)appWillUnhide:(NSNotification*)note;
- (void)appDidUnhide:(NSNotification*)note;
- (id)initWithContentRect:(NSRect)contentRect styleMask:(unsigned int)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag;
@end

/* Delegate for our NSWindow to send ask for quit on close */
@interface OTTD_QuickdrawWindowDelegate : NSObject{
	WindowQuickdrawSubdriver *driver;
}

- (void)setDriver:(WindowQuickdrawSubdriver*)drv;

- (BOOL)windowShouldClose:(id)sender;
@end

class WindowQuickdrawSubdriver: public CocoaSubdriver {
	int device_width;
	int device_height;
	int device_depth;

	int window_width;
	int window_height;
	int window_pitch;

	int buffer_depth;

	void *pixel_buffer;
	void *window_buffer;

	OTTD_QuickdrawWindow *window;

	#define MAX_DIRTY_RECTS 100
	Rect dirty_rects[MAX_DIRTY_RECTS];
	int num_dirty_rects;

	uint16 palette16[256];
	uint32 palette32[256];

public:
	bool active;
	bool setup;

	NSQuickDrawView *qdview;

private:
	void GetDeviceInfo();

	bool SetVideoMode(int width, int height);

	/**
	 * This function copies 32bpp pixels from the screen buffer in 16bpp windowed mode.
	 *
	 * @param left The x coord for the left edge of the box to blit.
	 * @param top The y coord for the top edge of the box to blit.
	 * @param right The x coord for the right edge of the box to blit.
	 * @param bottom The y coord for the bottom edge of the box to blit.
	 */
	void Blit32ToView32(int left, int top, int right, int bottom);

	/**
	 * This function copies 8bpp pixels from the screen buffer in 32bpp windowed mode.
	 *
	 * @param left The x coord for the left edge of the box to blit.
	 * @param top The y coord for the top edge of the box to blit.
	 * @param right The x coord for the right edge of the box to blit.
	 * @param bottom The y coord for the bottom edge of the box to blit.
	 */
	void BlitIndexedToView32(int left, int top, int right, int bottom);

	/**
	 * This function copies 8bpp pixels from the screen buffer in 16bpp windowed mode.
	 *
	 * @param left The x coord for the left edge of the box to blit.
	 * @param top The y coord for the top edge of the box to blit.
	 * @param right The x coord for the right edge of the box to blit.
	 * @param bottom The y coord for the bottom edge of the box to blit.
	 */
	void BlitIndexedToView16(int left, int top, int right, int bottom);

	inline void BlitToView(int left, int top, int right, int bottom);
	void DrawResizeIcon();


public:
	WindowQuickdrawSubdriver(int bpp);
	virtual ~WindowQuickdrawSubdriver();

	virtual void Draw(bool force_update);
	virtual void MakeDirty(int left, int top, int width, int height);
	virtual void UpdatePalette(uint first_color, uint num_colors);

	virtual uint ListModes(OTTD_Point *modes, uint max_modes);

	virtual bool ChangeResolution(int w, int h);

	virtual bool IsFullscreen() { return false; }

	virtual int GetWidth() { return window_width; }
	virtual int GetHeight() { return window_height; }
	virtual void *GetPixelBuffer() { return pixel_buffer; }

	/* Convert local coordinate to window server (CoreGraphics) coordinate */
	virtual CGPoint PrivateLocalToCG(NSPoint *p);

	virtual NSPoint GetMouseLocation(NSEvent *event);
	virtual bool MouseIsInsideView(NSPoint *pt);

	virtual bool IsActive() { return active; }


	void SetPortAlphaOpaque();
	bool WindowResized();
};


@implementation OTTD_QuickdrawWindow

- (void)setDriver:(WindowQuickdrawSubdriver*)drv
{
	driver = drv;
}


/* we override these methods to fix the miniaturize animation/dock icon bug */
- (void)miniaturize:(id)sender
{
	/* make the alpha channel opaque so anim won't have holes in it */
	driver->SetPortAlphaOpaque ();

	/* window is hidden now */
	driver->active = false;

	QZ_ShowMouse();

	[ super miniaturize:sender ];
}

- (void)display
{
	/* This method fires just before the window deminaturizes from the Dock.
	 * We'll save the current visible surface, let the window manager redraw any
	 * UI elements, and restore the surface. This way, no expose event
	 * is required, and the deminiaturize works perfectly.
	 */

	driver->SetPortAlphaOpaque();

	/* save current visible surface */
	[ self cacheImageInRect:[ driver->qdview frame ] ];

	/* let the window manager redraw controls, border, etc */
	[ super display ];

	/* restore visible surface */
	[ self restoreCachedImage ];

	/* window is visible again */
	driver->active = true;
}

- (void)setFrame:(NSRect)frameRect display:(BOOL)flag
{
	[ super setFrame:frameRect display:flag ];

	/* Don't do anything if the window is currently being created */
	if (driver->setup) return;

	if (!driver->WindowResized()) error("Cocoa: Failed to resize window.");
}

- (void)appDidHide:(NSNotification*)note
{
	driver->active = false;
}


- (void)appWillUnhide:(NSNotification*)note
{
	driver->SetPortAlphaOpaque ();

	/* save current visible surface */
	[ self cacheImageInRect:[ driver->qdview frame ] ];
}

- (void)appDidUnhide:(NSNotification*)note
{
	/* restore cached image, since it may not be current, post expose event too */
	[ self restoreCachedImage ];

	driver->active = true;
}


- (id)initWithContentRect:(NSRect)contentRect styleMask:(unsigned int)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
	/* Make our window subclass receive these application notifications */
	[ [ NSNotificationCenter defaultCenter ] addObserver:self
		selector:@selector(appDidHide:) name:NSApplicationDidHideNotification object:NSApp ];

	[ [ NSNotificationCenter defaultCenter ] addObserver:self
		selector:@selector(appDidUnhide:) name:NSApplicationDidUnhideNotification object:NSApp ];

	[ [ NSNotificationCenter defaultCenter ] addObserver:self
		selector:@selector(appWillUnhide:) name:NSApplicationWillUnhideNotification object:NSApp ];

	return [ super initWithContentRect:contentRect styleMask:styleMask backing:backingType defer:flag ];
}

@end

@implementation OTTD_QuickdrawWindowDelegate
- (void)setDriver:(WindowQuickdrawSubdriver*)drv
{
	driver = drv;
}

- (BOOL)windowShouldClose:(id)sender
{
	HandleExitGameRequest();

	return NO;
}

- (void)windowDidBecomeKey:(NSNotification*)aNotification
{
	driver->active = true;
}

- (void)windowDidResignKey:(NSNotification*)aNotification
{
	driver->active = false;
}

- (void)windowDidBecomeMain:(NSNotification*)aNotification
{
	driver->active = true;
}

- (void)windowDidResignMain:(NSNotification*)aNotification
{
	driver->active = false;
}

@end


static const int _resize_icon_width  = 16;
static const int _resize_icon_height = 16;

static bool _resize_icon[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1,
	0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1,
	0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0,
	0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0,
	0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1,
	0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1,
	0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0,
	1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0
};


void WindowQuickdrawSubdriver::GetDeviceInfo()
{
	/* Initialize the video settings; this data persists between mode switches */
	CFDictionaryRef cur_mode = CGDisplayCurrentMode(kCGDirectMainDisplay);

	/* Gather some information that is useful to know about the display */
	CFNumberGetValue((const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayBitsPerPixel),
		kCFNumberSInt32Type, &this->device_depth);

	CFNumberGetValue((const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayWidth),
		kCFNumberSInt32Type, &this->device_width);

	CFNumberGetValue((const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayHeight),
		kCFNumberSInt32Type, &this->device_height);
}

bool WindowQuickdrawSubdriver::SetVideoMode(int width, int height)
{
	this->setup = true;
	this->GetDeviceInfo();

	if (this->buffer_depth > this->device_depth) {
		DEBUG(driver, 0, "Cannot use a blitter with a higer screen depth than the display when running in windowed mode.");
		this->setup = false;
		return false;
	}

	if (width > this->device_width) width = this->device_width;
	if (height > this->device_height) height = this->device_height;

	NSRect contentRect = NSMakeRect(0, 0, width, height);

	/* Check if we should recreate the window */
	if (this->window == nil) {
		OTTD_QuickdrawWindowDelegate *delegate;

		/* Set the window style */
		unsigned int style = NSTitledWindowMask;
		style |= (NSMiniaturizableWindowMask | NSClosableWindowMask);
		style |= NSResizableWindowMask;

		/* Manually create a window, avoids having a nib file resource */
		this->window = [ [ OTTD_QuickdrawWindow alloc ] initWithContentRect:contentRect
						styleMask:style	backing:NSBackingStoreBuffered defer:NO ];

		if (this->window == nil) {
			DEBUG(driver, 0, "Could not create the Cocoa window.");
			this->setup = false;
			return false;
		}

		[ this->window setDriver:this ];

		char caption[50];
		snprintf(caption, sizeof(caption), "OpenTTD %s", _openttd_revision);
		NSString *nsscaption = [ [ NSString alloc ] initWithUTF8String:caption ];
		[ this->window setTitle:nsscaption ];
		[ this->window setMiniwindowTitle:nsscaption ];
		[ nsscaption release ];

		[ this->window setAcceptsMouseMovedEvents:YES ];
		[ this->window setViewsNeedDisplay:NO ];

		delegate = [ [ OTTD_QuickdrawWindowDelegate alloc ] init ];
		[ delegate setDriver:this ];
		[ this->window setDelegate: [ delegate autorelease ] ];
	} else {
		/* We already have a window, just change its size */
		[ this->window setContentSize:contentRect.size ];
		/* Ensure frame height - title bar height >= view height */
		contentRect.size.height = Clamp(height, 0, [ this->window frame ].size.height - 22); // 22 is the height of title bar of window
		height = contentRect.size.height;
		[ this->qdview setFrameSize:contentRect.size ];
	}

	// Update again
	this->window_width = width;
	this->window_height = height;

	[ this->window center ];

	/* Only recreate the view if it doesn't already exist */
	if (this->qdview == nil) {
		this->qdview = [ [ NSQuickDrawView alloc ] initWithFrame:contentRect ];
		if (this->qdview == nil) {
			DEBUG(driver, 0, "Could not create the Quickdraw view.");
			this->setup = false;
			return false;
		}

		[ this->qdview setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable ];
		[ [ this->window contentView ] addSubview:this->qdview ];
		[ this->qdview release ];
		[ this->window makeKeyAndOrderFront:nil ];
	}

	bool ret = this->WindowResized();
	this->UpdatePalette(0, 256);

	this->setup = false;
	return ret;
}

void WindowQuickdrawSubdriver::Blit32ToView32(int left, int top, int right, int bottom)
{
	const uint32 *src   = (uint32*)this->pixel_buffer;
	uint32       *dst   = (uint32*)this->window_buffer;
	uint          width = this->window_width;
	uint          pitch = this->window_pitch / 4;

	dst += top * pitch + left;
	src += top * width + left;

	for (int y = top; y < bottom; y++, dst+= pitch, src+= width) {
		memcpy(dst, src, (right - left) * 4);
	}
}

void WindowQuickdrawSubdriver::BlitIndexedToView32(int left, int top, int right, int bottom)
{
	const uint32 *pal   = this->palette32;
	const uint8  *src   = (uint8*)this->pixel_buffer;
	uint32       *dst   = (uint32*)this->window_buffer;
	uint          width = this->window_width;
	uint          pitch = this->window_pitch / 4;

	for (int y = top; y < bottom; y++) {
		for (int x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}

void WindowQuickdrawSubdriver::BlitIndexedToView16(int left, int top, int right, int bottom)
{
	const uint16 *pal   = this->palette16;
	const uint8  *src   = (uint8*)this->pixel_buffer;
	uint16       *dst   = (uint16*)this->window_buffer;
	uint          width = this->window_width;
	uint          pitch = this->window_pitch / 2;

	for (int y = top; y < bottom; y++) {
		for (int x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}


inline void WindowQuickdrawSubdriver::BlitToView(int left, int top, int right, int bottom)
{
	switch (this->device_depth) {
		case 32:
			switch (this->buffer_depth) {
				case 32:
					this->Blit32ToView32(left, top, right, bottom);
					break;
				case 8:
					this->BlitIndexedToView32(left, top, right, bottom);
					break;
			}
			break;
		case 16:
			this->BlitIndexedToView16(left, top, right, bottom);
			break;
	}
}

void WindowQuickdrawSubdriver::DrawResizeIcon()
{
	int xoff = this->window_width - _resize_icon_width;
	int yoff = this->window_height - _resize_icon_height;

	switch (this->device_depth) {
		case 32:
			for (int y = 0; y < _resize_icon_height; y++) {
				uint32 *trg = (uint32*)this->window_buffer + (yoff + y) * this->window_pitch / 4 + xoff;

				for (int x = 0; x < _resize_icon_width; x++, trg++) {
					if (_resize_icon[y * _resize_icon_width + x]) *trg = 0xff000000;
				}
			}
			break;
		case 16:
			for (int y = 0; y < _resize_icon_height; y++) {
				uint16 *trg = (uint16*)this->window_buffer + (yoff + y) * this->window_pitch / 2 + xoff;

				for (int x = 0; x < _resize_icon_width; x++, trg++) {
					if (_resize_icon[y * _resize_icon_width + x]) *trg = 0x0000;
				}
			}
			break;
	}
}


WindowQuickdrawSubdriver::WindowQuickdrawSubdriver(int bpp)
{
	this->window_width  = 0;
	this->window_height = 0;
	this->buffer_depth  = bpp;
	this->pixel_buffer  = NULL;
	this->active        = false;
	this->setup         = false;

	this->window = nil;
	this->qdview = nil;

	this->num_dirty_rects = MAX_DIRTY_RECTS;
}

WindowQuickdrawSubdriver::~WindowQuickdrawSubdriver()
{
	QZ_ShowMouse();

	/* Release window mode resources */
	if (this->window != nil) [ this->window close ];

	free(this->pixel_buffer);
}

void WindowQuickdrawSubdriver::Draw(bool force_update)
{
	/* Check if we need to do anything */
	if (this->num_dirty_rects == 0 || [ this->window isMiniaturized ]) return;

	if (this->num_dirty_rects >= MAX_DIRTY_RECTS) {
		this->num_dirty_rects = 1;
		this->dirty_rects[0].left = 0;
		this->dirty_rects[0].top = 0;
		this->dirty_rects[0].right = this->window_width;
		this->dirty_rects[0].bottom = this->window_height;
	}

	RgnHandle dirty = NewRgn();
	RgnHandle temp  = NewRgn();

	SetEmptyRgn(dirty);

	/* Build the region of dirty rectangles */
	for (int i = 0; i < this->num_dirty_rects; i++) {
		this->BlitToView(this->dirty_rects[i].left, this->dirty_rects[i].top,
				this->dirty_rects[i].right, this->dirty_rects[i].bottom);

		MacSetRectRgn(temp, this->dirty_rects[i].left, this->dirty_rects[i].top,
				this->dirty_rects[i].right, this->dirty_rects[i].bottom);
		MacUnionRgn(dirty, temp, dirty);
	}

	this->DrawResizeIcon();

	/* Flush the dirty region */
	QDFlushPortBuffer( (OpaqueGrafPtr*) [ this->qdview qdPort ], dirty);
	DisposeRgn(dirty);
	DisposeRgn(temp);

	this->num_dirty_rects = 0;
}

void WindowQuickdrawSubdriver::MakeDirty(int left, int top, int width, int height)
{
	if (this->num_dirty_rects < MAX_DIRTY_RECTS) {
		this->dirty_rects[this->num_dirty_rects].left = left;
		this->dirty_rects[this->num_dirty_rects].top = top;
		this->dirty_rects[this->num_dirty_rects].right = left + width;
		this->dirty_rects[this->num_dirty_rects].bottom = top + height;
	}
	this->num_dirty_rects++;
}

void WindowQuickdrawSubdriver::UpdatePalette(uint first_color, uint num_colors)
{
	if (this->buffer_depth != 8) return;

	switch (this->device_depth) {
		case 32:
			for (uint i = first_color; i < first_color + num_colors; i++) {
				uint32 clr32 = 0xff000000;
				clr32 |= (uint32)_cur_palette[i].r << 16;
				clr32 |= (uint32)_cur_palette[i].g << 8;
				clr32 |= (uint32)_cur_palette[i].b;
				this->palette32[i] = clr32;
			}
			break;
		case 16:
			for (uint i = first_color; i < first_color + num_colors; i++) {
				uint16 clr16 = 0x0000;
				clr16 |= (uint16)((_cur_palette[i].r >> 3) & 0x1f) << 10;
				clr16 |= (uint16)((_cur_palette[i].g >> 3) & 0x1f) << 5;
				clr16 |= (uint16)((_cur_palette[i].b >> 3) & 0x1f);
				this->palette16[i] = clr16;
			}
			break;
	}

	this->num_dirty_rects = MAX_DIRTY_RECTS;
}

uint WindowQuickdrawSubdriver::ListModes(OTTD_Point *modes, uint max_modes)
{
	return QZ_ListModes(modes, max_modes, kCGDirectMainDisplay, this->buffer_depth);
}

bool WindowQuickdrawSubdriver::ChangeResolution(int w, int h)
{
	int old_width  = this->window_width;
	int old_height = this->window_height;

	if (this->SetVideoMode(w, h)) return true;

	if (old_width != 0 && old_height != 0) this->SetVideoMode(old_width, old_height);

	return false;
}

/* Convert local coordinate to window server (CoreGraphics) coordinate */
CGPoint WindowQuickdrawSubdriver::PrivateLocalToCG(NSPoint *p)
{
	*p = [ this->qdview convertPoint:*p toView: nil ];
	*p = [ this->window convertBaseToScreen:*p ];
	p->y = this->device_height - p->y;

	return CGPointMake(p->x, p->y);
}

NSPoint WindowQuickdrawSubdriver::GetMouseLocation(NSEvent *event)
{
	NSPoint pt = [ event locationInWindow ];
	pt = [ this->qdview convertPoint:pt fromView:nil ];

	return pt;
}

bool WindowQuickdrawSubdriver::MouseIsInsideView(NSPoint *pt)
{
	return [ this->qdview mouse:*pt inRect:[ this->qdview bounds ] ];
}


/* This function makes the *game region* of the window 100% opaque.
 * The genie effect uses the alpha component. Otherwise,
 * it doesn't seem to matter what value it has.
 */
void WindowQuickdrawSubdriver::SetPortAlphaOpaque()
{
	if (this->device_depth != 32) return;

	uint32 *pixels = (uint32*)this->window_buffer;
	uint32  pitch  = this->window_pitch / 4;

	for (int y = 0; y < this->window_height; y++)
		for (int x = 0; x < this->window_width; x++) {
		pixels[y * pitch + x] |= 0xFF000000;
	}
}

bool WindowQuickdrawSubdriver::WindowResized()
{
	if (this->window == nil || this->qdview == nil) return true;

	NSRect   newframe = [ this->qdview frame ];
	CGrafPtr thePort  = (OpaqueGrafPtr*) [ this->qdview qdPort ];

	LockPortBits(thePort);
	this->window_buffer = GetPixBaseAddr(GetPortPixMap(thePort));
	this->window_pitch = GetPixRowBytes(GetPortPixMap(thePort));
	UnlockPortBits(thePort);

	/* _cocoa_video_data.realpixels now points to the window's pixels
	 * We want it to point to the *view's* pixels
	 */
	int voff = [ this->window frame ].size.height - newframe.size.height - newframe.origin.y;
	int hoff = [ this->qdview frame ].origin.x;
	this->window_buffer = (uint8*)this->window_buffer + (voff * this->window_pitch) + hoff * (this->device_depth / 8);

	this->window_width = newframe.size.width;
	this->window_height = newframe.size.height;

	free(this->pixel_buffer);
	this->pixel_buffer = malloc(this->window_width * this->window_height * this->buffer_depth / 8);
	if (this->pixel_buffer == NULL) {
		DEBUG(driver, 0, "Failed to allocate pixel buffer");
		return false;
	}

	QZ_GameSizeChanged();

	/* Redraw screen */
	this->num_dirty_rects = MAX_DIRTY_RECTS;

	return true;
}


CocoaSubdriver *QZ_CreateWindowQuickdrawSubdriver(int width, int height, int bpp)
{
	WindowQuickdrawSubdriver *ret;

	if (MacOSVersionIsAtLeast(10, 5, 0)) {
		DEBUG(driver, 0, "The cocoa quickdraw subdriver is not recommended for Mac OS X 10.5 or later.");
	}

	if (bpp != 8 && bpp != 32) {
		DEBUG(driver, 0, "The cocoa quickdraw subdriver only supports 8 and 32 bpp.");
		return NULL;
	}

	ret = new WindowQuickdrawSubdriver(bpp);

	if (!ret->ChangeResolution(width, height)) {
		delete ret;
		return NULL;
	}

	return ret;
}

#endif /* ENABLE_COCOA_QUICKDRAW */
#endif /* WITH_COCOA */
