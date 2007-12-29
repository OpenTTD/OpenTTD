/* $Id$ */

/******************************************************************************
 *                             Cocoa video driver                             *
 * Known things left to do:                                                   *
 *  Scale&copy the old pixel buffer to the new one when switching resolution. *
 ******************************************************************************/

#ifdef WITH_COCOA

#define MAC_OS_X_VERSION_MIN_REQUIRED    MAC_OS_X_VERSION_10_3
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


/* From Menus.h (according to Xcode Developer Documentation) */
extern "C" void ShowMenuBar();
extern "C" void HideMenuBar();

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
#include "../../core/geometry_type.hpp"
#include "cocoa_v.h"
#include "../../gfx_func.h"

#undef Rect


/* Structure for rez switch gamma fades
 * We can hide the monitor flicker by setting the gamma tables to 0
 */
#define QZ_GAMMA_TABLE_SIZE 256

struct OTTD_QuartzGammaTable {
	CGGammaValue red[QZ_GAMMA_TABLE_SIZE];
	CGGammaValue green[QZ_GAMMA_TABLE_SIZE];
	CGGammaValue blue[QZ_GAMMA_TABLE_SIZE];
};

/* Add methods to get at private members of NSScreen.
 * Since there is a bug in Apple's screen switching code that does not update
 * this variable when switching to fullscreen, we'll set it manually (but only
 * for the main screen).
 */
@interface NSScreen (NSScreenAccess)
	- (void) setFrame:(NSRect)frame;
@end

@implementation NSScreen (NSScreenAccess)
- (void) setFrame:(NSRect)frame;
{
	_frame = frame;
}
@end



uint QZ_ListModes(OTTD_Point* modes, uint max_modes, CGDirectDisplayID display_id, int display_depth)
{
	CFArrayRef mode_list;
	CFIndex num_modes;
	CFIndex i;
	uint count = 0;

	mode_list  = CGDisplayAvailableModes(display_id);
	num_modes = CFArrayGetCount(mode_list);

	/* Build list of modes with the requested bpp */
	for (i = 0; i < num_modes && count < max_modes; i++) {
		CFDictionaryRef onemode;
		CFNumberRef     number;
		int bpp;
		int intvalue;
		bool hasMode;
		uint16 width, height;

		onemode = (const __CFDictionary*)CFArrayGetValueAtIndex(mode_list, i);
		number = (const __CFNumber*)CFDictionaryGetValue(onemode, kCGDisplayBitsPerPixel);
		CFNumberGetValue (number, kCFNumberSInt32Type, &bpp);

		if (bpp != display_depth) continue;

		number = (const __CFNumber*)CFDictionaryGetValue(onemode, kCGDisplayWidth);
		CFNumberGetValue(number, kCFNumberSInt32Type, &intvalue);
		width = (uint16)intvalue;

		number = (const __CFNumber*)CFDictionaryGetValue(onemode, kCGDisplayHeight);
		CFNumberGetValue(number, kCFNumberSInt32Type, &intvalue);
		height = (uint16)intvalue;

		/* Check if mode is already in the list */
		{
			uint i;
			hasMode = false;
			for (i = 0; i < count; i++) {
				if (modes[i].x == width &&  modes[i].y == height) {
					hasMode = true;
					break;
				}
			}
		}

		if (hasMode) continue;

		/* Add mode to the list */
		modes[count].x = width;
		modes[count].y = height;
		count++;
	}

	/* Sort list smallest to largest */
	{
		uint i, j;
		for (i = 0; i < count; i++) {
			for (j = 0; j < count-1; j++) {
				if (modes[j].x > modes[j + 1].x || (
					modes[j].x == modes[j + 1].x &&
					modes[j].y >  modes[j + 1].y
					)) {
					uint tmpw = modes[j].x;
					uint tmph = modes[j].y;

					modes[j].x = modes[j + 1].x;
					modes[j].y = modes[j + 1].y;

					modes[j + 1].x = tmpw;
					modes[j + 1].y = tmph;
				}
			}
		}
	}

	return count;
}


class FullscreenSubdriver: public CocoaSubdriver {
	int                display_width;
	int                display_height;
	int                display_depth;
	int                screen_pitch;
	void*              screen_buffer;
	void*              pixel_buffer;

	CGDirectDisplayID  display_id;         /* 0 == main display (only support single display) */
	CFDictionaryRef    cur_mode;           /* current mode of the display */
	CFDictionaryRef    save_mode;          /* original mode of the display */
	CGDirectPaletteRef palette;            /* palette of an 8-bit display */

	#define MAX_DIRTY_RECTS 100
	Rect dirty_rects[MAX_DIRTY_RECTS];
	int num_dirty_rects;


	/* Gamma functions to try to hide the flash from a rez switch
	 * Fade the display from normal to black
	 * Save gamma tables for fade back to normal
	 */
	uint32 FadeGammaOut(OTTD_QuartzGammaTable* table)
	{
		CGGammaValue redTable[QZ_GAMMA_TABLE_SIZE];
		CGGammaValue greenTable[QZ_GAMMA_TABLE_SIZE];
		CGGammaValue blueTable[QZ_GAMMA_TABLE_SIZE];
		float percent;
		int j;
		unsigned int actual;

		if (CGGetDisplayTransferByTable(
					display_id, QZ_GAMMA_TABLE_SIZE,
					table->red, table->green, table->blue, &actual
				) != CGDisplayNoErr ||
				actual != QZ_GAMMA_TABLE_SIZE) {
			return 1;
		}

		memcpy(redTable,   table->red,   sizeof(redTable));
		memcpy(greenTable, table->green, sizeof(greenTable));
		memcpy(blueTable,  table->blue,  sizeof(greenTable));

		for (percent = 1.0; percent >= 0.0; percent -= 0.01) {
			for (j = 0; j < QZ_GAMMA_TABLE_SIZE; j++) {
				redTable[j]   = redTable[j]   * percent;
				greenTable[j] = greenTable[j] * percent;
				blueTable[j]  = blueTable[j]  * percent;
			}

			if (CGSetDisplayTransferByTable(
						display_id, QZ_GAMMA_TABLE_SIZE,
						redTable, greenTable, blueTable
					) != CGDisplayNoErr) {
				CGDisplayRestoreColorSyncSettings();
				return 1;
			}

			CSleep(10);
		}

		return 0;
	}

	/* Fade the display from black to normal
	 * Restore previously saved gamma values
	 */
	uint32 FadeGammaIn(const OTTD_QuartzGammaTable* table)
	{
		CGGammaValue redTable[QZ_GAMMA_TABLE_SIZE];
		CGGammaValue greenTable[QZ_GAMMA_TABLE_SIZE];
		CGGammaValue blueTable[QZ_GAMMA_TABLE_SIZE];
		float percent;
		int j;

		memset(redTable, 0, sizeof(redTable));
		memset(greenTable, 0, sizeof(greenTable));
		memset(blueTable, 0, sizeof(greenTable));

		for (percent = 0.0; percent <= 1.0; percent += 0.01) {
			for (j = 0; j < QZ_GAMMA_TABLE_SIZE; j++) {
				redTable[j]   = table->red[j]   * percent;
				greenTable[j] = table->green[j] * percent;
				blueTable[j]  = table->blue[j]  * percent;
			}

			if (CGSetDisplayTransferByTable(
						display_id, QZ_GAMMA_TABLE_SIZE,
						redTable, greenTable, blueTable
					) != CGDisplayNoErr) {
				CGDisplayRestoreColorSyncSettings();
				return 1;
			}

			CSleep(10);
		}

		return 0;
	}

	/* Wait for the VBL to occur (estimated since we don't have a hardware interrupt) */
	void WaitForVerticalBlank()
	{
		/* The VBL delay is based on Ian Ollmann's RezLib <iano@cco.caltech.edu> */
		double refreshRate;
		double linesPerSecond;
		double target;
		double position;
		double adjustment;
		CFNumberRef refreshRateCFNumber;

		refreshRateCFNumber = (const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayRefreshRate);
		if (refreshRateCFNumber == NULL) return;

		if (CFNumberGetValue(refreshRateCFNumber, kCFNumberDoubleType, &refreshRate) == 0)
			return;

		if (refreshRate == 0) return;

		linesPerSecond = refreshRate * display_height;
		target = display_height;

		/* Figure out the first delay so we start off about right */
		position = CGDisplayBeamPosition(display_id);
		if (position > target) position = 0;

		adjustment = (target - position) / linesPerSecond;

		CSleep((uint32)(adjustment * 1000));
	}


	bool SetVideoMode(int w, int h)
	{
		int exact_match;
		CFNumberRef number;
		int bpp;
		int gamma_error;
		OTTD_QuartzGammaTable gamma_table;
		NSRect screen_rect;
		CGError error;
		NSPoint pt;

		/* Destroy any previous mode */
		if (pixel_buffer != NULL) {
			free(pixel_buffer);
			pixel_buffer = NULL;
		}

		/* See if requested mode exists */
		cur_mode = CGDisplayBestModeForParameters(display_id, display_depth, w, h, &exact_match);

		/* If the mode wasn't an exact match, check if it has the right bpp, and update width and height */
		if (!exact_match) {
			number = (const __CFNumber*) CFDictionaryGetValue(cur_mode, kCGDisplayBitsPerPixel);
			CFNumberGetValue(number, kCFNumberSInt32Type, &bpp);
			if (bpp != display_depth) {
				DEBUG(driver, 0, "Failed to find display resolution");
				goto ERR_NO_MATCH;
			}

			number = (const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayWidth);
			CFNumberGetValue(number, kCFNumberSInt32Type, &w);

			number = (const __CFNumber*)CFDictionaryGetValue(cur_mode, kCGDisplayHeight);
			CFNumberGetValue(number, kCFNumberSInt32Type, &h);
		}

		/* Fade display to zero gamma */
		gamma_error = FadeGammaOut(&gamma_table);

		/* Put up the blanking window (a window above all other windows) */
		error = CGDisplayCapture(display_id);

		if (CGDisplayNoErr != error) {
			DEBUG(driver, 0, "Failed capturing display");
			goto ERR_NO_CAPTURE;
		}

		/* Do the physical switch */
		if (CGDisplaySwitchToMode(display_id, cur_mode) != CGDisplayNoErr) {
			DEBUG(driver, 0, "Failed switching display resolution");
			goto ERR_NO_SWITCH;
		}

		screen_buffer = CGDisplayBaseAddress(display_id);
		screen_pitch  = CGDisplayBytesPerRow(display_id);

		display_width = CGDisplayPixelsWide(display_id);
		display_height = CGDisplayPixelsHigh(display_id);

		/* Setup double-buffer emulation */
		pixel_buffer = malloc(display_width * display_height * display_depth / 8);
		if (pixel_buffer == NULL) {
			DEBUG(driver, 0, "Failed to allocate memory for double buffering");
			goto ERR_DOUBLEBUF;
		}

		if (display_depth == 8 && !CGDisplayCanSetPalette(display_id)) {
			DEBUG(driver, 0, "Not an indexed display mode.");
			goto ERR_NOT_INDEXED;
		}

		/* If we don't hide menu bar, it will get events and interrupt the program */
		HideMenuBar();

		/* Fade the display to original gamma */
		if (!gamma_error) FadeGammaIn(&gamma_table);

		/* There is a bug in Cocoa where NSScreen doesn't synchronize
		 * with CGDirectDisplay, so the main screen's frame is wrong.
		 * As a result, coordinate translation produces incorrect results.
		 * We can hack around this bug by setting the screen rect ourselves.
		 * This hack should be removed if/when the bug is fixed.
		*/
		screen_rect = NSMakeRect(0, 0, display_width, display_height);
		[ [ NSScreen mainScreen ] setFrame:screen_rect ];


		pt = [ NSEvent mouseLocation ];
		pt.y = display_height - pt.y;
		if (MouseIsInsideView(&pt)) QZ_HideMouse();

		UpdatePalette(0, 256);

		return true;

		/* Since the blanking window covers *all* windows (even force quit) correct recovery is crucial */
ERR_NOT_INDEXED:
		free(pixel_buffer);
		pixel_buffer = NULL;
ERR_DOUBLEBUF:
		CGDisplaySwitchToMode(display_id, save_mode);
ERR_NO_SWITCH:
		CGReleaseAllDisplays();
ERR_NO_CAPTURE:
		if (!gamma_error) FadeGammaIn(&gamma_table);
ERR_NO_MATCH:
		display_width = 0;
		display_height = 0;

		return false;
	}

	void RestoreVideoMode()
	{
		/* Release fullscreen resources */
		OTTD_QuartzGammaTable gamma_table;
		int gamma_error;
		NSRect screen_rect;

		gamma_error = FadeGammaOut(&gamma_table);

		/* Restore original screen resolution/bpp */
		CGDisplaySwitchToMode(display_id, save_mode);
		CGReleaseAllDisplays();
		ShowMenuBar();
		/* Reset the main screen's rectangle
		 * See comment in SetVideoMode for why we do this
		 */
		screen_rect = NSMakeRect(0, 0, CGDisplayPixelsWide(display_id), CGDisplayPixelsHigh(display_id));
		[ [ NSScreen mainScreen ] setFrame:screen_rect ];

		QZ_ShowMouse();

		/* Destroy the pixel buffer */
		if (pixel_buffer != NULL) {
			free(pixel_buffer);
			pixel_buffer = NULL;
		}

		if (!gamma_error) FadeGammaIn(&gamma_table);

		display_width = 0;
		display_height = 0;
	}

public:
	FullscreenSubdriver(int bpp)
	{
		if (bpp != 8 && bpp != 32) {
			error("Cocoa: This video driver only supports 8 and 32 bpp blitters.");
		}

		/* Initialize the video settings; this data persists between mode switches */
		display_id = kCGDirectMainDisplay;
		save_mode  = CGDisplayCurrentMode(display_id);

		if (bpp == 8) palette = CGPaletteCreateDefaultColorPalette();

		display_width  = 0;
		display_height = 0;
		display_depth  = bpp;
		pixel_buffer   = NULL;

		num_dirty_rects = MAX_DIRTY_RECTS;
	}

	virtual ~FullscreenSubdriver()
	{
		RestoreVideoMode();
	}

	virtual void Draw()
	{
		const uint8* src   = (uint8*) pixel_buffer;
		uint8* dst         = (uint8*) screen_buffer;
		uint pitch         = screen_pitch;
		uint width         = display_width;
		uint num_dirty     = num_dirty_rects;
		uint bytesperpixel = display_depth / 8;
		uint i;

		/* Check if we need to do anything */
		if (num_dirty == 0) return;

		if (num_dirty >= MAX_DIRTY_RECTS) {
			num_dirty = 1;
			dirty_rects[0].left   = 0;
			dirty_rects[0].top    = 0;
			dirty_rects[0].right  = display_width;
			dirty_rects[0].bottom = display_height;
		}

		WaitForVerticalBlank();
		/* Build the region of dirty rectangles */
		for (i = 0; i < num_dirty; i++) {
			uint y      = dirty_rects[i].top;
			uint left   = dirty_rects[i].left;
			uint length = dirty_rects[i].right - left;
			uint bottom = dirty_rects[i].bottom;

			for (; y < bottom; y++) {
				memcpy(dst + y * pitch + left * bytesperpixel, src + y * width * bytesperpixel + left * bytesperpixel, length * bytesperpixel);
			}
		}

		num_dirty_rects = 0;
	}

	virtual void MakeDirty(int left, int top, int width, int height)
	{
		if (num_dirty_rects < MAX_DIRTY_RECTS) {
			dirty_rects[num_dirty_rects].left = left;
			dirty_rects[num_dirty_rects].top = top;
			dirty_rects[num_dirty_rects].right = left + width;
			dirty_rects[num_dirty_rects].bottom = top + height;
		}
		num_dirty_rects++;
	}

	virtual void UpdatePalette(uint first_color, uint num_colors)
	{
		CGTableCount  index;
		CGDeviceColor color;

		if (display_depth != 8)
			return;

		for (index = first_color; index < first_color+num_colors; index++) {
			/* Clamp colors between 0.0 and 1.0 */
			color.red   = _cur_palette[index].r / 255.0;
			color.blue  = _cur_palette[index].b / 255.0;
			color.green = _cur_palette[index].g / 255.0;

			CGPaletteSetColorAtIndex(palette, color, index);
		}

		CGDisplaySetPalette(display_id, palette);
	}

	virtual uint ListModes(OTTD_Point* modes, uint max_modes)
	{
		return QZ_ListModes(modes, max_modes, display_id, display_depth);
	}

	virtual bool ChangeResolution(int w, int h)
	{
		int old_width  = display_width;
		int old_height = display_height;

		if (SetVideoMode(w, h))
			return true;

		if (old_width != 0 && old_height != 0)
			SetVideoMode(old_width, old_height);

		return false;
	}

	virtual bool IsFullscreen()
	{
		return true;
	}

	virtual int GetWidth()
	{
		return display_width;
	}

	virtual int GetHeight()
	{
		return display_height;
	}

	virtual void *GetPixelBuffer()
	{
		return pixel_buffer;
	}

	/*
		Convert local coordinate to window server (CoreGraphics) coordinate.
		In fullscreen mode this just means copying the coords.
	*/
	virtual CGPoint PrivateLocalToCG(NSPoint* p)
	{
		CGPoint cgp;

		cgp.x = p->x;
		cgp.y = p->y;

		return cgp;
	}

	virtual NSPoint GetMouseLocation(NSEvent *event)
	{
		NSPoint pt;

		pt = [ NSEvent mouseLocation ];
		pt.y = display_height - pt.y;

		return pt;
	}

	virtual bool MouseIsInsideView(NSPoint *pt)
	{
		return pt->x >= 0 && pt->y >= 0 && pt->x < display_width && pt->y < display_height;
	}

	virtual bool IsActive()
	{
		return true;
	}
};

CocoaSubdriver *QZ_CreateFullscreenSubdriver(int width, int height, int bpp)
{
	FullscreenSubdriver *ret;

	ret = new FullscreenSubdriver(bpp);

	if (!ret->ChangeResolution(width, height)) {
		delete ret;
		return NULL;
	}

	return ret;
}

#endif /* WITH_COCOA */
