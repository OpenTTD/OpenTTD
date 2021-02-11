/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cocoa_v.mm Code related to the cocoa video driver(s). */

/******************************************************************************
 *                             Cocoa video driver                             *
 * Known things left to do:                                                   *
 *  Nothing at the moment.                                                    *
 ******************************************************************************/

#ifdef WITH_COCOA

#include "../../stdafx.h"
#include "../../os/macosx/macos.h"

#define Rect  OTTDRect
#define Point OTTDPoint
#import <Cocoa/Cocoa.h>
#undef Rect
#undef Point

#include "../../openttd.h"
#include "../../debug.h"
#include "../../rev.h"
#include "../../core/geometry_type.hpp"
#include "cocoa_v.h"
#include "cocoa_wnd.h"
#include "../../blitter/factory.hpp"
#include "../../gfx_func.h"
#include "../../window_func.h"
#include "../../window_gui.h"
#include "../../core/math_func.hpp"
#include "../../framerate_type.h"

#import <sys/param.h> /* for MAXPATHLEN */

/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */

/* On some old versions of MAC OS this may not be defined.
 * Those versions generally only produce code for PPC. So it should be safe to
 * set this to 0. */
#ifndef kCGBitmapByteOrder32Host
#define kCGBitmapByteOrder32Host 0
#endif

bool _cocoa_video_started = false;


/** List of common display/window sizes. */
static const Dimension _default_resolutions[] = {
	{  640,  480 },
	{  800,  600 },
	{ 1024,  768 },
	{ 1152,  864 },
	{ 1280,  800 },
	{ 1280,  960 },
	{ 1280, 1024 },
	{ 1400, 1050 },
	{ 1600, 1200 },
	{ 1680, 1050 },
	{ 1920, 1200 },
	{ 2560, 1440 }
};

static FVideoDriver_Cocoa iFVideoDriver_Cocoa;


VideoDriver_Cocoa::VideoDriver_Cocoa()
{
	this->window_width  = 0;
	this->window_height = 0;
	this->buffer_depth  = 0;
	this->window_buffer = nullptr;
	this->pixel_buffer  = nullptr;
	this->active        = false;
	this->setup         = false;

	this->window    = nil;
	this->cocoaview = nil;

	this->color_space = nullptr;
	this->cgcontext   = nullptr;

	this->num_dirty_rects = lengthof(this->dirty_rects);
}

/**
 * Stop the cocoa video subdriver.
 */
void VideoDriver_Cocoa::Stop()
{
	if (!_cocoa_video_started) return;

	CocoaExitApplication();

	/* Release window mode resources */
	if (this->window != nil) [ this->window close ];

	CGContextRelease(this->cgcontext);

	CGColorSpaceRelease(this->color_space);
	free(this->window_buffer);
	free(this->pixel_buffer);

	_cocoa_video_started = false;
}

/**
 * Initialize a cocoa video subdriver.
 */
const char *VideoDriver_Cocoa::Start(const StringList &parm)
{
	if (!MacOSVersionIsAtLeast(10, 7, 0)) return "The Cocoa video driver requires Mac OS X 10.7 or later.";

	if (_cocoa_video_started) return "Already started";
	_cocoa_video_started = true;

	/* Don't create a window or enter fullscreen if we're just going to show a dialog. */
	if (!CocoaSetupApplication()) return nullptr;

	this->UpdateAutoResolution();

	this->orig_res = _cur_resolution;
	int width  = _cur_resolution.width;
	int height = _cur_resolution.height;
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	if (bpp != 8 && bpp != 32) {
		Stop();
		return "The cocoa quartz subdriver only supports 8 and 32 bpp.";
	}

	if (!this->ChangeResolution(width, height, bpp)) {
		Stop();
		return "Could not create subdriver";
	}

	if (_fullscreen) this->ToggleFullscreen(_fullscreen);

	this->GameSizeChanged();
	this->UpdateVideoModes();

	return nullptr;
}

/**
 * Set dirty a rectangle managed by a cocoa video subdriver.
 *
 * @param left Left x cooordinate of the dirty rectangle.
 * @param top Uppder y coordinate of the dirty rectangle.
 * @param width Width of the dirty rectangle.
 * @param height Height of the dirty rectangle.
 */
void VideoDriver_Cocoa::MakeDirty(int left, int top, int width, int height)
{
	if (this->num_dirty_rects < lengthof(this->dirty_rects)) {
		dirty_rects[this->num_dirty_rects].left = left;
		dirty_rects[this->num_dirty_rects].top = top;
		dirty_rects[this->num_dirty_rects].right = left + width;
		dirty_rects[this->num_dirty_rects].bottom = top + height;
	}
	this->num_dirty_rects++;
}

/**
 * Start the main programme loop when using a cocoa video driver.
 */
void VideoDriver_Cocoa::MainLoop()
{
	/* Restart game loop if it was already running (e.g. after bootstrapping),
	 * otherwise this call is a no-op. */
	[ [ NSNotificationCenter defaultCenter ] postNotificationName:OTTDMainLaunchGameEngine object:nil ];

	/* Start the main event loop. */
	[ NSApp run ];
}

/**
 * Change the resolution when using a cocoa video driver.
 *
 * @param w New window width.
 * @param h New window height.
 * @return Whether the video driver was successfully updated.
 */
bool VideoDriver_Cocoa::ChangeResolution(int w, int h)
{
	bool ret = this->ChangeResolution(w, h, BlitterFactory::GetCurrentBlitter()->GetScreenDepth());
	this->GameSizeChanged();
	return ret;
}

/**
 * Toggle between windowed and full screen mode for cocoa display driver.
 *
 * @param full_screen Whether to switch to full screen or not.
 * @return Whether the mode switch was successful.
 */
bool VideoDriver_Cocoa::ToggleFullscreen(bool full_screen)
{
	if (this->IsFullscreen() == full_screen) return true;

	if ([ this->window respondsToSelector:@selector(toggleFullScreen:) ]) {
		[ this->window performSelector:@selector(toggleFullScreen:) withObject:this->window ];
		this->UpdateVideoModes();
		return true;
	}

	return false;
}

/**
 * Callback invoked after the blitter was changed.
 *
 * @return True if no error.
 */
bool VideoDriver_Cocoa::AfterBlitterChange()
{
	return this->ChangeResolution(_screen.width, _screen.height);
}

/**
 * An edit box lost the input focus. Abort character compositing if necessary.
 */
void VideoDriver_Cocoa::EditBoxLostFocus()
{
	[ [ this->cocoaview inputContext ] discardMarkedText ];
	/* Clear any marked string from the current edit box. */
	HandleTextInput(NULL, true);
}

/**
 * Get the resolution of the main screen.
 */
Dimension VideoDriver_Cocoa::GetScreenSize() const
{
	NSRect frame = [ [ NSScreen mainScreen ] frame ];
	return { static_cast<uint>(NSWidth(frame)), static_cast<uint>(NSHeight(frame)) };
}

/**
 * Handle a change of the display area.
 */
void VideoDriver_Cocoa::GameSizeChanged()
{
	/* Tell the game that the resolution has changed */
	_screen.width = this->window_width;
	_screen.height = this->window_height;
	_screen.pitch = this->window_width;
	_screen.dst_ptr = this->GetPixelBuffer();

	/* Store old window size if we entered fullscreen mode. */
	bool fullscreen = this->IsFullscreen();
	if (fullscreen && !_fullscreen) this->orig_res = _cur_resolution;
	_fullscreen = fullscreen;

	BlitterFactory::GetCurrentBlitter()->PostResize();

	::GameSizeChanged();
}

/* Subclass of OTTD_CocoaView to fix Quartz rendering */
@interface OTTD_QuartzView : OTTD_CocoaView
- (void)setDriver:(VideoDriver_Cocoa *)drv;
- (void)drawRect:(NSRect)invalidRect;
@end


@implementation OTTD_QuartzView

- (void)setDriver:(VideoDriver_Cocoa *)drv
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


/**
 * Update the video modus.
 */
void VideoDriver_Cocoa::UpdateVideoModes()
{
	_resolutions.clear();

	if (this->IsFullscreen()) {
		/* Full screen, there is only one possible resolution. */
		NSSize screen = [ [ this->window screen ] frame ].size;
		_resolutions.emplace_back((uint)screen.width, (uint)screen.height);
	} else {
		/* Windowed; offer a selection of common window sizes up until the
		 * maximum usable screen space. This excludes the menu and dock areas. */
		NSSize maxSize = [ [ NSScreen mainScreen] visibleFrame ].size;
		for (const auto &d : _default_resolutions) {
			if (d.width < maxSize.width && d.height < maxSize.height) _resolutions.push_back(d);
		}
		_resolutions.emplace_back((uint)maxSize.width, (uint)maxSize.height);
	}
}

void VideoDriver_Cocoa::GetDeviceInfo()
{
	/* Initialize the video settings; this data persists between mode switches
	 * and gather some information that is useful to know about the display */

	/* Use the new API when compiling for OSX 10.6 or later */
	CGDisplayModeRef cur_mode = CGDisplayCopyDisplayMode(kCGDirectMainDisplay);
	if (cur_mode == NULL) { return; }

	this->device_width = CGDisplayModeGetWidth(cur_mode);
	this->device_height = CGDisplayModeGetHeight(cur_mode);

	CGDisplayModeRelease(cur_mode);
}

bool VideoDriver_Cocoa::IsFullscreen()
{
	return this->window != nil && ([ this->window styleMask ] & NSWindowStyleMaskFullScreen) != 0;
}

bool VideoDriver_Cocoa::SetVideoMode(int width, int height, int bpp)
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

		/* Add built in full-screen support when available (OS X 10.7 and higher)
		 * This code actually compiles for 10.5 and later, but only makes sense in conjunction
		 * with the quartz fullscreen support as found only in 10.7 and later
		 */
		if ([ this->window respondsToSelector:@selector(toggleFullScreen:) ]) {
			NSWindowCollectionBehavior behavior = [ this->window collectionBehavior ];
			behavior |= NSWindowCollectionBehaviorFullScreenPrimary;
			[ this->window setCollectionBehavior:behavior ];

			NSButton* fullscreenButton = [ this->window standardWindowButton:NSWindowFullScreenButton ];
			[ fullscreenButton setAction:@selector(toggleFullScreen:) ];
			[ fullscreenButton setTarget:this->window ];
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
		[ this->window setDelegate:[ delegate autorelease ] ];
	} else {
		/* We already have a window, just change its size */
		[ this->window setContentSize:contentRect.size ];

		/* Ensure frame height - title bar height >= view height */
		float content_height = [ this->window contentRectForFrameRect:[ this->window frame ] ].size.height;
		contentRect.size.height = Clamp(height, 0, (int)content_height);

		if (this->cocoaview != nil) {
			height = (int)contentRect.size.height;
			[ this->cocoaview setFrameSize:contentRect.size ];
		}
	}

	this->window_width = width;
	this->window_height = height;
	this->buffer_depth = bpp;

	[ (OTTD_CocoaWindow *)this->window center ];

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

	[ this->window setColorSpace:[ NSColorSpace sRGBColorSpace ] ];
	this->color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	if (this->color_space == nullptr) this->color_space = CGColorSpaceCreateDeviceRGB();
	if (this->color_space == nullptr) error("Could not get a valid colour space for drawing.");

	bool ret = WindowResized();
	this->UpdatePalette(0, 256);

	this->setup = false;

	return ret;
}

void VideoDriver_Cocoa::BlitIndexedToView32(int left, int top, int right, int bottom)
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


void VideoDriver_Cocoa::Draw(bool force_update)
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	/* Check if we need to do anything */
	if (this->num_dirty_rects == 0 || [ this->window isMiniaturized ]) return;

	if (this->num_dirty_rects >= lengthof(this->dirty_rects)) {
		this->num_dirty_rects = 1;
		this->dirty_rects[0].left = 0;
		this->dirty_rects[0].top = 0;
		this->dirty_rects[0].right = this->window_width;
		this->dirty_rects[0].bottom = this->window_height;
	}

	/* Build the region of dirty rectangles */
	for (uint i = 0; i < this->num_dirty_rects; i++) {
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

void VideoDriver_Cocoa::UpdatePalette(uint first_color, uint num_colors)
{
	if (this->buffer_depth != 8) return;

	for (uint i = first_color; i < first_color + num_colors; i++) {
		uint32 clr = 0xff000000;
		clr |= (uint32)_cur_palette.palette[i].r << 16;
		clr |= (uint32)_cur_palette.palette[i].g << 8;
		clr |= (uint32)_cur_palette.palette[i].b;
		this->palette[i] = clr;
	}

	this->num_dirty_rects = lengthof(this->dirty_rects);
}

bool VideoDriver_Cocoa::ChangeResolution(int w, int h, int bpp)
{
	int old_width  = this->window_width;
	int old_height = this->window_height;
	int old_bpp    = this->buffer_depth;

	if (this->SetVideoMode(w, h, bpp)) return true;
	if (old_width != 0 && old_height != 0) this->SetVideoMode(old_width, old_height, old_bpp);

	return false;
}

/* Convert local coordinate to window server (CoreGraphics) coordinate */
CGPoint VideoDriver_Cocoa::PrivateLocalToCG(NSPoint *p)
{

	p->y = this->window_height - p->y;
	*p = [ this->cocoaview convertPoint:*p toView:nil ];
	*p = [ this->window convertRectToScreen:NSMakeRect(p->x, p->y, 0, 0) ].origin;

	p->y = this->device_height - p->y;

	CGPoint cgp;
	cgp.x = p->x;
	cgp.y = p->y;

	return cgp;
}

NSPoint VideoDriver_Cocoa::GetMouseLocation(NSEvent *event)
{
	NSPoint pt;

	if ( [ event window ] == nil) {
		pt = [ this->cocoaview convertPoint:[ [ this->cocoaview window ] convertRectFromScreen:NSMakeRect([ event locationInWindow ].x, [ event locationInWindow ].y, 0, 0) ].origin fromView:nil ];
	} else {
		pt = [ event locationInWindow ];
	}

	pt.y = this->window_height - pt.y;

	return pt;
}

bool VideoDriver_Cocoa::MouseIsInsideView(NSPoint *pt)
{
	return [ cocoaview mouse:*pt inRect:[ this->cocoaview bounds ] ];
}

/** Clear buffer to opaque black. */
static void ClearWindowBuffer(uint32 *buffer, uint32 pitch, uint32 height)
{
	uint32 fill = Colour(0, 0, 0).data;
	for (uint32 y = 0; y < height; y++) {
		for (uint32 x = 0; x < pitch; x++) {
			buffer[y * pitch + x] = fill;
		}
	}
}

bool VideoDriver_Cocoa::WindowResized()
{
	if (this->window == nil || this->cocoaview == nil) return true;

	NSRect newframe = [ this->cocoaview frame ];

	this->window_width = (int)newframe.size.width;
	this->window_height = (int)newframe.size.height;

	/* Create Core Graphics Context */
	free(this->window_buffer);
	this->window_buffer = malloc(this->window_width * this->window_height * sizeof(uint32));
	/* Initialize with opaque black. */
	ClearWindowBuffer((uint32 *)this->window_buffer, this->window_width, this->window_height);

	CGContextRelease(this->cgcontext);
	this->cgcontext = CGBitmapContextCreate(
		this->window_buffer,       // data
		this->window_width,        // width
		this->window_height,       // height
		8,                         // bits per component
		this->window_width * 4,    // bytes per row
		this->color_space,         // color space
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

	this->GameSizeChanged();

	/* Redraw screen */
	this->num_dirty_rects = lengthof(this->dirty_rects);

	return true;
}

void VideoDriver_Cocoa::CheckPaletteAnim()
{
	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
				this->UpdatePalette(_cur_palette.first_dirty, _cur_palette.count_dirty);
				break;

			case Blitter::PALETTE_ANIMATION_BLITTER:
				blitter->PaletteAnimate(_cur_palette);
				break;

			case Blitter::PALETTE_ANIMATION_NONE:
				break;

			default:
				NOT_REACHED();
		}
		_cur_palette.count_dirty = 0;
	}
}

#endif /* WITH_COCOA */
