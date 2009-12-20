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
#include "../../functions.h"

/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */

class WindowQuartzSubdriver;


/* Subclass of NSWindow to fix genie effect and support resize events  */
@interface OTTD_QuartzWindow : NSWindow {
	WindowQuartzSubdriver *driver;
}

- (void)setDriver:(WindowQuartzSubdriver*)drv;

- (void)miniaturize:(id)sender;
- (void)display;
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag;
- (void)appDidHide:(NSNotification*)note;
- (void)appWillUnhide:(NSNotification*)note;
- (void)appDidUnhide:(NSNotification*)note;
- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag;
@end

/* Delegate for our NSWindow to send ask for quit on close */
@interface OTTD_QuartzWindowDelegate : NSObject{
	WindowQuartzSubdriver *driver;
}

- (void)setDriver:(WindowQuartzSubdriver*)drv;

- (BOOL)windowShouldClose:(id)sender;
@end

/* Subclass of NSView to fix Quartz rendering */
@interface OTTD_QuartzView : NSView {
	WindowQuartzSubdriver *driver;
}

- (void)setDriver:(WindowQuartzSubdriver*)drv;

- (void)drawRect:(NSRect)rect;
- (BOOL)isOpaque;
@end

class WindowQuartzSubdriver: public CocoaSubdriver {
	int device_width;
	int device_height;

	int window_width;
	int window_height;

	int buffer_depth;

	void *pixel_buffer;
	void *image_buffer;

	OTTD_QuartzWindow *window;

	#define MAX_DIRTY_RECTS 100
	Rect dirty_rects[MAX_DIRTY_RECTS];
	int num_dirty_rects;

	uint32 palette[256];

public:
	bool active;
	bool setup;

	OTTD_QuartzView *qzview;
	CGContextRef cgcontext;

private:
	void GetDeviceInfo();

	bool SetVideoMode(int width, int height);

	/**
	 * This function copies 8bpp pixels from the screen buffer in 32bpp windowed mode.
	 *
	 * @param left The x coord for the left edge of the box to blit.
	 * @param top The y coord for the top edge of the box to blit.
	 * @param right The x coord for the right edge of the box to blit.
	 * @param bottom The y coord for the bottom edge of the box to blit.
	 */
	void BlitIndexedToView32(int left, int top, int right, int bottom);

public:
	WindowQuartzSubdriver(int bpp);
	virtual ~WindowQuartzSubdriver();

	virtual void Draw(bool force_update);
	virtual void MakeDirty(int left, int top, int width, int height);
	virtual void UpdatePalette(uint first_color, uint num_colors);

	virtual uint ListModes(OTTD_Point *modes, uint max_modes);

	virtual bool ChangeResolution(int w, int h);

	virtual bool IsFullscreen() { return false; }

	virtual int GetWidth() { return window_width; }
	virtual int GetHeight() { return window_height; }
	virtual void *GetPixelBuffer() { return buffer_depth == 8 ? pixel_buffer : image_buffer; }

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
		CMProfileRef sysProfile;

		if (CMGetSystemProfile(&sysProfile) == noErr) {
			colorSpace = CGColorSpaceCreateWithPlatformColorSpace(sysProfile);
			CMCloseProfile(sysProfile);
		} else {
			colorSpace = CGColorSpaceCreateDeviceRGB();
		}

		if (colorSpace == NULL) error("Could not get system colour space. You might need to recalibrate your monitor.");
	}

	return colorSpace;
}


@implementation OTTD_QuartzWindow

- (void)setDriver:(WindowQuartzSubdriver*)drv
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
	[ self cacheImageInRect:[ driver->qzview frame ] ];

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
	[ self cacheImageInRect:[ driver->qzview frame ] ];
}

- (void)appDidUnhide:(NSNotification*)note
{
	/* restore cached image, since it may not be current, post expose event too */
	[ self restoreCachedImage ];

	driver->active = true;
}


- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
	/* Make our window subclass receive these application notifications */
	[ [ NSNotificationCenter defaultCenter ] addObserver:self selector:@selector(appDidHide:) name:NSApplicationDidHideNotification object:NSApp ];

	[ [ NSNotificationCenter defaultCenter ] addObserver:self selector:@selector(appDidUnhide:) name:NSApplicationDidUnhideNotification object:NSApp ];

	[ [ NSNotificationCenter defaultCenter ] addObserver:self selector:@selector(appWillUnhide:) name:NSApplicationWillUnhideNotification object:NSApp ];

	return [ super initWithContentRect:contentRect styleMask:styleMask backing:backingType defer:flag ];
}

@end

@implementation OTTD_QuartzWindowDelegate

- (void)setDriver:(WindowQuartzSubdriver*)drv
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

@implementation OTTD_QuartzView

- (void)setDriver:(WindowQuartzSubdriver*)drv
{
	driver = drv;
}


- (BOOL)isOpaque
{
	return YES;
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
		blitArea += dirtyRects[n].size.width * dirtyRects[n].size.height;
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
	/* Initialize the video settings; this data persists between mode switches */
	CFDictionaryRef cur_mode = CGDisplayCurrentMode(kCGDirectMainDisplay);

	/* Gather some information that is useful to know about the display */
	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayWidth),
		kCFNumberSInt32Type, &this->device_width
	);

	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayHeight),
		kCFNumberSInt32Type, &this->device_height
	);
}

bool WindowQuartzSubdriver::SetVideoMode(int width, int height)
{
	this->setup = true;
	this->GetDeviceInfo();

	if (width > this->device_width) width = this->device_width;
	if (height > this->device_height) height = this->device_height;

	NSRect contentRect = NSMakeRect(0, 0, width, height);

	/* Check if we should recreate the window */
	if (this->window == nil) {
		OTTD_QuartzWindowDelegate *delegate;

		/* Set the window style */
		unsigned int style = NSTitledWindowMask;
		style |= (NSMiniaturizableWindowMask | NSClosableWindowMask);
		style |= NSResizableWindowMask;

		/* Manually create a window, avoids having a nib file resource */
		this->window = [ [ OTTD_QuartzWindow alloc ]
							initWithContentRect:contentRect
							styleMask:style
							backing:NSBackingStoreBuffered
							defer:NO ];

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

		[ this->window useOptimizedDrawing:YES ];

		delegate = [ [ OTTD_QuartzWindowDelegate alloc ] init ];
		[ delegate setDriver:this ];
		[ this->window setDelegate:[ delegate autorelease ] ];
	} else {
		/* We already have a window, just change its size */
		[ this->window setContentSize:contentRect.size ];

		// Ensure frame height - title bar height >= view height
		contentRect.size.height = Clamp(height, 0, [ this->window frame ].size.height - 22 /* 22 is the height of title bar of window*/);

		if (this->qzview != nil) {
			height = contentRect.size.height;
			[ this->qzview setFrameSize:contentRect.size ];
		}
	}

	this->window_width = width;
	this->window_height = height;

	[ this->window center ];

	/* Only recreate the view if it doesn't already exist */
	if (this->qzview == nil) {
		this->qzview = [ [ OTTD_QuartzView alloc ] initWithFrame:contentRect ];
		if (this->qzview == nil) {
			DEBUG(driver, 0, "Could not create the Quickdraw view.");
			this->setup = false;
			return false;
		}

		[ this->qzview setDriver:this ];

		[ this->qzview setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable ];
		[ this->window setContentView:qzview ];
		[ this->qzview release ];
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
	uint32       *dst   = (uint32*)this->image_buffer;
	uint          width = this->window_width;
	uint          pitch = this->window_width;

	for (int y = top; y < bottom; y++) {
		for (int x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}


WindowQuartzSubdriver::WindowQuartzSubdriver(int bpp)
{
	this->window_width  = 0;
	this->window_height = 0;
	this->buffer_depth  = bpp;
	this->image_buffer  = NULL;
	this->pixel_buffer  = NULL;
	this->active        = false;
	this->setup         = false;

	this->window = nil;
	this->qzview = nil;

	this->cgcontext = NULL;

	this->num_dirty_rects = MAX_DIRTY_RECTS;
}

WindowQuartzSubdriver::~WindowQuartzSubdriver()
{
	QZ_ShowMouse();

	/* Release window mode resources */
	if (this->window != nil) [ this->window close ];

	CGContextRelease(this->cgcontext);

	free(this->image_buffer);
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
		[ this->qzview setNeedsDisplayInRect:dirtyrect ];
		if (force_update) [ this->qzview displayIfNeeded ];
	}

	//DrawResizeIcon();

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
		clr |= (uint32)_cur_palette[i].r << 16;
		clr |= (uint32)_cur_palette[i].g << 8;
		clr |= (uint32)_cur_palette[i].b;
		this->palette[i] = clr;
	}

	this->num_dirty_rects = MAX_DIRTY_RECTS;
}

uint WindowQuartzSubdriver::ListModes(OTTD_Point *modes, uint max_modes)
{
	return QZ_ListModes(modes, max_modes, kCGDirectMainDisplay, this->buffer_depth);
}

bool WindowQuartzSubdriver::ChangeResolution(int w, int h)
{
	int old_width  = this->window_width;
	int old_height = this->window_height;

	if (this->SetVideoMode(w, h)) return true;
	if (old_width != 0 && old_height != 0) this->SetVideoMode(old_width, old_height);

	return false;
}

/* Convert local coordinate to window server (CoreGraphics) coordinate */
CGPoint WindowQuartzSubdriver::PrivateLocalToCG(NSPoint *p)
{

	p->y = this->window_height - p->y;
	*p = [ this->qzview convertPoint:*p toView:nil ];

	*p = [ this->window convertBaseToScreen:*p ];
	p->y = this->device_height - p->y;

	CGPoint cgp;
	cgp.x = p->x;
	cgp.y = p->y;

	return cgp;
}

NSPoint WindowQuartzSubdriver::GetMouseLocation(NSEvent *event)
{
	NSPoint pt = [ event locationInWindow ];
	pt = [ this->qzview convertPoint:pt fromView:nil ];

	pt.y = this->window_height - pt.y;

	return pt;
}

bool WindowQuartzSubdriver::MouseIsInsideView(NSPoint *pt)
{
	return [ qzview mouse:*pt inRect:[ this->qzview bounds ] ];
}


/* This function makes the *game region* of the window 100% opaque.
 * The genie effect uses the alpha component. Otherwise,
 * it doesn't seem to matter what value it has.
 */
void WindowQuartzSubdriver::SetPortAlphaOpaque()
{
	uint32 *pixels = (uint32*)this->image_buffer;
	uint32  pitch  = this->window_width;

	for (int y = 0; y < this->window_height; y++)
		for (int x = 0; x < this->window_width; x++) {
		pixels[y * pitch + x] |= 0xFF000000;
	}
}

bool WindowQuartzSubdriver::WindowResized()
{
	if (this->window == nil || this->qzview == nil) return true;

	NSRect newframe = [ this->qzview frame ];

	this->window_width = newframe.size.width;
	this->window_height = newframe.size.height;

	/* Create Core Graphics Context */
	free(this->image_buffer);
	this->image_buffer = (uint32*)malloc(this->window_width * this->window_height * 4);

	CGContextRelease(this->cgcontext);
	this->cgcontext = CGBitmapContextCreate(
		this->image_buffer,        // data
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

	WindowQuartzSubdriver *ret = new WindowQuartzSubdriver(bpp);

	if (!ret->ChangeResolution(width, height)) {
		delete ret;
		return NULL;
	}

	return ret;
}


#endif /* MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4 */
#endif /* ENABLE_COCOA_QUARTZ */
#endif /* WITH_COCOA */
