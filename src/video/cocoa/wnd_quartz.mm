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
#ifdef ENABLE_COCOA_QUARTZ

#include "../../stdafx.h"
#include "../../os/macosx/macos.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4

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

/* On some old versions of MAC OS this may not be defined.
 * Those versions generally only produce code for PPC. So it should be safe to
 * set this to 0. */
#ifndef kCGBitmapByteOrder32Host
#define kCGBitmapByteOrder32Host 0
#endif

/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */

class WindowQuartzSubdriver;

/* Subclass of OTTD_CocoaView to fix Quartz rendering */
@interface OTTD_QuartzView : OTTD_CocoaView
- (void)setDriver:(WindowQuartzSubdriver*)drv;
- (void)drawRect:(NSRect)invalidRect;
@end

class WindowQuartzSubdriver : public CocoaSubdriver {
private:
	/**
	 * This function copies 8bpp pixels from the screen buffer in 32bpp windowed mode.
	 *
	 * @param left The x coord for the left edge of the box to blit.
	 * @param top The y coord for the top edge of the box to blit.
	 * @param right The x coord for the right edge of the box to blit.
	 * @param bottom The y coord for the bottom edge of the box to blit.
	 */
	void BlitIndexedToView32(int left, int top, int right, int bottom);

	virtual void GetDeviceInfo();
	virtual bool SetVideoMode(int width, int height, int bpp);

public:
	WindowQuartzSubdriver();
	virtual ~WindowQuartzSubdriver();

	virtual void Draw(bool force_update);
	virtual void MakeDirty(int left, int top, int width, int height);
	virtual void UpdatePalette(uint first_color, uint num_colors);

	virtual uint ListModes(OTTD_Point *modes, uint max_modes);

	virtual bool ChangeResolution(int w, int h, int bpp);

	virtual bool IsFullscreen() { return false; }
	virtual bool ToggleFullscreen(); /* Full screen mode on OSX 10.7 */

	virtual int GetWidth() { return window_width; }
	virtual int GetHeight() { return window_height; }
	virtual void *GetPixelBuffer() { return buffer_depth == 8 ? pixel_buffer : window_buffer; }

	/* Convert local coordinate to window server (CoreGraphics) coordinate */
	virtual CGPoint PrivateLocalToCG(NSPoint *p);

	virtual NSPoint GetMouseLocation(NSEvent *event);
	virtual bool MouseIsInsideView(NSPoint *pt);

	virtual bool IsActive() { return active; }


	void SetPortAlphaOpaque();
	bool WindowResized();
};


static CGColorSpaceRef QZ_GetCorrectColorSpace()
{
	static CGColorSpaceRef colorSpace = NULL;

	if (colorSpace == NULL) {
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
		if (MacOSVersionIsAtLeast(10, 5, 0)) {
			colorSpace = CGDisplayCopyColorSpace(CGMainDisplayID());
		} else
#endif
		{
#if (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5) && !defined(HAVE_OSX_1011_SDK)
			CMProfileRef sysProfile;
			if (CMGetSystemProfile(&sysProfile) == noErr) {
				colorSpace = CGColorSpaceCreateWithPlatformColorSpace(sysProfile);
				CMCloseProfile(sysProfile);
			}
#endif
		}

		if (colorSpace == NULL) colorSpace = CGColorSpaceCreateDeviceRGB();

		if (colorSpace == NULL) error("Could not get system colour space. You might need to recalibrate your monitor.");
	}

	return colorSpace;
}


@implementation OTTD_QuartzView

- (void)setDriver:(WindowQuartzSubdriver*)drv
{
	driver = drv;
}
- (void)drawRect:(NSRect)invalidRect
{
	if (driver->cgcontext == NULL) return;

	CGContextRef viewContext = (CGContextRef)[ [ NSGraphicsContext currentContext ] graphicsPort ];
	CGContextSetShouldAntialias(viewContext, FALSE);
	CGContextSetInterpolationQuality(viewContext, kCGInterpolationNone);

	/* The obtained 'rect' is actually a union of all dirty rects, let's ask for an explicit list of rects instead */
	const NSRect *dirtyRects;
	NSInteger     dirtyRectCount;
	[ self getRectsBeingDrawn:&dirtyRects count:&dirtyRectCount ];

	/* We need an Image in order to do blitting, but as we don't touch the context between this call and drawing no copying will actually be done here */
	CGImageRef fullImage = CGBitmapContextCreateImage(driver->cgcontext);

	/* Calculate total area we are blitting */
	uint32 blitArea = 0;
	for (int n = 0; n < dirtyRectCount; n++) {
		blitArea += (uint32)(dirtyRects[n].size.width * dirtyRects[n].size.height);
	}

	/*
	 * This might be completely stupid, but in my extremely subjective opinion it feels faster
	 * The point is, if we're blitting less than 50% of the dirty rect union then it's still a good idea to blit each dirty
	 * rect separately but if we blit more than that, it's just cheaper to blit the entire union in one pass.
	 * Feel free to remove or find an even better value than 50% ... / blackis
	 */
	NSRect frameRect = [ self frame ];
	if (blitArea / (float)(invalidRect.size.width * invalidRect.size.height) > 0.5f) {
		NSRect rect = invalidRect;
		CGRect clipRect;
		CGRect blitRect;

		blitRect.origin.x = rect.origin.x;
		blitRect.origin.y = rect.origin.y;
		blitRect.size.width = rect.size.width;
		blitRect.size.height = rect.size.height;

		clipRect.origin.x = rect.origin.x;
		clipRect.origin.y = frameRect.size.height - rect.origin.y - rect.size.height;

		clipRect.size.width = rect.size.width;
		clipRect.size.height = rect.size.height;

		/* Blit dirty part of image */
		CGImageRef clippedImage = CGImageCreateWithImageInRect(fullImage, clipRect);
		CGContextDrawImage(viewContext, blitRect, clippedImage);
		CGImageRelease(clippedImage);
	} else {
		for (int n = 0; n < dirtyRectCount; n++) {
			NSRect rect = dirtyRects[n];
			CGRect clipRect;
			CGRect blitRect;

			blitRect.origin.x = rect.origin.x;
			blitRect.origin.y = rect.origin.y;
			blitRect.size.width = rect.size.width;
			blitRect.size.height = rect.size.height;

			clipRect.origin.x = rect.origin.x;
			clipRect.origin.y = frameRect.size.height - rect.origin.y - rect.size.height;

			clipRect.size.width = rect.size.width;
			clipRect.size.height = rect.size.height;

			/* Blit dirty part of image */
			CGImageRef clippedImage = CGImageCreateWithImageInRect(fullImage, clipRect);
			CGContextDrawImage(viewContext, blitRect, clippedImage);
			CGImageRelease(clippedImage);
		}
	}

	CGImageRelease(fullImage);
}

@end


void WindowQuartzSubdriver::GetDeviceInfo()
{
	/* Initialize the video settings; this data persists between mode switches
	 * and gather some information that is useful to know about the display */

#	if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
	/* This way is deprecated as of OSX 10.6 but continues to work.Thus use it
	 * always, unless allowed to skip compatibility with 10.5 and earlier */
	CFDictionaryRef cur_mode = CGDisplayCurrentMode(kCGDirectMainDisplay);

	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayWidth),
		kCFNumberSInt32Type, &this->device_width
	);

	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayHeight),
		kCFNumberSInt32Type, &this->device_height
	);
#	else
	/* Use the new API when compiling for OSX 10.6 or later */
	CGDisplayModeRef cur_mode = CGDisplayCopyDisplayMode(kCGDirectMainDisplay);
	if (cur_mode == NULL) { return; }

	this->device_width = CGDisplayModeGetWidth(cur_mode);
	this->device_height = CGDisplayModeGetHeight(cur_mode);

	CGDisplayModeRelease(cur_mode);
#	endif
}

/** Switch to full screen mode on OSX 10.7
 * @return Whether we switched to full screen
 */
bool WindowQuartzSubdriver::ToggleFullscreen()
{
	if ([ this->window respondsToSelector:@selector(toggleFullScreen:) ]) {
		[ this->window performSelector:@selector(toggleFullScreen:) withObject:this->window ];
		return true;
	}

	return false;
}

bool WindowQuartzSubdriver::SetVideoMode(int width, int height, int bpp)
{
	this->setup = true;
	this->GetDeviceInfo();

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
		this->window = [ [ OTTD_CocoaWindow alloc ]
							initWithContentRect:contentRect
							styleMask:style
							backing:NSBackingStoreBuffered
							defer:NO ];

		if (this->window == nil) {
			DEBUG(driver, 0, "Could not create the Cocoa window.");
			this->setup = false;
			return false;
		}

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
		/* Add built in full-screen support when available (OS X 10.7 and higher)
		 * This code actually compiles for 10.5 and later, but only makes sense in conjunction
		 * with the quartz fullscreen support as found only in 10.7 and later
		 */
		if ([this->window respondsToSelector:@selector(toggleFullScreen:)]) {
#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
			/* Constants needed to build on pre-10.7 SDKs. Source: NSWindow documentation. */
			const int NSWindowCollectionBehaviorFullScreenPrimary = 1 << 7;
			const int NSWindowFullScreenButton = 7;
#endif

			NSWindowCollectionBehavior behavior = [ this->window collectionBehavior ];
			behavior |= NSWindowCollectionBehaviorFullScreenPrimary;
			[ this->window setCollectionBehavior:behavior ];

			NSButton* fullscreenButton = [ this->window standardWindowButton:NSWindowFullScreenButton ];
			[ fullscreenButton setAction:@selector(toggleFullScreen:) ];
			[ fullscreenButton setTarget:this->window ];

			[ this->window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary ];
		}
#endif

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

		[ this->window useOptimizedDrawing:YES ];

		delegate = [ [ OTTD_CocoaWindowDelegate alloc ] init ];
		[ delegate setDriver:this ];
		[ this->window setDelegate:[ delegate autorelease ] ];
	} else {
		/* We already have a window, just change its size */
		[ this->window setContentSize:contentRect.size ];

		/* Ensure frame height - title bar height >= view height */
		contentRect.size.height = Clamp(height, 0, (int)[ this->window frame ].size.height - 22 /* 22 is the height of title bar of window*/);

		if (this->cocoaview != nil) {
			height = (int)contentRect.size.height;
			[ this->cocoaview setFrameSize:contentRect.size ];
		}
	}

	this->window_width = width;
	this->window_height = height;
	this->buffer_depth = bpp;

	[ this->window center ];

	/* Only recreate the view if it doesn't already exist */
	if (this->cocoaview == nil) {
		this->cocoaview = [ [ OTTD_QuartzView alloc ] initWithFrame:contentRect ];
		if (this->cocoaview == nil) {
			DEBUG(driver, 0, "Could not create the Quartz view.");
			this->setup = false;
			return false;
		}

		[ this->cocoaview setDriver:this ];

		[ (NSView*)this->cocoaview setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable ];
		[ this->window setContentView:cocoaview ];
		[ this->cocoaview release ];
		[ this->window makeKeyAndOrderFront:nil ];
	}

	bool ret = WindowResized();
	this->UpdatePalette(0, 256);

	this->setup = false;

	return ret;
}

void WindowQuartzSubdriver::BlitIndexedToView32(int left, int top, int right, int bottom)
{
	const uint32 *pal   = this->palette;
	const uint8  *src   = (uint8*)this->pixel_buffer;
	uint32       *dst   = (uint32*)this->window_buffer;
	uint          width = this->window_width;
	uint          pitch = this->window_width;

	for (int y = top; y < bottom; y++) {
		for (int x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}


WindowQuartzSubdriver::WindowQuartzSubdriver()
{
	this->window_width  = 0;
	this->window_height = 0;
	this->buffer_depth  = 0;
	this->window_buffer  = NULL;
	this->pixel_buffer  = NULL;
	this->active        = false;
	this->setup         = false;

	this->window = nil;
	this->cocoaview = nil;

	this->cgcontext = NULL;

	this->num_dirty_rects = MAX_DIRTY_RECTS;
}

WindowQuartzSubdriver::~WindowQuartzSubdriver()
{
	/* Release window mode resources */
	if (this->window != nil) [ this->window close ];

	CGContextRelease(this->cgcontext);

	free(this->window_buffer);
	free(this->pixel_buffer);
}

void WindowQuartzSubdriver::Draw(bool force_update)
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

	/* Build the region of dirty rectangles */
	for (int i = 0; i < this->num_dirty_rects; i++) {
		/* We only need to blit in indexed mode since in 32bpp mode the game draws directly to the image. */
		if (this->buffer_depth == 8) {
			BlitIndexedToView32(
				this->dirty_rects[i].left,
				this->dirty_rects[i].top,
				this->dirty_rects[i].right,
				this->dirty_rects[i].bottom
			);
		}

		NSRect dirtyrect;
		dirtyrect.origin.x = this->dirty_rects[i].left;
		dirtyrect.origin.y = this->window_height - this->dirty_rects[i].bottom;
		dirtyrect.size.width = this->dirty_rects[i].right - this->dirty_rects[i].left;
		dirtyrect.size.height = this->dirty_rects[i].bottom - this->dirty_rects[i].top;

		/* Normally drawRect will be automatically called by Mac OS X during next update cycle,
		 * and then blitting will occur. If force_update is true, it will be done right now. */
		[ this->cocoaview setNeedsDisplayInRect:dirtyrect ];
		if (force_update) [ this->cocoaview displayIfNeeded ];
	}

	this->num_dirty_rects = 0;
}

void WindowQuartzSubdriver::MakeDirty(int left, int top, int width, int height)
{
	if (this->num_dirty_rects < MAX_DIRTY_RECTS) {
		dirty_rects[this->num_dirty_rects].left = left;
		dirty_rects[this->num_dirty_rects].top = top;
		dirty_rects[this->num_dirty_rects].right = left + width;
		dirty_rects[this->num_dirty_rects].bottom = top + height;
	}
	this->num_dirty_rects++;
}

void WindowQuartzSubdriver::UpdatePalette(uint first_color, uint num_colors)
{
	if (this->buffer_depth != 8) return;

	for (uint i = first_color; i < first_color + num_colors; i++) {
		uint32 clr = 0xff000000;
		clr |= (uint32)_cur_palette.palette[i].r << 16;
		clr |= (uint32)_cur_palette.palette[i].g << 8;
		clr |= (uint32)_cur_palette.palette[i].b;
		this->palette[i] = clr;
	}

	this->num_dirty_rects = MAX_DIRTY_RECTS;
}

uint WindowQuartzSubdriver::ListModes(OTTD_Point *modes, uint max_modes)
{
	return QZ_ListModes(modes, max_modes, kCGDirectMainDisplay, this->buffer_depth);
}

bool WindowQuartzSubdriver::ChangeResolution(int w, int h, int bpp)
{
	int old_width  = this->window_width;
	int old_height = this->window_height;
	int old_bpp    = this->buffer_depth;

	if (this->SetVideoMode(w, h, bpp)) return true;
	if (old_width != 0 && old_height != 0) this->SetVideoMode(old_width, old_height, old_bpp);

	return false;
}

/* Convert local coordinate to window server (CoreGraphics) coordinate */
CGPoint WindowQuartzSubdriver::PrivateLocalToCG(NSPoint *p)
{

	p->y = this->window_height - p->y;
	*p = [ this->cocoaview convertPoint:*p toView:nil ];

	*p = [ this->window convertBaseToScreen:*p ];
	p->y = this->device_height - p->y;

	CGPoint cgp;
	cgp.x = p->x;
	cgp.y = p->y;

	return cgp;
}

NSPoint WindowQuartzSubdriver::GetMouseLocation(NSEvent *event)
{
	NSPoint pt;

	if ( [ event window ] == nil) {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
		if ([ this->cocoaview respondsToSelector:@selector(convertRectFromScreen:) ]) {
			pt = [ this->cocoaview convertPoint:[ [ this->cocoaview window ] convertRectFromScreen:NSMakeRect([ event locationInWindow ].x, [ event locationInWindow ].y, 0, 0) ].origin fromView:nil ];
		}
		else
#endif
		{
			pt = [ this->cocoaview convertPoint:[ [ this->cocoaview window ] convertScreenToBase:[ event locationInWindow ] ] fromView:nil ];
		}
	} else {
		pt = [ event locationInWindow ];
	}

	pt.y = this->window_height - pt.y;

	return pt;
}

bool WindowQuartzSubdriver::MouseIsInsideView(NSPoint *pt)
{
	return [ cocoaview mouse:*pt inRect:[ this->cocoaview bounds ] ];
}


/* This function makes the *game region* of the window 100% opaque.
 * The genie effect uses the alpha component. Otherwise,
 * it doesn't seem to matter what value it has.
 */
void WindowQuartzSubdriver::SetPortAlphaOpaque()
{
	uint32 *pixels = (uint32*)this->window_buffer;
	uint32  pitch  = this->window_width;

	for (int y = 0; y < this->window_height; y++)
		for (int x = 0; x < this->window_width; x++) {
		pixels[y * pitch + x] |= 0xFF000000;
	}
}

bool WindowQuartzSubdriver::WindowResized()
{
	if (this->window == nil || this->cocoaview == nil) return true;

	NSRect newframe = [ this->cocoaview frame ];

	this->window_width = (int)newframe.size.width;
	this->window_height = (int)newframe.size.height;

	/* Create Core Graphics Context */
	free(this->window_buffer);
	this->window_buffer = (uint32*)malloc(this->window_width * this->window_height * 4);

	CGContextRelease(this->cgcontext);
	this->cgcontext = CGBitmapContextCreate(
		this->window_buffer,        // data
		this->window_width,        // width
		this->window_height,       // height
		8,                         // bits per component
		this->window_width * 4,    // bytes per row
		QZ_GetCorrectColorSpace(), // color space
		kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host
	);

	assert(this->cgcontext != NULL);
	CGContextSetShouldAntialias(this->cgcontext, FALSE);
	CGContextSetAllowsAntialiasing(this->cgcontext, FALSE);
	CGContextSetInterpolationQuality(this->cgcontext, kCGInterpolationNone);

	if (this->buffer_depth == 8) {
		free(this->pixel_buffer);
		this->pixel_buffer = malloc(this->window_width * this->window_height);
		if (this->pixel_buffer == NULL) {
			DEBUG(driver, 0, "Failed to allocate pixel buffer");
			return false;
		}
	}

	QZ_GameSizeChanged();

	/* Redraw screen */
	this->num_dirty_rects = MAX_DIRTY_RECTS;

	return true;
}


CocoaSubdriver *QZ_CreateWindowQuartzSubdriver(int width, int height, int bpp)
{
	if (!MacOSVersionIsAtLeast(10, 4, 0)) {
		DEBUG(driver, 0, "The cocoa quartz subdriver requires Mac OS X 10.4 or later.");
		return NULL;
	}

	if (bpp != 8 && bpp != 32) {
		DEBUG(driver, 0, "The cocoa quartz subdriver only supports 8 and 32 bpp.");
		return NULL;
	}

	WindowQuartzSubdriver *ret = new WindowQuartzSubdriver();

	if (!ret->ChangeResolution(width, height, bpp)) {
		delete ret;
		return NULL;
	}

	return ret;
}


#endif /* MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4 */
#endif /* ENABLE_COCOA_QUARTZ */
#endif /* WITH_COCOA */
