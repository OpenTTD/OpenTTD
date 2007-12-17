/* $Id$ */

/******************************************************************************
 *                             Cocoa video driver                             *
 * Known things left to do:                                                   *
 *  List available resolutions.                                               *
 ******************************************************************************/

#ifdef WITH_COCOA
#ifdef ENABLE_COCOA_QUICKDRAW

#define MAC_OS_X_VERSION_MIN_REQUIRED    MAC_OS_X_VERSION_10_3
#define MAC_OS_X_VERSION_MAX_ALLOWED     MAC_OS_X_VERSION_10_3
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
#include "../../debug.h"
#include "../../variables.h"
#include "cocoa_v.h"

#undef Point
#undef Rect


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

	virtual void Draw();
	virtual void MakeDirty(int left, int top, int width, int height);
	virtual void UpdatePalette(uint first_color, uint num_colors);

	virtual uint ListModes(OTTDPoint* modes, uint max_modes);

	virtual bool ChangeResolution(int w, int h);

	virtual bool IsFullscreen() { return false; }

	virtual int GetWidth() { return window_width; }
	virtual int GetHeight() { return window_height; }
	virtual void *GetPixelBuffer() { return pixel_buffer; }

	/* Convert local coordinate to window server (CoreGraphics) coordinate */
	virtual CGPoint PrivateLocalToCG(NSPoint* p);

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



extern const char _openttd_revision[];


void WindowQuickdrawSubdriver::GetDeviceInfo()
{
	CFDictionaryRef    cur_mode;

	/* Initialize the video settings; this data persists between mode switches */
	cur_mode = CGDisplayCurrentMode(kCGDirectMainDisplay);

	/* Gather some information that is useful to know about the display */
	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayBitsPerPixel),
		kCFNumberSInt32Type, &device_depth
	);

	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayWidth),
		kCFNumberSInt32Type, &device_width
	);

	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayHeight),
		kCFNumberSInt32Type, &device_height
	);
}

bool WindowQuickdrawSubdriver::SetVideoMode(int width, int height)
{
	char caption[50];
	NSString *nsscaption;
	unsigned int style;
	NSRect contentRect;
	BOOL isCustom = NO;
	bool ret;

	setup = true;

	GetDeviceInfo();

	if (buffer_depth > device_depth) {
		DEBUG(driver, 0, "Cannot use a blitter with a higer screen depth than the display when running in windowed mode.");
		setup = false;
		return false;
	}

	if (width > device_width)
		width = device_width;
	if (height > device_height)
		height = device_height;

	contentRect = NSMakeRect(0, 0, width, height);

	/* Check if we should recreate the window */
	if (window == nil) {
		OTTD_QuickdrawWindowDelegate *delegate;

		/* Set the window style */
		style = NSTitledWindowMask;
		style |= (NSMiniaturizableWindowMask | NSClosableWindowMask);
		style |= NSResizableWindowMask;

		/* Manually create a window, avoids having a nib file resource */
		window = [ [ OTTD_QuickdrawWindow alloc ]
						initWithContentRect:contentRect
						styleMask:style
						backing:NSBackingStoreBuffered
						defer:NO ];

		if (window == nil) {
			DEBUG(driver, 0, "Could not create the Cocoa window.");
			setup = false;
			return false;
		}

		[ window setDriver:this ];

		snprintf(caption, sizeof(caption), "OpenTTD %s", _openttd_revision);
		nsscaption = [ [ NSString alloc ] initWithCString:caption ];
		[ window setTitle:nsscaption ];
		[ window setMiniwindowTitle:nsscaption ];
		[ nsscaption release ];

		[ window setAcceptsMouseMovedEvents:YES ];
		[ window setViewsNeedDisplay:NO ];

		delegate = [ [ OTTD_QuickdrawWindowDelegate alloc ] init ];
		[ delegate setDriver:this ];
		[ window setDelegate: [ delegate autorelease ] ];
	} else {
		/* We already have a window, just change its size */
		if (!isCustom) {
			[ window setContentSize:contentRect.size ];
			// Ensure frame height - title bar height >= view height
			contentRect.size.height = Clamp(height, 0, [ window frame ].size.height - 22 /* 22 is the height of title bar of window*/);
			height = contentRect.size.height;
			[ qdview setFrameSize:contentRect.size ];
		}
	}

	// Update again
	window_width = width;
	window_height = height;

	[ window center ];

	/* Only recreate the view if it doesn't already exist */
	if (qdview == nil) {
		qdview = [ [ NSQuickDrawView alloc ] initWithFrame:contentRect ];
		if (qdview == nil) {
			DEBUG(driver, 0, "Could not create the Quickdraw view.");
			setup = false;
			return false;
		}

		[ qdview setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable ];
		[ [ window contentView ] addSubview:qdview ];
		[ qdview release ];
		[ window makeKeyAndOrderFront:nil ];
	}

	ret = WindowResized();

	UpdatePalette(0, 256);

	setup = false;

	return ret;
}

void WindowQuickdrawSubdriver::Blit32ToView32(int left, int top, int right, int bottom)
{
	const uint32* src   = (uint32*)pixel_buffer;
	uint32*       dst   = (uint32*)window_buffer;
	uint          width = window_width;
	uint          pitch = window_pitch / 4;
	int y;


	dst += top * pitch + left;
	src += top * width + left;

	for (y = top; y < bottom; y++, dst+= pitch, src+= width) {
		memcpy(dst, src, (right - left) * 4);
	}
}

void WindowQuickdrawSubdriver::BlitIndexedToView32(int left, int top, int right, int bottom)
{
	const uint32* pal   = palette32;
	const uint8*  src   = (uint8*)pixel_buffer;
	uint32*       dst   = (uint32*)window_buffer;
	uint          width = window_width;
	uint          pitch = window_pitch / 4;
	int x;
	int y;

	for (y = top; y < bottom; y++) {
		for (x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}

void WindowQuickdrawSubdriver::BlitIndexedToView16(int left, int top, int right, int bottom)
{
	const uint16* pal   = palette16;
	const uint8*  src   = (uint8*)pixel_buffer;
	uint16*       dst   = (uint16*)window_buffer;
	uint          width = window_width;
	uint          pitch = window_pitch / 2;
	int x;
	int y;

	for (y = top; y < bottom; y++) {
		for (x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}


inline void WindowQuickdrawSubdriver::BlitToView(int left, int top, int right, int bottom)
{
	switch (device_depth) {
		case 32:
			switch(buffer_depth) {
				case 32:
					Blit32ToView32(left, top, right, bottom);
					break;
				case 8:
					BlitIndexedToView32(left, top, right, bottom);
					break;
			}
			break;
		case 16:
			BlitIndexedToView16(left, top, right, bottom);
			break;
	}
}

void WindowQuickdrawSubdriver::DrawResizeIcon()
{
	int xoff = window_width - _resize_icon_width;
	int yoff = window_height - _resize_icon_height;
	int x;
	int y;

	switch (device_depth) {
		case 32:
			for (y = 0; y < _resize_icon_height; y++) {
				uint32* trg = (uint32*)window_buffer + (yoff + y) * window_pitch / 4 + xoff;

				for (x = 0; x < _resize_icon_width; x++, trg++) {
					if (_resize_icon[y * _resize_icon_width + x]) *trg = 0xff000000;
				}
			}
			break;
		case 16:
			for (y = 0; y < _resize_icon_height; y++) {
				uint16* trg = (uint16*)window_buffer + (yoff + y) * window_pitch / 2 + xoff;

				for (x = 0; x < _resize_icon_width; x++, trg++) {
					if (_resize_icon[y * _resize_icon_width + x]) *trg = 0x0000;
				}
			}
			break;
	}
}


WindowQuickdrawSubdriver::WindowQuickdrawSubdriver(int bpp)
{
	window_width  = 0;
	window_height = 0;
	buffer_depth  = bpp;
	pixel_buffer  = NULL;
	active        = false;
	setup         = false;

	window = nil;
	qdview = nil;

	num_dirty_rects = MAX_DIRTY_RECTS;
}

WindowQuickdrawSubdriver::~WindowQuickdrawSubdriver()
{
	QZ_ShowMouse();

	/* Release window mode resources */
	if (window != nil) [ window close ];

	free(pixel_buffer);
}

void WindowQuickdrawSubdriver::Draw()
{
	int i;
	RgnHandle dirty, temp;

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

	dirty = NewRgn();
	temp  = NewRgn();

	SetEmptyRgn(dirty);

	/* Build the region of dirty rectangles */
	for (i = 0; i < num_dirty_rects; i++) {
		BlitToView(
			dirty_rects[i].left,
			dirty_rects[i].top,
			dirty_rects[i].right,
			dirty_rects[i].bottom
		);

		MacSetRectRgn(
			temp,
			dirty_rects[i].left,
			dirty_rects[i].top,
			dirty_rects[i].right,
			dirty_rects[i].bottom
		);
		MacUnionRgn(dirty, temp, dirty);
	}

	DrawResizeIcon();

	/* Flush the dirty region */
	QDFlushPortBuffer( (OpaqueGrafPtr*) [ qdview qdPort ], dirty);
	DisposeRgn(dirty);
	DisposeRgn(temp);

	num_dirty_rects = 0;
}

void WindowQuickdrawSubdriver::MakeDirty(int left, int top, int width, int height)
{
	if (num_dirty_rects < MAX_DIRTY_RECTS) {
		dirty_rects[num_dirty_rects].left = left;
		dirty_rects[num_dirty_rects].top = top;
		dirty_rects[num_dirty_rects].right = left + width;
		dirty_rects[num_dirty_rects].bottom = top + height;
	}
	num_dirty_rects++;
}

void WindowQuickdrawSubdriver::UpdatePalette(uint first_color, uint num_colors)
{
	uint i;

	if (buffer_depth != 8)
		return;

	switch (device_depth) {
		case 32:
			for (i = first_color; i < first_color + num_colors; i++) {
				uint32 clr32 = 0xff000000;
				clr32 |= (uint32)_cur_palette[i].r << 16;
				clr32 |= (uint32)_cur_palette[i].g << 8;
				clr32 |= (uint32)_cur_palette[i].b;
				palette32[i] = clr32;
			}
			break;
		case 16:
			for (i = first_color; i < first_color + num_colors; i++) {
				uint16 clr16 = 0x0000;
				clr16 |= (uint16)((_cur_palette[i].r >> 3) & 0x1f) << 10;
				clr16 |= (uint16)((_cur_palette[i].g >> 3) & 0x1f) << 5;
				clr16 |= (uint16)((_cur_palette[i].b >> 3) & 0x1f);
				palette16[i] = clr16;
			}
			break;
	}

	num_dirty_rects = MAX_DIRTY_RECTS;
}

uint WindowQuickdrawSubdriver::ListModes(OTTDPoint* modes, uint max_modes)
{
	if (max_modes == 0) return 0;

	modes[0].x = window_width;
	modes[0].y = window_height;

	return 1;
}

bool WindowQuickdrawSubdriver::ChangeResolution(int w, int h)
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
CGPoint WindowQuickdrawSubdriver::PrivateLocalToCG(NSPoint* p)
{
	CGPoint cgp;

	*p = [ qdview convertPoint:*p toView: nil ];
	*p = [ window convertBaseToScreen:*p ];
	p->y = device_height - p->y;

	cgp.x = p->x;
	cgp.y = p->y;

	return cgp;
}

NSPoint WindowQuickdrawSubdriver::GetMouseLocation(NSEvent *event)
{
	NSPoint pt;

	pt = [ event locationInWindow ];
	pt = [ qdview convertPoint:pt fromView:nil ];

	return pt;
}

bool WindowQuickdrawSubdriver::MouseIsInsideView(NSPoint *pt)
{
	return [ qdview mouse:*pt inRect:[ qdview bounds ] ];
}


/* This function makes the *game region* of the window 100% opaque.
 * The genie effect uses the alpha component. Otherwise,
 * it doesn't seem to matter what value it has.
 */
void WindowQuickdrawSubdriver::SetPortAlphaOpaque()
{
	if (device_depth != 32)
		return;

	uint32* pixels = (uint32*)window_buffer;
	uint32  pitch  = window_pitch / 4;
	int x, y;

	for (y = 0; y < window_height; y++)
		for (x = 0; x < window_width; x++) {
		pixels[y * pitch + x] |= 0xFF000000;
	}
}

bool WindowQuickdrawSubdriver::WindowResized()
{
	if (window == nil || qdview == nil) return true;

	NSRect   newframe = [ qdview frame ];
	CGrafPtr thePort  = (OpaqueGrafPtr*) [ qdview qdPort ];
	int voff, hoff;

	LockPortBits(thePort);
	window_buffer = GetPixBaseAddr(GetPortPixMap(thePort));
	window_pitch = GetPixRowBytes(GetPortPixMap(thePort));
	UnlockPortBits(thePort);

	/* _cocoa_video_data.realpixels now points to the window's pixels
	 * We want it to point to the *view's* pixels
	 */
	voff = [ window frame ].size.height - newframe.size.height - newframe.origin.y;
	hoff = [ qdview frame ].origin.x;
	window_buffer = (uint8*)window_buffer + (voff * window_pitch) + hoff * (device_depth / 8);

	window_width = newframe.size.width;
	window_height = newframe.size.height;

	free(pixel_buffer);
	pixel_buffer = malloc(window_width * window_height * buffer_depth / 8);
	if (pixel_buffer == NULL) {
		DEBUG(driver, 0, "Failed to allocate pixel buffer");
		return false;
	}

	QZ_GameSizeChanged();

	/* Redraw screen */
	num_dirty_rects = MAX_DIRTY_RECTS;

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
