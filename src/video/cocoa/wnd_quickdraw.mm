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
#include "../../framerate_type.h"

/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */


class WindowQuickdrawSubdriver;


class WindowQuickdrawSubdriver : public CocoaSubdriver {
private:
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

	virtual void GetDeviceInfo();
	virtual bool SetVideoMode(int width, int height, int bpp);

public:
	WindowQuickdrawSubdriver();
	virtual ~WindowQuickdrawSubdriver();

	virtual void Draw(bool force_update);
	virtual void MakeDirty(int left, int top, int width, int height);
	virtual void UpdatePalette(uint first_color, uint num_colors);

	virtual uint ListModes(OTTD_Point *modes, uint max_modes);

	virtual bool ChangeResolution(int w, int h, int bpp);

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

bool WindowQuickdrawSubdriver::SetVideoMode(int width, int height, int bpp)
{
	this->setup = true;
	this->GetDeviceInfo();

	if (bpp > this->device_depth) {
		DEBUG(driver, 0, "Cannot use a blitter with a higher screen depth than the display when running in windowed mode.");
		this->setup = false;
		return false;
	}

	if (width > this->device_width) width = this->device_width;
	if (height > this->device_height) height = this->device_height;

	NSRect contentRect = NSMakeRect(0, 0, width, height);

	/* Check if we should recreate the window */
	if (this->window == nil) {
		OTTD_CocoaWindowDelegate *delegate;

		/* Set the window style */
		unsigned int style = NSTitledWindowMask;
		style |= (NSMiniaturizableWindowMask | NSClosableWindowMask);
		style |= NSResizableWindowMask;

		/* Manually create a window, avoids having a nib file resource */
		this->window = [ [ OTTD_CocoaWindow alloc ] initWithContentRect:contentRect
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

		[ this->window setContentMinSize:NSMakeSize(64.0f, 64.0f) ];

		[ this->window setAcceptsMouseMovedEvents:YES ];
		[ this->window setViewsNeedDisplay:NO ];

		delegate = [ [ OTTD_CocoaWindowDelegate alloc ] init ];
		[ delegate setDriver:this ];
		[ this->window setDelegate: [ delegate autorelease ] ];
	} else {
		/* We already have a window, just change its size */
		[ this->window setContentSize:contentRect.size ];
		/* Ensure frame height - title bar height >= view height
		 * The height of title bar of the window is 22 pixels */
		contentRect.size.height = Clamp(height, 0, [ this->window frame ].size.height - 22);
		height = contentRect.size.height;
		[ this->cocoaview setFrameSize:contentRect.size ];
	}

	/* Update again */
	this->window_width = width;
	this->window_height = height;
	this->buffer_depth = bpp;

	[ this->window center ];

	/* Only recreate the view if it doesn't already exist */
	if (this->cocoaview == nil) {
		this->cocoaview = [ [ NSQuickDrawView alloc ] initWithFrame:contentRect ];
		if (this->cocoaview == nil) {
			DEBUG(driver, 0, "Could not create the Quickdraw view.");
			this->setup = false;
			return false;
		}

		[ this->cocoaview setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable ];
		[ [ this->window contentView ] addSubview:this->cocoaview ];
		[ this->cocoaview release ];
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
	const uint32 *pal   = this->palette;
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
	const uint32 *pal   = this->palette;
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


WindowQuickdrawSubdriver::WindowQuickdrawSubdriver()
{
	this->window_width  = 0;
	this->window_height = 0;
	this->buffer_depth  = 0;
	this->pixel_buffer  = NULL;
	this->active        = false;
	this->setup         = false;

	this->window = nil;
	this->cocoaview = nil;

	this->num_dirty_rects = MAX_DIRTY_RECTS;
}

WindowQuickdrawSubdriver::~WindowQuickdrawSubdriver()
{
	/* Release window mode resources */
	if (this->window != nil) [ this->window close ];

	free(this->pixel_buffer);
}

void WindowQuickdrawSubdriver::Draw(bool force_update)
{
	PerformanceMeasurer framerate(PFE_VIDEO);

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
	QDFlushPortBuffer( (OpaqueGrafPtr*) [ this->cocoaview qdPort ], dirty);
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
				clr32 |= (uint32)_cur_palette.palette[i].r << 16;
				clr32 |= (uint32)_cur_palette.palette[i].g << 8;
				clr32 |= (uint32)_cur_palette.palette[i].b;
				this->palette[i] = clr32;
			}
			break;
		case 16:
			for (uint i = first_color; i < first_color + num_colors; i++) {
				uint16 clr16 = 0x0000;
				clr16 |= (uint16)((_cur_palette.palette[i].r >> 3) & 0x1f) << 10;
				clr16 |= (uint16)((_cur_palette.palette[i].g >> 3) & 0x1f) << 5;
				clr16 |= (uint16)((_cur_palette.palette[i].b >> 3) & 0x1f);
				this->palette[i] = clr16;
			}
			break;
	}

	this->num_dirty_rects = MAX_DIRTY_RECTS;
}

uint WindowQuickdrawSubdriver::ListModes(OTTD_Point *modes, uint max_modes)
{
	return QZ_ListModes(modes, max_modes, kCGDirectMainDisplay, this->buffer_depth);
}

bool WindowQuickdrawSubdriver::ChangeResolution(int w, int h, int bpp)
{
	int old_width  = this->window_width;
	int old_height = this->window_height;
	int old_bpp    = this->buffer_depth;

	if (this->SetVideoMode(w, h, bpp)) return true;

	if (old_width != 0 && old_height != 0) this->SetVideoMode(old_width, old_height, old_bpp);

	return false;
}

/* Convert local coordinate to window server (CoreGraphics) coordinate */
CGPoint WindowQuickdrawSubdriver::PrivateLocalToCG(NSPoint *p)
{
	*p = [ this->cocoaview convertPoint:*p toView: nil ];
	*p = [ this->window convertBaseToScreen:*p ];
	p->y = this->device_height - p->y;

	return CGPointMake(p->x, p->y);
}

NSPoint WindowQuickdrawSubdriver::GetMouseLocation(NSEvent *event)
{
	NSPoint pt = [ event locationInWindow ];
	pt = [ this->cocoaview convertPoint:pt fromView:nil ];

	return pt;
}

bool WindowQuickdrawSubdriver::MouseIsInsideView(NSPoint *pt)
{
	return [ this->cocoaview mouse:*pt inRect:[ this->cocoaview bounds ] ];
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
	if (this->window == nil || this->cocoaview == nil) return true;

	NSRect   newframe = [ this->cocoaview frame ];
	CGrafPtr thePort  = (OpaqueGrafPtr*) [ this->cocoaview qdPort ];

	LockPortBits(thePort);
	this->window_buffer = GetPixBaseAddr(GetPortPixMap(thePort));
	this->window_pitch = GetPixRowBytes(GetPortPixMap(thePort));
	UnlockPortBits(thePort);

	/* _cocoa_video_data.realpixels now points to the window's pixels
	 * We want it to point to the *view's* pixels
	 */
	int voff = [ this->window frame ].size.height - newframe.size.height - newframe.origin.y;
	int hoff = [ this->cocoaview frame ].origin.x;
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

	ret = new WindowQuickdrawSubdriver();

	if (!ret->ChangeResolution(width, height, bpp)) {
		delete ret;
		return NULL;
	}

	return ret;
}

#endif /* ENABLE_COCOA_QUICKDRAW */
#endif /* WITH_COCOA */
