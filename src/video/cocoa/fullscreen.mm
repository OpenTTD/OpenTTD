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
 *  Scale&copy the old pixel buffer to the new one when switching resolution. *
 ******************************************************************************/

#ifdef WITH_COCOA

#include "../../stdafx.h"

#define Rect  OTTDRect
#define Point OTTDPoint
#import <Cocoa/Cocoa.h>
#undef Rect
#undef Point

#include "../../debug.h"
#include "../../core/geometry_type.hpp"
#include "../../core/sort_func.hpp"
#include "cocoa_v.h"
#include "../../gfx_func.h"
#include "../../os/macosx/macos.h"

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
/* The 64 bits libraries don't seem to know about _frame, so this hack won't work. */
#if !__LP64__
	_frame = frame;
#endif
}
@end


static int CDECL ModeSorter(const OTTD_Point *p1, const OTTD_Point *p2)
{
	if (p1->x < p2->x) return -1;
	if (p1->x > p2->x) return +1;
	if (p1->y < p2->y) return -1;
	if (p1->y > p2->y) return +1;
	return 0;
}

uint QZ_ListModes(OTTD_Point *modes, uint max_modes, CGDirectDisplayID display_id, int device_depth)
{
	CFArrayRef mode_list  = CGDisplayAvailableModes(display_id);
	CFIndex    num_modes = CFArrayGetCount(mode_list);

	/* Build list of modes with the requested bpp */
	uint count = 0;
	for (CFIndex i = 0; i < num_modes && count < max_modes; i++) {
		int intvalue, bpp;
		uint16 width, height;

		CFDictionaryRef onemode = (const __CFDictionary*)CFArrayGetValueAtIndex(mode_list, i);
		CFNumberRef number = (const __CFNumber*)CFDictionaryGetValue(onemode, kCGDisplayBitsPerPixel);
		CFNumberGetValue(number, kCFNumberSInt32Type, &bpp);

		if (bpp != device_depth) continue;

		number = (const __CFNumber*)CFDictionaryGetValue(onemode, kCGDisplayWidth);
		CFNumberGetValue(number, kCFNumberSInt32Type, &intvalue);
		width = (uint16)intvalue;

		number = (const __CFNumber*)CFDictionaryGetValue(onemode, kCGDisplayHeight);
		CFNumberGetValue(number, kCFNumberSInt32Type, &intvalue);
		height = (uint16)intvalue;

		/* Check if mode is already in the list */
		bool hasMode = false;
		for (uint i = 0; i < count; i++) {
			if (modes[i].x == width &&  modes[i].y == height) {
				hasMode = true;
				break;
			}
		}

		if (hasMode) continue;

		/* Add mode to the list */
		modes[count].x = width;
		modes[count].y = height;
		count++;
	}

	/* Sort list smallest to largest */
	QSortT(modes, count, &ModeSorter);

	return count;
}

/** Small function to test if the main display can display 8 bpp in fullscreen */
bool QZ_CanDisplay8bpp()
{
	/* 8bpp modes are deprecated starting in 10.5. CoreGraphics will return them
	 * as available in the display list, but many features (e.g. palette animation)
	 * will be broken. */
	if (MacOSVersionIsAtLeast(10, 5, 0)) return false;

	OTTD_Point p;

	/* We want to know if 8 bpp is possible in fullscreen and not anything about
	 * resolutions. Because of this we want to fill a list of 1 resolution of 8 bpp
	 * on display 0 (main) and return if we found one. */
	return QZ_ListModes(&p, 1, 0, 8);
}

class FullscreenSubdriver: public CocoaSubdriver {
	CGDirectDisplayID  display_id;         ///< 0 == main display (only support single display)
	CFDictionaryRef    cur_mode;           ///< current mode of the display
	CFDictionaryRef    save_mode;          ///< original mode of the display
	CGDirectPaletteRef palette;            ///< palette of an 8-bit display


	/* Gamma functions to try to hide the flash from a res switch
	 * Fade the display from normal to black
	 * Save gamma tables for fade back to normal
	 */
	uint32 FadeGammaOut(OTTD_QuartzGammaTable *table)
	{
		CGGammaValue redTable[QZ_GAMMA_TABLE_SIZE];
		CGGammaValue greenTable[QZ_GAMMA_TABLE_SIZE];
		CGGammaValue blueTable[QZ_GAMMA_TABLE_SIZE];

		unsigned int actual;
		if (CGGetDisplayTransferByTable(this->display_id, QZ_GAMMA_TABLE_SIZE, table->red, table->green, table->blue, &actual) != CGDisplayNoErr
				|| actual != QZ_GAMMA_TABLE_SIZE) {
			return 1;
		}

		memcpy(redTable,   table->red,   sizeof(redTable));
		memcpy(greenTable, table->green, sizeof(greenTable));
		memcpy(blueTable,  table->blue,  sizeof(greenTable));

		for (float percent = 1.0; percent >= 0.0; percent -= 0.01) {
			for (int j = 0; j < QZ_GAMMA_TABLE_SIZE; j++) {
				redTable[j]   = redTable[j]   * percent;
				greenTable[j] = greenTable[j] * percent;
				blueTable[j]  = blueTable[j]  * percent;
			}

			if (CGSetDisplayTransferByTable(this->display_id, QZ_GAMMA_TABLE_SIZE, redTable, greenTable, blueTable) != CGDisplayNoErr) {
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
	uint32 FadeGammaIn(const OTTD_QuartzGammaTable *table)
	{
		CGGammaValue redTable[QZ_GAMMA_TABLE_SIZE];
		CGGammaValue greenTable[QZ_GAMMA_TABLE_SIZE];
		CGGammaValue blueTable[QZ_GAMMA_TABLE_SIZE];

		memset(redTable, 0, sizeof(redTable));
		memset(greenTable, 0, sizeof(greenTable));
		memset(blueTable, 0, sizeof(greenTable));

		for (float percent = 0.0; percent <= 1.0; percent += 0.01) {
			for (int j = 0; j < QZ_GAMMA_TABLE_SIZE; j++) {
				redTable[j]   = table->red[j]   * percent;
				greenTable[j] = table->green[j] * percent;
				blueTable[j]  = table->blue[j]  * percent;
			}

			if (CGSetDisplayTransferByTable(this->display_id, QZ_GAMMA_TABLE_SIZE, redTable, greenTable, blueTable) != CGDisplayNoErr) {
				CGDisplayRestoreColorSyncSettings();
				return 1;
			}

			CSleep(10);
		}

		return 0;
	}

	/** Wait for the VBL to occur (estimated since we don't have a hardware interrupt) */
	void WaitForVerticalBlank()
	{
		/* The VBL delay is based on Ian Ollmann's RezLib <iano@cco.caltech.edu> */

		CFNumberRef refreshRateCFNumber = (const __CFNumber*)CFDictionaryGetValue(this->cur_mode, kCGDisplayRefreshRate);
		if (refreshRateCFNumber == NULL) return;

		double refreshRate;
		if (CFNumberGetValue(refreshRateCFNumber, kCFNumberDoubleType, &refreshRate) == 0) return;

		if (refreshRate == 0) return;

		double linesPerSecond = refreshRate * this->device_height;
		double target = this->device_height;

		/* Figure out the first delay so we start off about right */
		double position = CGDisplayBeamPosition(this->display_id);
		if (position > target) position = 0;

		double adjustment = (target - position) / linesPerSecond;

		CSleep((uint32)(adjustment * 1000));
	}


	bool SetVideoMode(int w, int h)
	{
		/* Define this variables at the top (against coding style) because
		 * otherwise GCC 4.2 barfs at the goto's jumping over variable initialization. */
		NSRect screen_rect;
		int gamma_error;
		NSPoint mouseLocation;

		/* Destroy any previous mode */
		if (this->pixel_buffer != NULL) {
			free(this->pixel_buffer);
			this->pixel_buffer = NULL;
		}

		/* See if requested mode exists */
		boolean_t exact_match;
		this->cur_mode = CGDisplayBestModeForParameters(this->display_id, this->device_depth, w, h, &exact_match);

		/* If the mode wasn't an exact match, check if it has the right bpp, and update width and height */
		if (!exact_match) {
			int bpp;
			CFNumberRef number = (const __CFNumber*) CFDictionaryGetValue(this->cur_mode, kCGDisplayBitsPerPixel);
			CFNumberGetValue(number, kCFNumberSInt32Type, &bpp);
			if (bpp != this->device_depth) {
				DEBUG(driver, 0, "Failed to find display resolution");
				goto ERR_NO_MATCH;
			}

			number = (const __CFNumber*)CFDictionaryGetValue(this->cur_mode, kCGDisplayWidth);
			CFNumberGetValue(number, kCFNumberSInt32Type, &w);

			number = (const __CFNumber*)CFDictionaryGetValue(this->cur_mode, kCGDisplayHeight);
			CFNumberGetValue(number, kCFNumberSInt32Type, &h);
		}

		/* Capture the main screen */
		CGDisplayCapture(this->display_id);

		/* Store the mouse coordinates relative to the total screen */
		mouseLocation = [ NSEvent mouseLocation ];
		mouseLocation.x /= this->device_width;
		mouseLocation.y /= this->device_height;

		/* Fade display to zero gamma */
		OTTD_QuartzGammaTable gamma_table;
		gamma_error = this->FadeGammaOut(&gamma_table);

		/* Put up the blanking window (a window above all other windows) */
		if (CGDisplayCapture(this->display_id) != CGDisplayNoErr ) {
			DEBUG(driver, 0, "Failed capturing display");
			goto ERR_NO_CAPTURE;
		}

		/* Do the physical switch */
		if (CGDisplaySwitchToMode(this->display_id, this->cur_mode) != CGDisplayNoErr) {
			DEBUG(driver, 0, "Failed switching display resolution");
			goto ERR_NO_SWITCH;
		}

		this->window_buffer = CGDisplayBaseAddress(this->display_id);
		this->window_pitch  = CGDisplayBytesPerRow(this->display_id);

		this->device_width  = CGDisplayPixelsWide(this->display_id);
		this->device_height = CGDisplayPixelsHigh(this->display_id);

		/* Setup double-buffer emulation */
		this->pixel_buffer = malloc(this->device_width * this->device_height * this->device_depth / 8);
		if (this->pixel_buffer == NULL) {
			DEBUG(driver, 0, "Failed to allocate memory for double buffering");
			goto ERR_DOUBLEBUF;
		}

		if (this->device_depth == 8 && !CGDisplayCanSetPalette(this->display_id)) {
			DEBUG(driver, 0, "Not an indexed display mode.");
			goto ERR_NOT_INDEXED;
		}

		/* If we don't hide menu bar, it will get events and interrupt the program */
		HideMenuBar();

		/* Hide the OS cursor */
		CGDisplayHideCursor(this->display_id);

		/* Fade the display to original gamma */
		if (!gamma_error) FadeGammaIn(&gamma_table);

		/* There is a bug in Cocoa where NSScreen doesn't synchronize
		 * with CGDirectDisplay, so the main screen's frame is wrong.
		 * As a result, coordinate translation produces incorrect results.
		 * We can hack around this bug by setting the screen rect ourselves.
		 * This hack should be removed if/when the bug is fixed.
		 */
		screen_rect = NSMakeRect(0, 0, this->device_width, this->device_height);
		[ [ NSScreen mainScreen ] setFrame:screen_rect ];

		this->UpdatePalette(0, 256);

		/* Move the mouse cursor to approx the same location */
		CGPoint display_mouseLocation;
		display_mouseLocation.x = mouseLocation.x * this->device_width;
		display_mouseLocation.y = this->device_height - (mouseLocation.y * this->device_height);

		_cursor.in_window = true;

		CGDisplayMoveCursorToPoint(this->display_id, display_mouseLocation);

		return true;

		/* Since the blanking window covers *all* windows (even force quit) correct recovery is crucial */
ERR_NOT_INDEXED:
		free(this->pixel_buffer);
		this->pixel_buffer = NULL;
ERR_DOUBLEBUF:
		CGDisplaySwitchToMode(this->display_id, this->save_mode);
ERR_NO_SWITCH:
		CGReleaseAllDisplays();
ERR_NO_CAPTURE:
		if (!gamma_error) this->FadeGammaIn(&gamma_table);
ERR_NO_MATCH:
		this->device_width = 0;
		this->device_height = 0;

		return false;
	}

	void RestoreVideoMode()
	{
		/* Release fullscreen resources */
		OTTD_QuartzGammaTable gamma_table;
		int gamma_error = this->FadeGammaOut(&gamma_table);

		/* Restore original screen resolution/bpp */
		CGDisplaySwitchToMode(this->display_id, this->save_mode);

		CGReleaseAllDisplays();

		/* Bring back the cursor */
		CGDisplayShowCursor(this->display_id);

		ShowMenuBar();

		/* Reset the main screen's rectangle
		 * See comment in SetVideoMode for why we do this */
		NSRect screen_rect = NSMakeRect(0, 0, CGDisplayPixelsWide(this->display_id), CGDisplayPixelsHigh(this->display_id));
		[ [ NSScreen mainScreen ] setFrame:screen_rect ];

		/* Destroy the pixel buffer */
		if (this->pixel_buffer != NULL) {
			free(this->pixel_buffer);
			this->pixel_buffer = NULL;
		}

		if (!gamma_error) this->FadeGammaIn(&gamma_table);

		this->device_width  = CGDisplayPixelsWide(this->display_id);
		this->device_height = CGDisplayPixelsHigh(this->display_id);
	}

public:
	FullscreenSubdriver(int bpp)
	{
		if (bpp != 8 && bpp != 32) {
			error("Cocoa: This video driver only supports 8 and 32 bpp blitters.");
		}

		/* Initialize the video settings; this data persists between mode switches */
		this->display_id = kCGDirectMainDisplay;
		this->save_mode  = CGDisplayCurrentMode(this->display_id);

		if (bpp == 8) this->palette = CGPaletteCreateDefaultColorPalette();

		this->device_width  = CGDisplayPixelsWide(this->display_id);
		this->device_height = CGDisplayPixelsHigh(this->display_id);
		this->device_depth  = bpp;
		this->pixel_buffer   = NULL;

		this->num_dirty_rects = MAX_DIRTY_RECTS;
	}

	virtual ~FullscreenSubdriver()
	{
		this->RestoreVideoMode();
	}

	virtual void Draw(bool force_update)
	{
		const uint8 *src   = (uint8 *)this->pixel_buffer;
		uint8 *dst         = (uint8 *)this->window_buffer;
		uint pitch         = this->window_pitch;
		uint width         = this->device_width;
		uint num_dirty     = this->num_dirty_rects;
		uint bytesperpixel = this->device_depth / 8;

		/* Check if we need to do anything */
		if (num_dirty == 0) return;

		if (num_dirty >= MAX_DIRTY_RECTS) {
			num_dirty = 1;
			this->dirty_rects[0].left   = 0;
			this->dirty_rects[0].top    = 0;
			this->dirty_rects[0].right  = this->device_width;
			this->dirty_rects[0].bottom = this->device_height;
		}

		WaitForVerticalBlank();
		/* Build the region of dirty rectangles */
		for (uint i = 0; i < num_dirty; i++) {
			uint y      = this->dirty_rects[i].top;
			uint left   = this->dirty_rects[i].left;
			uint length = this->dirty_rects[i].right - left;
			uint bottom = this->dirty_rects[i].bottom;

			for (; y < bottom; y++) {
				memcpy(dst + y * pitch + left * bytesperpixel, src + y * width * bytesperpixel + left * bytesperpixel, length * bytesperpixel);
			}
		}

		this->num_dirty_rects = 0;
	}

	virtual void MakeDirty(int left, int top, int width, int height)
	{
		if (this->num_dirty_rects < MAX_DIRTY_RECTS) {
			this->dirty_rects[this->num_dirty_rects].left = left;
			this->dirty_rects[this->num_dirty_rects].top = top;
			this->dirty_rects[this->num_dirty_rects].right = left + width;
			this->dirty_rects[this->num_dirty_rects].bottom = top + height;
		}
		this->num_dirty_rects++;
	}

	virtual void UpdatePalette(uint first_color, uint num_colors)
	{
		if (this->device_depth != 8) return;

		for (uint32_t index = first_color; index < first_color + num_colors; index++) {
			/* Clamp colors between 0.0 and 1.0 */
			CGDeviceColor color;
			color.red   = _cur_palette[index].r / 255.0;
			color.blue  = _cur_palette[index].b / 255.0;
			color.green = _cur_palette[index].g / 255.0;

			CGPaletteSetColorAtIndex(this->palette, color, index);
		}

		CGDisplaySetPalette(this->display_id, this->palette);
	}

	virtual uint ListModes(OTTD_Point *modes, uint max_modes)
	{
		return QZ_ListModes(modes, max_modes, this->display_id, this->device_depth);
	}

	virtual bool ChangeResolution(int w, int h)
	{
		int old_width  = this->device_width;
		int old_height = this->device_height;

		if (SetVideoMode(w, h)) return true;

		if (old_width != 0 && old_height != 0) SetVideoMode(old_width, old_height);

		return false;
	}

	virtual bool IsFullscreen()
	{
		return true;
	}

	virtual int GetWidth()
	{
		return this->device_width;
	}

	virtual int GetHeight()
	{
		return this->device_height;
	}

	virtual void *GetPixelBuffer()
	{
		return this->pixel_buffer;
	}

	/*
	 * Convert local coordinate to window server (CoreGraphics) coordinate.
	 * In fullscreen mode this just means copying the coords.
	 */
	virtual CGPoint PrivateLocalToCG(NSPoint *p)
	{
		return CGPointMake(p->x, p->y);
	}

	virtual NSPoint GetMouseLocation(NSEvent *event)
	{
		NSPoint pt = [ NSEvent mouseLocation ];
		pt.y = this->device_height - pt.y;

		return pt;
	}

	virtual bool MouseIsInsideView(NSPoint *pt)
	{
		return pt->x >= 0 && pt->y >= 0 && pt->x < this->device_width && pt->y < this->device_height;
	}

	virtual bool IsActive()
	{
		return true;
	}
};

CocoaSubdriver *QZ_CreateFullscreenSubdriver(int width, int height, int bpp)
{
	FullscreenSubdriver *ret = new FullscreenSubdriver(bpp);

	if (!ret->ChangeResolution(width, height)) {
		delete ret;
		return NULL;
	}

	return ret;
}

#endif /* WITH_COCOA */
