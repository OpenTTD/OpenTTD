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
#import <QuartzCore/QuartzCore.h>
#undef Rect
#undef Point

#include "../../openttd.h"
#include "../../debug.h"
#include "../../error_func.h"
#include "../../core/geometry_func.hpp"
#include "../../core/math_func.hpp"
#include "cocoa_v.h"
#include "cocoa_wnd.h"
#include "../../blitter/factory.hpp"
#include "../../framerate_type.h"
#include "../../gfx_func.h"
#include "../../thread.h"
#include "../../core/random_func.hpp"
#include "../../progress.h"
#include "../../settings_type.h"
#include "../../window_func.h"
#include "../../window_gui.h"

#import <sys/param.h> /* for MAXPATHLEN */
#import <sys/time.h> /* gettimeofday */

/* The 10.12 SDK added new names for some enum constants and
 * deprecated the old ones. As there's no functional change in any
 * way, just use a define for older SDKs to the old names. */
#ifndef HAVE_OSX_1012_SDK
#	define NSEventModifierFlagCommand NSCommandKeyMask
#	define NSEventModifierFlagControl NSControlKeyMask
#	define NSEventModifierFlagOption NSAlternateKeyMask
#	define NSEventModifierFlagShift NSShiftKeyMask
#	define NSEventModifierFlagCapsLock NSAlphaShiftKeyMask
#endif

/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */

bool _cocoa_video_started = false;
static Palette _local_palette; ///< Current palette to use for drawing.

extern bool _tab_is_down;


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


VideoDriver_Cocoa::VideoDriver_Cocoa()
{
	this->setup         = false;
	this->buffer_locked = false;

	this->refresh_sys_sprites = true;

	this->window    = nil;
	this->cocoaview = nil;
	this->delegate  = nil;

	this->color_space = nullptr;

	this->dirty_rect = {};
}

/** Stop Cocoa video driver. */
void VideoDriver_Cocoa::Stop()
{
	if (!_cocoa_video_started) return;

	CocoaExitApplication();

	/* Release window mode resources */
	if (this->window != nil) [ this->window close ];
	[ this->cocoaview release ];
	[ this->delegate release ];

	CGColorSpaceRelease(this->color_space);

	_cocoa_video_started = false;
}

/** Common driver initialization. */
const char *VideoDriver_Cocoa::Initialize()
{
	if (!MacOSVersionIsAtLeast(10, 7, 0)) return "The Cocoa video driver requires Mac OS X 10.7 or later.";

	if (_cocoa_video_started) return "Already started";
	_cocoa_video_started = true;

	/* Don't create a window or enter fullscreen if we're just going to show a dialog. */
	if (!CocoaSetupApplication()) return nullptr;

	this->UpdateAutoResolution();
	this->orig_res = _cur_resolution;

	return nullptr;
}

/**
 * Set dirty a rectangle managed by a cocoa video subdriver.
 * @param left Left x cooordinate of the dirty rectangle.
 * @param top Uppder y coordinate of the dirty rectangle.
 * @param width Width of the dirty rectangle.
 * @param height Height of the dirty rectangle.
 */
void VideoDriver_Cocoa::MakeDirty(int left, int top, int width, int height)
{
	Rect r = {left, top, left + width, top + height};
	this->dirty_rect = BoundingRect(this->dirty_rect, r);
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
 * @param w New window width.
 * @param h New window height.
 * @return Whether the video driver was successfully updated.
 */
bool VideoDriver_Cocoa::ChangeResolution(int w, int h)
{
	NSSize screen_size = [ [ NSScreen mainScreen ] frame ].size;
	w = std::min(w, (int)screen_size.width);
	h = std::min(h, (int)screen_size.height);

	NSRect contentRect = NSMakeRect(0, 0, w, h);
	[ this->window setContentSize:contentRect.size ];

	/* Ensure frame height - title bar height >= view height */
	float content_height = [ this->window contentRectForFrameRect:[ this->window frame ] ].size.height;
	contentRect.size.height = Clamp(h, 0, (int)content_height);

	if (this->cocoaview != nil) {
		h = (int)contentRect.size.height;
		[ this->cocoaview setFrameSize:contentRect.size ];
	}

	[ (OTTD_CocoaWindow *)this->window center ];
	this->AllocateBackingStore();

	return true;
}

/**
 * Toggle between windowed and full screen mode for cocoa display driver.
 * @param full_screen Whether to switch to full screen or not.
 * @return Whether the mode switch was successful.
 */
bool VideoDriver_Cocoa::ToggleFullscreen(bool full_screen)
{
	if (this->IsFullscreen() == full_screen) return true;

	if ([ this->window respondsToSelector:@selector(toggleFullScreen:) ]) {
		[ this->window performSelector:@selector(toggleFullScreen:) withObject:this->window ];

		/* Hide the menu bar and the dock */
		[ NSMenu setMenuBarVisible:!full_screen ];

		this->UpdateVideoModes();
		InvalidateWindowClassesData(WC_GAME_OPTIONS, 3);
		return true;
	}

	return false;
}

void VideoDriver_Cocoa::ClearSystemSprites()
{
	this->refresh_sys_sprites = true;
}

void VideoDriver_Cocoa::PopulateSystemSprites()
{
	if (this->refresh_sys_sprites && this->window != nil) {
		[ this->window refreshSystemSprites ];
		this->refresh_sys_sprites = false;
	}
}

/**
 * Callback invoked after the blitter was changed.
 * @return True if no error.
 */
bool VideoDriver_Cocoa::AfterBlitterChange()
{
	this->AllocateBackingStore(true);
	return true;
}

/**
 * An edit box lost the input focus. Abort character compositing if necessary.
 */
void VideoDriver_Cocoa::EditBoxLostFocus()
{
	[ [ this->cocoaview inputContext ] performSelectorOnMainThread:@selector(discardMarkedText) withObject:nil waitUntilDone:[ NSThread isMainThread ] ];
	/* Clear any marked string from the current edit box. */
	HandleTextInput(nullptr, true);
}

/**
 * Get refresh rates of all connected monitors.
 */
std::vector<int> VideoDriver_Cocoa::GetListOfMonitorRefreshRates()
{
	std::vector<int> rates{};

	if (MacOSVersionIsAtLeast(10, 6, 0)) {
		std::array<CGDirectDisplayID, 16> displays;

		uint32_t count = 0;
		CGGetActiveDisplayList(displays.size(), displays.data(), &count);

		for (uint32_t i = 0; i < count; i++) {
			CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displays[i]);
			int rate = (int)CGDisplayModeGetRefreshRate(mode);
			if (rate > 0) rates.push_back(rate);
			CGDisplayModeRelease(mode);
		}
	}

	return rates;
}

/**
 * Get the resolution of the main screen.
 */
Dimension VideoDriver_Cocoa::GetScreenSize() const
{
	NSRect frame = [ [ NSScreen mainScreen ] frame ];
	return { static_cast<uint>(NSWidth(frame)), static_cast<uint>(NSHeight(frame)) };
}

/** Get DPI scale of our window. */
float VideoDriver_Cocoa::GetDPIScale()
{
	return this->cocoaview != nil ? [ this->cocoaview getContentsScale ] : 1.0f;
}

/** Lock video buffer for drawing if it isn't already mapped. */
bool VideoDriver_Cocoa::LockVideoBuffer()
{
	if (this->buffer_locked) return false;
	this->buffer_locked = true;

	_screen.dst_ptr = this->GetVideoPointer();
	assert(_screen.dst_ptr != nullptr);

	return true;
}

/** Unlock video buffer. */
void VideoDriver_Cocoa::UnlockVideoBuffer()
{
	if (_screen.dst_ptr != nullptr) {
		/* Hand video buffer back to the drawing backend. */
		this->ReleaseVideoPointer();
		_screen.dst_ptr = nullptr;
	}

	this->buffer_locked = false;
}

/**
 * Are we in fullscreen mode?
 * @return whether fullscreen mode is currently used
 */
bool VideoDriver_Cocoa::IsFullscreen()
{
	return this->window != nil && ([ this->window styleMask ] & NSWindowStyleMaskFullScreen) != 0;
}

/**
 * Handle a change of the display area.
 */
void VideoDriver_Cocoa::GameSizeChanged()
{
	/* Store old window size if we entered fullscreen mode. */
	bool fullscreen = this->IsFullscreen();
	if (fullscreen && !_fullscreen) this->orig_res = _cur_resolution;
	_fullscreen = fullscreen;

	BlitterFactory::GetCurrentBlitter()->PostResize();

	::GameSizeChanged();

	/* We need to store the window size as non-Retina size in
	 * the config file to get same windows size on next start. */
	_cur_resolution.width = [ this->cocoaview frame ].size.width;
	_cur_resolution.height = [ this->cocoaview frame ].size.height;
}

/**
 * Update the video mode.
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

/**
 * Build window and view with a given size.
 * @param width Window width.
 * @param height Window height.
 */
bool VideoDriver_Cocoa::MakeWindow(int width, int height)
{
	this->setup = true;

	/* Limit window size to screen frame. */
	NSSize screen_size = [ [ NSScreen mainScreen ] frame ].size;
	if (width > screen_size.width) width = screen_size.width;
	if (height > screen_size.height) height = screen_size.height;

	NSRect contentRect = NSMakeRect(0, 0, width, height);

	/* Create main window. */
#ifdef HAVE_OSX_1012_SDK
	unsigned int style = NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskClosable;
#else
	unsigned int style = NSTitledWindowMask | NSResizableWindowMask | NSMiniaturizableWindowMask | NSClosableWindowMask;
#endif
	this->window = [ [ OTTD_CocoaWindow alloc ] initWithContentRect:contentRect styleMask:style backing:NSBackingStoreBuffered defer:NO driver:this ];
	if (this->window == nil) {
		Debug(driver, 0, "Could not create the Cocoa window.");
		this->setup = false;
		return false;
	}

	/* Add built in full-screen support when available (OS X 10.7 and higher)
	 * This code actually compiles for 10.5 and later, but only makes sense in conjunction
	 * with the quartz fullscreen support as found only in 10.7 and later. */
	if ([ this->window respondsToSelector:@selector(toggleFullScreen:) ]) {
		NSWindowCollectionBehavior behavior = [ this->window collectionBehavior ];
		behavior |= NSWindowCollectionBehaviorFullScreenPrimary;
		[ this->window setCollectionBehavior:behavior ];

		NSButton *fullscreenButton = [ this->window standardWindowButton:NSWindowZoomButton ];
		[ fullscreenButton setAction:@selector(toggleFullScreen:) ];
		[ fullscreenButton setTarget:this->window ];
	}

	this->delegate = [ [ OTTD_CocoaWindowDelegate alloc ] initWithDriver:this ];
	[ this->window setDelegate:this->delegate ];

	[ this->window center ];
	[ this->window makeKeyAndOrderFront:nil ];

	/* Create wrapper view for input and event handling. */
	NSRect view_frame = [ this->window contentRectForFrameRect:[ this->window frame ] ];
	this->cocoaview = [ [ OTTD_CocoaView alloc ] initWithFrame:view_frame ];
	if (this->cocoaview == nil) {
		Debug(driver, 0, "Could not create the event wrapper view.");
		this->setup = false;
		return false;
	}
	[ this->cocoaview setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable ];

	/* Create content view. */
	NSView *draw_view = this->AllocateDrawView();
	if (draw_view == nil) {
		Debug(driver, 0, "Could not create the drawing view.");
		this->setup = false;
		return false;
	}
	[ draw_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable ];

	/* Create view chain: window -> input wrapper view -> content view. */
	[ this->window setContentView:this->cocoaview ];
	[ this->cocoaview addSubview:draw_view ];
	[ this->window makeFirstResponder:this->cocoaview ];
	[ draw_view release ];

	[ this->window setColorSpace:[ NSColorSpace sRGBColorSpace ] ];
	CGColorSpaceRelease(this->color_space);
	this->color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	if (this->color_space == nullptr) this->color_space = CGColorSpaceCreateDeviceRGB();
	if (this->color_space == nullptr) FatalError("Could not get a valid colour space for drawing.");

	this->setup = false;

	return true;
}


/**
 * Poll and handle a single event from the OS.
 * @return True if there was an event to handle.
 */
bool VideoDriver_Cocoa::PollEvent()
{
#ifdef HAVE_OSX_1012_SDK
	NSEventMask mask = NSEventMaskAny;
#else
	NSEventMask mask = NSAnyEventMask;
#endif
	NSEvent *event = [ NSApp nextEventMatchingMask:mask untilDate:[ NSDate distantPast ] inMode:NSDefaultRunLoopMode dequeue:YES ];

	if (event == nil) return false;

	[ NSApp sendEvent:event ];

	return true;
}

void VideoDriver_Cocoa::InputLoop()
{
	NSUInteger cur_mods = [ NSEvent modifierFlags ];

	bool old_ctrl_pressed = _ctrl_pressed;

	_ctrl_pressed = (cur_mods & ( _settings_client.gui.right_mouse_btn_emulation != RMBE_CONTROL ? NSEventModifierFlagControl : NSEventModifierFlagCommand)) != 0;
	_shift_pressed = (cur_mods & NSEventModifierFlagShift) != 0;

	this->fast_forward_key_pressed = _tab_is_down;

	if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();
}

/** Main game loop. */
void VideoDriver_Cocoa::MainLoopReal()
{
	this->StartGameThread();

	for (;;) {
		@autoreleasepool {
			if (_exit_game) {
				/* Restore saved resolution if in fullscreen mode. */
				if (this->IsFullscreen()) _cur_resolution = this->orig_res;
				break;
			}

			this->Tick();
			this->SleepTillNextTick();
		}
	}

	this->StopGameThread();
}


/* Subclass of OTTD_CocoaView to fix Quartz rendering */
@interface OTTD_QuartzView : NSView {
	VideoDriver_CocoaQuartz *driver;
}
- (instancetype)initWithFrame:(NSRect)frameRect andDriver:(VideoDriver_CocoaQuartz *)drv;
@end

@implementation OTTD_QuartzView

- (instancetype)initWithFrame:(NSRect)frameRect andDriver:(VideoDriver_CocoaQuartz *)drv
{
	if (self = [ super initWithFrame:frameRect ]) {
		self->driver = drv;

		/* We manage our content updates ourselves. */
		self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
		self.wantsLayer = YES;

		self.layer.magnificationFilter = kCAFilterNearest;
		self.layer.opaque = YES;
	}
	return self;
}

- (BOOL)acceptsFirstResponder
{
	return NO;
}

- (BOOL)isOpaque
{
	return YES;
}

- (BOOL)wantsUpdateLayer
{
	return YES;
}

- (void)updateLayer
{
	if (driver->cgcontext == nullptr) return;

	/* Set layer contents to our backing buffer, which avoids needless copying. */
	CGImageRef fullImage = CGBitmapContextCreateImage(driver->cgcontext);
	self.layer.contents = (__bridge id)fullImage;
	CGImageRelease(fullImage);
}

- (void)viewDidChangeBackingProperties
{
	[ super viewDidChangeBackingProperties ];

	self.layer.contentsScale = [ driver->cocoaview getContentsScale ];
}

@end


static FVideoDriver_CocoaQuartz iFVideoDriver_CocoaQuartz;

/** Clear buffer to opaque black. */
static void ClearWindowBuffer(uint32_t *buffer, uint32_t pitch, uint32_t height)
{
	uint32_t fill = Colour(0, 0, 0).data;
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < pitch; x++) {
			buffer[y * pitch + x] = fill;
		}
	}
}

VideoDriver_CocoaQuartz::VideoDriver_CocoaQuartz()
{
	this->window_width  = 0;
	this->window_height = 0;
	this->window_pitch  = 0;
	this->buffer_depth  = 0;
	this->window_buffer = nullptr;
	this->pixel_buffer  = nullptr;

	this->cgcontext     = nullptr;
}

const char *VideoDriver_CocoaQuartz::Start(const StringList &param)
{
	const char *err = this->Initialize();
	if (err != nullptr) return err;

	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	if (bpp != 8 && bpp != 32) {
		Stop();
		return "The cocoa quartz subdriver only supports 8 and 32 bpp.";
	}

	bool fullscreen = _fullscreen;
	if (!this->MakeWindow(_cur_resolution.width, _cur_resolution.height)) {
		Stop();
		return "Could not create window";
	}

	this->AllocateBackingStore(true);

	if (fullscreen) this->ToggleFullscreen(fullscreen);

	this->GameSizeChanged();
	this->UpdateVideoModes();

	this->is_game_threaded = !GetDriverParamBool(param, "no_threads") && !GetDriverParamBool(param, "no_thread");

	return nullptr;

}

void VideoDriver_CocoaQuartz::Stop()
{
	this->VideoDriver_Cocoa::Stop();

	CGContextRelease(this->cgcontext);

	free(this->window_buffer);
	free(this->pixel_buffer);
}

NSView *VideoDriver_CocoaQuartz::AllocateDrawView()
{
	return [ [ OTTD_QuartzView alloc ] initWithFrame:[ this->cocoaview bounds ] andDriver:this ];
}

/** Resize the window. */
void VideoDriver_CocoaQuartz::AllocateBackingStore(bool)
{
	if (this->window == nil || this->cocoaview == nil || this->setup) return;

	this->UpdatePalette(0, 256);

	NSRect newframe = [ this->cocoaview getRealRect:[ this->cocoaview frame ] ];

	this->window_width = (int)newframe.size.width;
	this->window_height = (int)newframe.size.height;
	this->window_pitch = Align(this->window_width, 16 / sizeof(uint32_t)); // Quartz likes lines that are multiple of 16-byte.
	this->buffer_depth = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	/* Create Core Graphics Context */
	free(this->window_buffer);
	this->window_buffer = malloc(this->window_pitch * this->window_height * sizeof(uint32_t));
	/* Initialize with opaque black. */
	ClearWindowBuffer((uint32_t *)this->window_buffer, this->window_pitch, this->window_height);

	CGContextRelease(this->cgcontext);
	this->cgcontext = CGBitmapContextCreate(
		this->window_buffer,       // data
		this->window_width,        // width
		this->window_height,       // height
		8,                         // bits per component
		this->window_pitch * 4,    // bytes per row
		this->color_space,         // color space
		kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host
	);

	assert(this->cgcontext != nullptr);
	CGContextSetShouldAntialias(this->cgcontext, FALSE);
	CGContextSetAllowsAntialiasing(this->cgcontext, FALSE);
	CGContextSetInterpolationQuality(this->cgcontext, kCGInterpolationNone);

	if (this->buffer_depth == 8) {
		free(this->pixel_buffer);
		this->pixel_buffer = malloc(this->window_width * this->window_height);
		if (this->pixel_buffer == nullptr) UserError("Out of memory allocating pixel buffer");
	} else {
		free(this->pixel_buffer);
		this->pixel_buffer = nullptr;
	}

	/* Tell the game that the resolution has changed */
	_screen.width   = this->window_width;
	_screen.height  = this->window_height;
	_screen.pitch   = this->buffer_depth == 8 ? this->window_width : this->window_pitch;
	_screen.dst_ptr = this->GetVideoPointer();

	/* Redraw screen */
	this->MakeDirty(0, 0, _screen.width, _screen.height);
	this->GameSizeChanged();
}

/**
 * This function copies 8bpp pixels from the screen buffer in 32bpp windowed mode.
 *
 * @param left The x coord for the left edge of the box to blit.
 * @param top The y coord for the top edge of the box to blit.
 * @param right The x coord for the right edge of the box to blit.
 * @param bottom The y coord for the bottom edge of the box to blit.
 */
void VideoDriver_CocoaQuartz::BlitIndexedToView32(int left, int top, int right, int bottom)
{
	const uint32_t *pal   = this->palette;
	const uint8_t  *src   = (uint8_t*)this->pixel_buffer;
	uint32_t       *dst   = (uint32_t*)this->window_buffer;
	uint          width = this->window_width;
	uint          pitch = this->window_pitch;

	for (int y = top; y < bottom; y++) {
		for (int x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}

/** Update the palette */
void VideoDriver_CocoaQuartz::UpdatePalette(uint first_color, uint num_colors)
{
	if (this->buffer_depth != 8) return;

	for (uint i = first_color; i < first_color + num_colors; i++) {
		uint32_t clr = 0xff000000;
		clr |= (uint32_t)_local_palette.palette[i].r << 16;
		clr |= (uint32_t)_local_palette.palette[i].g << 8;
		clr |= (uint32_t)_local_palette.palette[i].b;
		this->palette[i] = clr;
	}

	this->MakeDirty(0, 0, _screen.width, _screen.height);
}

void VideoDriver_CocoaQuartz::CheckPaletteAnim()
{
	if (!CopyPalette(_local_palette)) return;

	Blitter *blitter = BlitterFactory::GetCurrentBlitter();

	switch (blitter->UsePaletteAnimation()) {
		case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
			this->UpdatePalette(_local_palette.first_dirty, _local_palette.count_dirty);
			break;

		case Blitter::PALETTE_ANIMATION_BLITTER:
			blitter->PaletteAnimate(_local_palette);
			break;

		case Blitter::PALETTE_ANIMATION_NONE:
			break;

		default:
			NOT_REACHED();
	}
}

/** Draw window */
void VideoDriver_CocoaQuartz::Paint()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	/* Check if we need to do anything */
	if (IsEmptyRect(this->dirty_rect) || [ this->window isMiniaturized ]) return;

	/* We only need to blit in indexed mode since in 32bpp mode the game draws directly to the image. */
	if (this->buffer_depth == 8) {
		BlitIndexedToView32(
			this->dirty_rect.left,
			this->dirty_rect.top,
			this->dirty_rect.right,
			this->dirty_rect.bottom
		);
	}

	NSRect dirtyrect;
	dirtyrect.origin.x = this->dirty_rect.left;
	dirtyrect.origin.y = this->window_height - this->dirty_rect.bottom;
	dirtyrect.size.width = this->dirty_rect.right - this->dirty_rect.left;
	dirtyrect.size.height = this->dirty_rect.bottom - this->dirty_rect.top;

	/* Notify OS X that we have new content to show. */
	[ this->cocoaview setNeedsDisplayInRect:[ this->cocoaview getVirtualRect:dirtyrect ] ];

	/* Tell the OS to get our contents to screen as soon as possible. */
	[ CATransaction flush ];

	this->dirty_rect = {};
}

#endif /* WITH_COCOA */
