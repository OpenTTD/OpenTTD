/* $Id$ */

/******************************************************************************
 *                             Cocoa video driver                             *
 * Known things left to do:                                                   *
 *  List available resolutions.                                               *
 ******************************************************************************/

#ifdef WITH_COCOA

#include <AvailabilityMacros.h>

#import <Cocoa/Cocoa.h>
#import <sys/time.h> /* gettimeofday */
#import <sys/param.h> /* for MAXPATHLEN */
#import <unistd.h>

/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */


/* Defined in stdbool.h */
#ifndef __cplusplus
# ifndef __BEOS__
#  undef bool
#  undef false
#  undef true
# endif
#endif


#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../debug.h"
#include "../../macros.h"
#include "../../os/macosx/splash.h"
#include "../../variables.h"
#include "../../gfx.h"
#include "cocoa_v.h"
#include "cocoa_keys.h"
#include "../../blitter/factory.hpp"
#include "../../fileio.h"

#undef Point
#undef Rect


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
- (id)initWithContentRect:(NSRect)contentRect styleMask:(unsigned int)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag;
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

	void* pixel_buffer;
	void* image_buffer;

	OTTD_QuartzWindow *window;

	#define MAX_DIRTY_RECTS 100
	Rect dirty_rects[MAX_DIRTY_RECTS];
	int num_dirty_rects;

	uint32 palette[256];

public:
	bool active;
	bool setup;

	OTTD_QuartzView* qzview;
	CGContextRef 	 cgcontext;

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

	virtual void Draw();
	virtual void MakeDirty(int left, int top, int width, int height);
	virtual void UpdatePalette(uint first_color, uint num_colors);

	virtual uint ListModes(OTTDPoint* modes, uint max_modes);

	virtual bool ChangeResolution(int w, int h);

	virtual bool IsFullscreen() { return false; }

	virtual int GetWidth() { return window_width; }
	virtual int GetHeight() { return window_height; }
	virtual void *GetPixelBuffer() { return buffer_depth == 8 ? pixel_buffer : image_buffer; }

	/* Convert local coordinate to window server (CoreGraphics) coordinate */
	virtual CGPoint PrivateLocalToCG(NSPoint* p);

	virtual NSPoint GetMouseLocation(NSEvent *event);
	virtual bool MouseIsInsideView(NSPoint *pt);

	virtual bool IsActive() { return active; }


	void SetPortAlphaOpaque();
	bool WindowResized();
};


static CGColorSpaceRef QZ_GetCorrectColorSpace()
{
	static CGColorSpaceRef colorSpace = NULL;

	if (colorSpace == NULL)
	{
		CMProfileRef sysProfile;

		if (CMGetSystemProfile(&sysProfile) == noErr)
		{
			colorSpace = CGColorSpaceCreateWithPlatformColorSpace(sysProfile);
			CMCloseProfile(sysProfile);
		}

		assert(colorSpace != NULL);
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

	if (!driver->WindowResized())
		error("Cocoa: Failed to resize window.");
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
	CGImageRef    fullImage;
	CGImageRef    clippedImage;
	NSRect        rect;
	const NSRect* dirtyRects;
	int           dirtyRectCount;
	int           n;
	CGRect        clipRect;
	CGRect        blitRect;
	uint32        blitArea       = 0;
	NSRect        frameRect      = [ self frame ];
	CGContextRef  viewContext    = (CGContextRef)[ [ NSGraphicsContext currentContext ] graphicsPort ];

	if (driver->cgcontext == NULL) return;

	CGContextSetShouldAntialias(viewContext, FALSE);
	CGContextSetInterpolationQuality(viewContext, kCGInterpolationNone);

	/* The obtained 'rect' is actually a union of all dirty rects, let's ask for an explicit list of rects instead */
	[ self getRectsBeingDrawn:&dirtyRects count:&dirtyRectCount ];

	/* We need an Image in order to do blitting, but as we don't touch the context between this call and drawing no copying will actually be done here */
	fullImage = CGBitmapContextCreateImage(driver->cgcontext);

	/* Calculate total area we are blitting */
	for (n = 0; n < dirtyRectCount; n++) {
		blitArea += dirtyRects[n].size.width * dirtyRects[n].size.height;
 	}

	/*
	 * This might be completely stupid, but in my extremely subjective opinion it feels faster
	 * The point is, if we're blitting less than 50% of the dirty rect union then it's still a good idea to blit each dirty
	 * rect separately but if we blit more than that, it's just cheaper to blit the entire union in one pass.
	 * Feel free to remove or find an even better value than 50% ... / blackis
	 */
	if (blitArea / (float)(invalidRect.size.width * invalidRect.size.height) > 0.5f) {
		rect = invalidRect;

		blitRect.origin.x = rect.origin.x;
		blitRect.origin.y = rect.origin.y;
		blitRect.size.width = rect.size.width;
		blitRect.size.height = rect.size.height;

		clipRect.origin.x = rect.origin.x;
		clipRect.origin.y = frameRect.size.height - rect.origin.y - rect.size.height;

		clipRect.size.width = rect.size.width;
		clipRect.size.height = rect.size.height;

		/* Blit dirty part of image */
		clippedImage = CGImageCreateWithImageInRect(fullImage, clipRect);
		CGContextDrawImage(viewContext, blitRect, clippedImage);
		CGImageRelease(clippedImage);
	} else {
		for (n = 0; n < dirtyRectCount; n++) {
			rect = dirtyRects[n];

			blitRect.origin.x = rect.origin.x;
			blitRect.origin.y = rect.origin.y;
			blitRect.size.width = rect.size.width;
			blitRect.size.height = rect.size.height;

			clipRect.origin.x = rect.origin.x;
			clipRect.origin.y = frameRect.size.height - rect.origin.y - rect.size.height;

			clipRect.size.width = rect.size.width;
			clipRect.size.height = rect.size.height;

			/* Blit dirty part of image */
			clippedImage = CGImageCreateWithImageInRect(fullImage, clipRect);
			CGContextDrawImage(viewContext, blitRect, clippedImage);
			CGImageRelease(clippedImage);
 		}
 	}

	CGImageRelease(fullImage);
}

@end


extern const char _openttd_revision[];


void WindowQuartzSubdriver::GetDeviceInfo()
{
	CFDictionaryRef    cur_mode;

	/* Initialize the video settings; this data persists between mode switches */
	cur_mode = CGDisplayCurrentMode(kCGDirectMainDisplay);

	/* Gather some information that is useful to know about the display */
	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayWidth),
		kCFNumberSInt32Type, &device_width
	);

	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayHeight),
		kCFNumberSInt32Type, &device_height
	);
}

bool WindowQuartzSubdriver::SetVideoMode(int width, int height)
{
	char caption[50];
	NSString *nsscaption;
	unsigned int style;
	NSRect contentRect;
	BOOL isCustom = NO;
	bool ret;

	setup = true;

	GetDeviceInfo();

	if (width > device_width)
		width = device_width;
	if (height > device_height)
		height = device_height;

	contentRect = NSMakeRect(0, 0, width, height);

	/* Check if we should recreate the window */
	if (window == nil) {
		OTTD_QuartzWindowDelegate *delegate;

		/* Set the window style */
		style = NSTitledWindowMask;
		style |= (NSMiniaturizableWindowMask | NSClosableWindowMask);
		style |= NSResizableWindowMask;

		/* Manually create a window, avoids having a nib file resource */
		window = [ [ OTTD_QuartzWindow alloc ]
						initWithContentRect: contentRect
						styleMask: style
						backing: NSBackingStoreBuffered
						defer: NO ];

		if (window == nil) {
			DEBUG(driver, 0, "Could not create the Cocoa window.");
			setup = false;
			return false;
		}

		[ window setDriver:this ];

		snprintf(caption, sizeof(caption), "OpenTTD %s", _openttd_revision);
		nsscaption = [ [ NSString alloc ] initWithCString:caption ];
		[ window setTitle: nsscaption ];
		[ window setMiniwindowTitle: nsscaption ];
		[ nsscaption release ];

		[ window setAcceptsMouseMovedEvents: YES ];
		[ window setViewsNeedDisplay: NO ];

		[ window useOptimizedDrawing: YES ];

		delegate = [ [ OTTD_QuartzWindowDelegate alloc ] init ];
		[ delegate setDriver: this ];
		[ window setDelegate: [ delegate autorelease ] ];
	} else {
		/* We already have a window, just change its size */
		if (!isCustom) {
			[ window setContentSize: contentRect.size ];

			// Ensure frame height - title bar height >= view height
			contentRect.size.height = Clamp(height, 0, [ window frame ].size.height - 22 /* 22 is the height of title bar of window*/);

			if (qzview != nil) {
				height = contentRect.size.height;
				[ qzview setFrameSize: contentRect.size ];
			}
		}
	}

	window_width = width;
	window_height = height;

	[ window center ];

	/* Only recreate the view if it doesn't already exist */
	if (qzview == nil) {
		qzview = [ [ OTTD_QuartzView alloc ] initWithFrame: contentRect ];
		if (qzview == nil) {
			DEBUG(driver, 0, "Could not create the Quickdraw view.");
			setup = false;
			return false;
		}

		[ qzview setDriver: this ];

		[ qzview setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable ];
		[ window setContentView: qzview ];
		[ qzview release ];
		[ window makeKeyAndOrderFront:nil ];
	}

	ret = WindowResized();

	UpdatePalette(0, 256);

	setup = false;

	return ret;
}

void WindowQuartzSubdriver::BlitIndexedToView32(int left, int top, int right, int bottom)
{
	const uint32* pal   = palette;
	const uint8*  src   = (uint8*)pixel_buffer;
	uint32*       dst   = (uint32*)image_buffer;
	uint          width = window_width;
	uint          pitch = window_width;
	int x;
	int y;

	for (y = top; y < bottom; y++) {
		for (x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}


WindowQuartzSubdriver::WindowQuartzSubdriver(int bpp)
{
	window_width  = 0;
	window_height = 0;
	buffer_depth  = bpp;
	image_buffer  = NULL;
	pixel_buffer  = NULL;
	active        = false;
	setup         = false;

	window = nil;
	qzview = nil;

	cgcontext = NULL;

	num_dirty_rects = MAX_DIRTY_RECTS;
}

WindowQuartzSubdriver::~WindowQuartzSubdriver()
{
	QZ_ShowMouse();

	/* Release window mode resources */
	if (window != nil) [ window close ];

	CGContextRelease(cgcontext);

	free(image_buffer);
	free(pixel_buffer);
}

void WindowQuartzSubdriver::Draw()
{
	int i;
	NSRect dirtyrect;

	/* Check if we need to do anything */
	if (num_dirty_rects == 0 ||
		[ window isMiniaturized ]) {
		return;
	}

	if (num_dirty_rects >= MAX_DIRTY_RECTS) {
		num_dirty_rects = 1;
		dirty_rects[0].left = 0;
		dirty_rects[0].top = 0;
		dirty_rects[0].right = window_width;
		dirty_rects[0].bottom = window_height;
	}

	/* Build the region of dirty rectangles */
	for (i = 0; i < num_dirty_rects; i++) {
		/* We only need to blit in indexed mode since in 32bpp mode the game draws directly to the image. */
		if(buffer_depth == 8) {
			BlitIndexedToView32(
				dirty_rects[i].left,
				dirty_rects[i].top,
				dirty_rects[i].right,
				dirty_rects[i].bottom
			);
		}

		dirtyrect.origin.x = dirty_rects[i].left;
		dirtyrect.origin.y = window_height - dirty_rects[i].bottom;
		dirtyrect.size.width = dirty_rects[i].right - dirty_rects[i].left;
		dirtyrect.size.height = dirty_rects[i].bottom - dirty_rects[i].top;

		/* drawRect will be automatically called by Mac OS X during next update cycle, and then blitting will occur */
		[ qzview setNeedsDisplayInRect: dirtyrect ];
	}

	//DrawResizeIcon();

	num_dirty_rects = 0;
}

void WindowQuartzSubdriver::MakeDirty(int left, int top, int width, int height)
{
	if (num_dirty_rects < MAX_DIRTY_RECTS) {
		dirty_rects[num_dirty_rects].left = left;
		dirty_rects[num_dirty_rects].top = top;
		dirty_rects[num_dirty_rects].right = left + width;
		dirty_rects[num_dirty_rects].bottom = top + height;
	}
	num_dirty_rects++;
}

void WindowQuartzSubdriver::UpdatePalette(uint first_color, uint num_colors)
{
	uint i;

	if (buffer_depth != 8)
		return;

	for (i = first_color; i < first_color + num_colors; i++) {
		uint32 clr = 0xff000000;
		clr |= (uint32)_cur_palette[i].r << 16;
		clr |= (uint32)_cur_palette[i].g << 8;
		clr |= (uint32)_cur_palette[i].b;
		palette[i] = clr;
	}

	num_dirty_rects = MAX_DIRTY_RECTS;
}

uint WindowQuartzSubdriver::ListModes(OTTDPoint* modes, uint max_modes)
{
	if (max_modes == 0) return 0;

	modes[0].x = window_width;
	modes[0].y = window_height;

	return 1;
}

bool WindowQuartzSubdriver::ChangeResolution(int w, int h)
{
	int old_width  = window_width;
	int old_height = window_height;

	if (SetVideoMode(w, h))
		return true;

	if (old_width != 0 && old_height != 0)
		SetVideoMode(old_width, old_height);

	return false;
}

/* Convert local coordinate to window server (CoreGraphics) coordinate */
CGPoint WindowQuartzSubdriver::PrivateLocalToCG(NSPoint* p)
{
	CGPoint cgp;

	p->y = window_height - p->y;
	*p = [ qzview convertPoint:*p toView: nil ];
	*p = [ window convertBaseToScreen:*p ];
	p->y = device_height - p->y;

	cgp.x = p->x;
	cgp.y = p->y;

	return cgp;
}

NSPoint WindowQuartzSubdriver::GetMouseLocation(NSEvent *event)
{
	NSPoint pt;

	pt = [ event locationInWindow ];
	pt = [ qzview convertPoint:pt fromView:nil ];

	pt.y = window_height - pt.y;

	return pt;
}

bool WindowQuartzSubdriver::MouseIsInsideView(NSPoint *pt)
{
	return [ qzview mouse:*pt inRect:[ qzview bounds ] ];
}


/* This function makes the *game region* of the window 100% opaque.
 * The genie effect uses the alpha component. Otherwise,
 * it doesn't seem to matter what value it has.
 */
void WindowQuartzSubdriver::SetPortAlphaOpaque()
{
	uint32* pixels = (uint32*)image_buffer;
	uint32  pitch  = window_width;
	int x, y;

	for (y = 0; y < window_height; y++)
		for (x = 0; x < window_width; x++) {
		pixels[y * pitch + x] |= 0xFF000000;
	}
}

bool WindowQuartzSubdriver::WindowResized()
{
	if (window == nil || qzview == nil) return true;

	NSRect newframe = [ qzview frame ];

	window_width = newframe.size.width;
	window_height = newframe.size.height;

	/* Create Core Graphics Context */
	free(image_buffer);
	image_buffer = (uint32*)malloc(window_width * window_height * 4);

	CGContextRelease(cgcontext);
	cgcontext = CGBitmapContextCreate(
		image_buffer,              // data
		window_width,              // width
		window_height,             // height
		8,                         // bits per component
		window_width * 4,          // bytes per row
		QZ_GetCorrectColorSpace(), // color space
		kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host
	);

	assert(cgcontext != NULL);
	CGContextSetShouldAntialias(cgcontext, FALSE);
	CGContextSetAllowsAntialiasing(cgcontext, FALSE);
	CGContextSetInterpolationQuality(cgcontext, kCGInterpolationNone);

	if (buffer_depth == 8) {
		free(pixel_buffer);
		pixel_buffer = malloc(window_width * window_height);
		if (pixel_buffer == NULL) {
			DEBUG(driver, 0, "Failed to allocate pixel buffer");
			return false;
		}
	}

	QZ_GameSizeChanged();

	/* Redraw screen */
	num_dirty_rects = MAX_DIRTY_RECTS;

	return true;
}


CocoaSubdriver *QZ_CreateWindowQuartzSubdriver(int width, int height, int bpp)
{
	WindowQuartzSubdriver *ret;

	if (bpp != 8 && bpp != 32) {
		DEBUG(driver, 0, "The cocoa quartz subdriver only supports 8 and 32 bpp.");
		return NULL;
	}

	ret = new WindowQuartzSubdriver(bpp);

	if (!ret->ChangeResolution(width, height)) {
		delete ret;
		return NULL;
	}

	return ret;
}

#endif /* WITH_COCOA */
