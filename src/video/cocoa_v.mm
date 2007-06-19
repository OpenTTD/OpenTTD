/* $Id$ */

/******************************************************************************
 *                             Cocoa video driver                             *
 * Known things left to do:                                                   *
 *  Nothing at the moment.                                                    *
 ******************************************************************************/

#ifdef WITH_COCOA

#include <AvailabilityMacros.h>
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4)
/* On 10.4, we get tons of warnings that the QuickDraw functions are deprecated.
 *  We know that. Don't keep bugging us about that. */
#	undef AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4
#	define AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_4 AVAILABLE_MAC_OS_X_VERSION_10_0_AND_LATER
#endif

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


/* Portions of CPS.h */
struct CPSProcessSerNum {
	UInt32 lo;
	UInt32 hi;
};

extern "C" OSErr CPSGetCurrentProcess(CPSProcessSerNum* psn);
extern "C" OSErr CPSEnableForegroundOperation(CPSProcessSerNum* psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern "C" OSErr CPSSetFrontProcess(CPSProcessSerNum* psn);

/* From Menus.h (according to Xcode Developer Documentation) */
extern "C" void ShowMenuBar();
extern "C" void HideMenuBar();

/* Disables a warning. This is needed since the method exists but has been dropped from the header, supposedly as of 10.4. */
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4)
@interface NSApplication(NSAppleMenu)
- (void)setAppleMenu:(NSMenu *)menu;
@end
#endif


/* Defined in ppc/param.h or i386/param.h included from sys/param.h */
#undef ALIGN
/* Defined in stdbool.h */
#ifndef __cplusplus
# ifndef __BEOS__
#  undef bool
#  undef false
#  undef true
# endif
#endif


#include "../stdafx.h"
#include "../debug.h"
#include "../macros.h"
#include "../os/macosx/splash.h"
#include "../variables.h"
#include "cocoa_v.h"
#include "cocoa_keys.h"
#include "../blitter/factory.hpp"
#include "../fileio.h"

#undef Point
#undef Rect

/* Taken from ../gfx.h */
extern bool _dbg_screen_rect;


/* Subclass of NSWindow to fix genie effect and support resize events  */
@interface OTTD_QuartzWindow : NSWindow
- (void)miniaturize:(id)sender;
- (void)display;
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag;
- (void)appDidHide:(NSNotification*)note;
- (void)appWillUnhide:(NSNotification*)note;
- (void)appDidUnhide:(NSNotification*)note;
- (id)initWithContentRect:(NSRect)contentRect styleMask:(unsigned int)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag;
@end

/* Delegate for our NSWindow to send ask for quit on close */
@interface OTTD_QuartzWindowDelegate : NSObject
- (BOOL)windowShouldClose:(id)sender;
@end

@interface OTTDMain : NSObject
@end


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


static void QZ_Draw();
static void QZ_UnsetVideoMode();
static void QZ_UpdatePalette(uint start, uint count);
static void QZ_WarpCursor(int x, int y);
static void QZ_ShowMouse();
static void QZ_HideMouse();
static void CocoaVideoFullScreen(bool full_screen);


static NSAutoreleasePool *_ottd_autorelease_pool;
static OTTDMain *_ottd_main;


static struct CocoaVideoData {
	bool isset;
	bool issetting;

	CGDirectDisplayID  display_id;         /* 0 == main display (only support single display) */
	CFDictionaryRef    mode;               /* current mode of the display */
	CFDictionaryRef    save_mode;          /* original mode of the display */
	CFArrayRef         mode_list;          /* list of available fullscreen modes */
	CGDirectPaletteRef palette;            /* palette of an 8-bit display */

	uint32 device_width;
	uint32 device_height;
	uint32 device_bpp;

	void *realpixels;
	uint8 *pixels;
	uint32 width;
	uint32 height;
	uint32 pitch;
	bool fullscreen;

	unsigned int current_mods;
	bool tab_is_down;
	bool emulating_right_button;

	bool cursor_visible;
	bool active;

#ifdef _DEBUG
	uint32 tEvent;
#endif

	OTTD_QuartzWindow *window;
	NSQuickDrawView *qdview;

#define MAX_DIRTY_RECTS 100
	Rect dirty_rects[MAX_DIRTY_RECTS];
	int num_dirty_rects;

	uint16 palette16[256];
	uint32 palette32[256];
} _cocoa_video_data;

static bool _cocoa_video_started = false;
static bool _cocoa_video_dialog = false;




/******************************************************************************
 *                             Game loop and accessories                      *
 ******************************************************************************/

static uint32 GetTick()
{
	struct timeval tim;

	gettimeofday(&tim, NULL);
	return tim.tv_usec / 1000 + tim.tv_sec * 1000;
}

static void QZ_CheckPaletteAnim()
{
	if (_pal_count_dirty != 0) {
		Blitter *blitter = BlitterFactoryBase::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
				QZ_UpdatePalette(_pal_first_dirty, _pal_count_dirty);
				break;

			case Blitter::PALETTE_ANIMATION_BLITTER:
				blitter->PaletteAnimate(_pal_first_dirty, _pal_count_dirty);
				break;

			case Blitter::PALETTE_ANIMATION_NONE:
				break;

			default:
				NOT_REACHED();
		}
		_pal_count_dirty = 0;
	}
}



struct VkMapping {
	unsigned short vk_from;
	byte map_to;
};

#define AS(x, z) {x, z}

static const VkMapping _vk_mapping[] = {
	AS(QZ_BACKQUOTE,  WKC_BACKQUOTE), // key left of '1'
	AS(QZ_BACKQUOTE2, WKC_BACKQUOTE), // some keyboards have it on another scancode

	// Pageup stuff + up/down
	//AM(SDLK_PAGEUP, SDLK_PAGEDOWN, WKC_PAGEUP, WKC_PAGEDOWN),  <==== Does this include HOME/END?
	AS(QZ_PAGEUP,   WKC_PAGEUP),
	AS(QZ_PAGEDOWN, WKC_PAGEDOWN),

	AS(QZ_UP,    WKC_UP),
	AS(QZ_DOWN,  WKC_DOWN),
	AS(QZ_LEFT,  WKC_LEFT),
	AS(QZ_RIGHT, WKC_RIGHT),

	AS(QZ_HOME, WKC_HOME),
	AS(QZ_END,  WKC_END),

	AS(QZ_INSERT, WKC_INSERT),
	AS(QZ_DELETE, WKC_DELETE),

	// Letters. QZ_[a-z] is not in numerical order so we can't use AM(...)
	AS(QZ_a, 'A'),
	AS(QZ_b, 'B'),
	AS(QZ_c, 'C'),
	AS(QZ_d, 'D'),
	AS(QZ_e, 'E'),
	AS(QZ_f, 'F'),
	AS(QZ_g, 'G'),
	AS(QZ_h, 'H'),
	AS(QZ_i, 'I'),
	AS(QZ_j, 'J'),
	AS(QZ_k, 'K'),
	AS(QZ_l, 'L'),
	AS(QZ_m, 'M'),
	AS(QZ_n, 'N'),
	AS(QZ_o, 'O'),
	AS(QZ_p, 'P'),
	AS(QZ_q, 'Q'),
	AS(QZ_r, 'R'),
	AS(QZ_s, 'S'),
	AS(QZ_t, 'T'),
	AS(QZ_u, 'U'),
	AS(QZ_v, 'V'),
	AS(QZ_w, 'W'),
	AS(QZ_x, 'X'),
	AS(QZ_y, 'Y'),
	AS(QZ_z, 'Z'),
	// Same thing for digits
	AS(QZ_0, '0'),
	AS(QZ_1, '1'),
	AS(QZ_2, '2'),
	AS(QZ_3, '3'),
	AS(QZ_4, '4'),
	AS(QZ_5, '5'),
	AS(QZ_6, '6'),
	AS(QZ_7, '7'),
	AS(QZ_8, '8'),
	AS(QZ_9, '9'),

	AS(QZ_ESCAPE,    WKC_ESC),
	AS(QZ_PAUSE,     WKC_PAUSE),
	AS(QZ_BACKSPACE, WKC_BACKSPACE),

	AS(QZ_SPACE,  WKC_SPACE),
	AS(QZ_RETURN, WKC_RETURN),
	AS(QZ_TAB,    WKC_TAB),

	// Function keys
	AS(QZ_F1,  WKC_F1),
	AS(QZ_F2,  WKC_F2),
	AS(QZ_F3,  WKC_F3),
	AS(QZ_F4,  WKC_F4),
	AS(QZ_F5,  WKC_F5),
	AS(QZ_F6,  WKC_F6),
	AS(QZ_F7,  WKC_F7),
	AS(QZ_F8,  WKC_F8),
	AS(QZ_F9,  WKC_F9),
	AS(QZ_F10, WKC_F10),
	AS(QZ_F11, WKC_F11),
	AS(QZ_F12, WKC_F12),

	// Numeric part.
	AS(QZ_KP0,         WKC_NUM_0),
	AS(QZ_KP1,         WKC_NUM_1),
	AS(QZ_KP2,         WKC_NUM_2),
	AS(QZ_KP3,         WKC_NUM_3),
	AS(QZ_KP4,         WKC_NUM_4),
	AS(QZ_KP5,         WKC_NUM_5),
	AS(QZ_KP6,         WKC_NUM_6),
	AS(QZ_KP7,         WKC_NUM_7),
	AS(QZ_KP8,         WKC_NUM_8),
	AS(QZ_KP9,         WKC_NUM_9),
	AS(QZ_KP_DIVIDE,   WKC_NUM_DIV),
	AS(QZ_KP_MULTIPLY, WKC_NUM_MUL),
	AS(QZ_KP_MINUS,    WKC_NUM_MINUS),
	AS(QZ_KP_PLUS,     WKC_NUM_PLUS),
	AS(QZ_KP_ENTER,    WKC_NUM_ENTER),
	AS(QZ_KP_PERIOD,   WKC_NUM_DECIMAL)
};


static uint32 QZ_MapKey(unsigned short sym)
{
	const VkMapping *map;
	uint32 key = 0;

	for (map = _vk_mapping; map != endof(_vk_mapping); ++map) {
		if (sym == map->vk_from) {
			key = map->map_to;
			break;
		}
	}

	if (_cocoa_video_data.current_mods & NSShiftKeyMask)     key |= WKC_SHIFT;
	if (_cocoa_video_data.current_mods & NSControlKeyMask)   key |= WKC_CTRL;
	if (_cocoa_video_data.current_mods & NSAlternateKeyMask) key |= WKC_ALT;
	if (_cocoa_video_data.current_mods & NSCommandKeyMask)   key |= WKC_META;

	return key << 16;
}

static void QZ_KeyEvent(unsigned short keycode, unsigned short unicode, BOOL down)
{
	switch (keycode) {
		case QZ_UP:    SB(_dirkeys, 1, 1, down); break;
		case QZ_DOWN:  SB(_dirkeys, 3, 1, down); break;
		case QZ_LEFT:  SB(_dirkeys, 0, 1, down); break;
		case QZ_RIGHT: SB(_dirkeys, 2, 1, down); break;

		case QZ_TAB: _cocoa_video_data.tab_is_down = down; break;

		case QZ_RETURN:
		case QZ_f:
			if (down && (_cocoa_video_data.current_mods & NSCommandKeyMask)) {
				CocoaVideoFullScreen(!_fullscreen);
			}
			break;
	}

	if (down) {
		uint32 pressed_key = QZ_MapKey(keycode) | unicode;
		HandleKeypress(pressed_key);
		DEBUG(driver, 2, "cocoa_v: QZ_KeyEvent: %x (%x), down, mapping: %x", keycode, unicode, pressed_key);
	} else {
		DEBUG(driver, 2, "cocoa_v: QZ_KeyEvent: %x (%x), up", keycode, unicode);
	}
}

static void QZ_DoUnsidedModifiers(unsigned int newMods)
{
	const int mapping[] = { QZ_CAPSLOCK, QZ_LSHIFT, QZ_LCTRL, QZ_LALT, QZ_LMETA };

	int i;
	unsigned int bit;

	if (_cocoa_video_data.current_mods == newMods) return;

	/* Iterate through the bits, testing each against the current modifiers */
	for (i = 0, bit = NSAlphaShiftKeyMask; bit <= NSCommandKeyMask; bit <<= 1, ++i) {
		unsigned int currentMask, newMask;

		currentMask = _cocoa_video_data.current_mods & bit;
		newMask     = newMods & bit;

		if (currentMask && currentMask != newMask) { /* modifier up event */
			/* If this was Caps Lock, we need some additional voodoo to make SDL happy (is this needed in ottd?) */
			if (bit == NSAlphaShiftKeyMask) QZ_KeyEvent(mapping[i], 0, YES);
			QZ_KeyEvent(mapping[i], 0, NO);
		} else if (newMask && currentMask != newMask) { /* modifier down event */
			QZ_KeyEvent(mapping[i], 0, YES);
			/* If this was Caps Lock, we need some additional voodoo to make SDL happy (is this needed in ottd?) */
			if (bit == NSAlphaShiftKeyMask) QZ_KeyEvent(mapping[i], 0, NO);
		}
	}

	_cocoa_video_data.current_mods = newMods;
}

static void QZ_MouseMovedEvent(int x, int y)
{
	if (_cursor.fix_at) {
		int dx = x - _cursor.pos.x;
		int dy = y - _cursor.pos.y;

		if (dx != 0 || dy != 0) {
			_cursor.delta.x += dx;
			_cursor.delta.y += dy;

			QZ_WarpCursor(_cursor.pos.x, _cursor.pos.y);
		}
	} else {
		_cursor.delta.x = x - _cursor.pos.x;
		_cursor.delta.y = y - _cursor.pos.y;
		_cursor.pos.x = x;
		_cursor.pos.y = y;
		_cursor.dirty = true;
	}
	HandleMouseEvents();
}


static void QZ_MouseButtonEvent(int button, BOOL down)
{
	switch (button) {
		case 0:
			if (down) {
				_left_button_down = true;
			} else {
				_left_button_down = false;
				_left_button_clicked = false;
			}
			HandleMouseEvents();
			break;

		case 1:
			if (down) {
				_right_button_down = true;
				_right_button_clicked = true;
			} else {
				_right_button_down = false;
			}
			HandleMouseEvents();
			break;
	}
}


static inline NSPoint QZ_GetMouseLocation(NSEvent *event)
{
	NSPoint pt;

	if (_cocoa_video_data.fullscreen) {
		pt = [ NSEvent mouseLocation ];
		pt.y = _cocoa_video_data.height - pt.y;
	} else {
		pt = [event locationInWindow];
		pt = [_cocoa_video_data.qdview convertPoint:pt fromView:nil];
	}

	return pt;
}

static bool QZ_MouseIsInsideView(NSPoint *pt)
{
	if (_cocoa_video_data.fullscreen) {
		return pt->x >= 0 && pt->y >= 0 && pt->x < _cocoa_video_data.width && pt->y < _cocoa_video_data.height;
	} else {
		return [ _cocoa_video_data.qdview mouse:*pt inRect:[ _cocoa_video_data.qdview bounds ] ];
	}
}


static bool QZ_PollEvent()
{
	NSEvent *event;
	NSPoint pt;
	NSString *chars;
#ifdef _DEBUG
	uint32 et0, et;
#endif

#ifdef _DEBUG
	et0 = GetTick();
#endif
	event = [ NSApp nextEventMatchingMask:NSAnyEventMask
			untilDate: [ NSDate distantPast ]
			inMode: NSDefaultRunLoopMode dequeue:YES ];
#ifdef _DEBUG
	et = GetTick();
	_cocoa_video_data.tEvent+= et - et0;
#endif

	if (event == nil) return false;
	if (!_cocoa_video_data.active) {
		QZ_ShowMouse();
		[NSApp sendEvent:event];
		return true;
	}

	QZ_DoUnsidedModifiers( [ event modifierFlags ] );

	switch ([event type]) {
		case NSMouseMoved:
		case NSOtherMouseDragged:
		case NSRightMouseDragged:
		case NSLeftMouseDragged:
			pt = QZ_GetMouseLocation(event);
			if (!QZ_MouseIsInsideView(&pt) &&
					!_cocoa_video_data.emulating_right_button) {
				QZ_ShowMouse();
				[NSApp sendEvent:event];
				break;
			}

			QZ_HideMouse();
			QZ_MouseMovedEvent((int)pt.x, (int)pt.y);
			break;

		case NSLeftMouseDown:
			pt = QZ_GetMouseLocation(event);
			if (!([ event modifierFlags ] & NSCommandKeyMask) ||
					!QZ_MouseIsInsideView(&pt)) {
				[NSApp sendEvent:event];
			}

			if (!QZ_MouseIsInsideView(&pt)) {
				QZ_ShowMouse();
				break;
			}

			QZ_HideMouse();
			QZ_MouseMovedEvent((int)pt.x, (int)pt.y);

			/* Right mouse button emulation */
			if ([ event modifierFlags ] & NSCommandKeyMask) {
				_cocoa_video_data.emulating_right_button = true;
				QZ_MouseButtonEvent(1, YES);
			} else {
				QZ_MouseButtonEvent(0, YES);
			}
			break;

		case NSLeftMouseUp:
			[NSApp sendEvent:event];

			pt = QZ_GetMouseLocation(event);
			if (!QZ_MouseIsInsideView(&pt)) {
				QZ_ShowMouse();
				break;
			}

			QZ_HideMouse();
			QZ_MouseMovedEvent((int)pt.x, (int)pt.y);

			/* Right mouse button emulation */
			if (_cocoa_video_data.emulating_right_button) {
				_cocoa_video_data.emulating_right_button = false;
				QZ_MouseButtonEvent(1, NO);
			} else {
				QZ_MouseButtonEvent(0, NO);
			}
			break;

		case NSRightMouseDown:
			pt = QZ_GetMouseLocation(event);
			if (!QZ_MouseIsInsideView(&pt)) {
				QZ_ShowMouse();
				[NSApp sendEvent:event];
				break;
			}

			QZ_HideMouse();
			QZ_MouseMovedEvent((int)pt.x, (int)pt.y);
			QZ_MouseButtonEvent(1, YES);
			break;

		case NSRightMouseUp:
			pt = QZ_GetMouseLocation(event);
			if (!QZ_MouseIsInsideView(&pt)) {
				QZ_ShowMouse();
				[NSApp sendEvent:event];
				break;
			}

			QZ_HideMouse();
			QZ_MouseMovedEvent((int)pt.x, (int)pt.y);
			QZ_MouseButtonEvent(1, NO);
			break;

#if 0
		/* This is not needed since openttd currently only use two buttons */
		case NSOtherMouseDown:
			pt = QZ_GetMouseLocation(event);
			if (!QZ_MouseIsInsideView(&pt)) {
				QZ_ShowMouse();
				[NSApp sendEvent:event];
				break;
			}

			QZ_HideMouse();
			QZ_MouseMovedEvent((int)pt.x, (int)pt.y);
			QZ_MouseButtonEvent([ event buttonNumber ], YES);
			break;

		case NSOtherMouseUp:
			pt = QZ_GetMouseLocation(event);
			if (!QZ_MouseIsInsideView(&pt)) {
				QZ_ShowMouse();
				[NSApp sendEvent:event];
				break;
			}

			QZ_HideMouse();
			QZ_MouseMovedEvent((int)pt.x, (int)pt.y);
			QZ_MouseButtonEvent([ event buttonNumber ], NO);
			break;
#endif

		case NSKeyDown:
			/* Quit, hide and minimize */
			switch ([event keyCode]) {
				case QZ_q:
				case QZ_h:
				case QZ_m:
					if ([ event modifierFlags ] & NSCommandKeyMask) {
						[NSApp sendEvent:event];
					}
					break;
			}

			chars = [ event characters ];
			QZ_KeyEvent([event keyCode], [ chars length ] ? [ chars characterAtIndex:0 ] : 0, YES);
			break;

		case NSKeyUp:
			/* Quit, hide and minimize */
			switch ([event keyCode]) {
				case QZ_q:
				case QZ_h:
				case QZ_m:
					if ([ event modifierFlags ] & NSCommandKeyMask) {
						[NSApp sendEvent:event];
					}
					break;
			}

			chars = [ event characters ];
			QZ_KeyEvent([event keyCode], [ chars length ] ? [ chars characterAtIndex:0 ] : 0, NO);
			break;

		case NSScrollWheel:
			if ([ event deltaY ] > 0.0) { /* Scroll up */
				_cursor.wheel--;
			} else if ([ event deltaY ] < 0.0) { /* Scroll down */
				_cursor.wheel++;
			} /* else: deltaY was 0.0 and we don't want to do anything */

			/* Set the scroll count for scrollwheel scrolling */
			_cursor.h_wheel -= (int)([ event deltaX ]* 5 * _patches.scrollwheel_multiplier);
			_cursor.v_wheel -= (int)([ event deltaY ]* 5 * _patches.scrollwheel_multiplier);
			break;

		default:
			[NSApp sendEvent:event];
	}

	return true;
}


static void QZ_GameLoop()
{
	uint32 cur_ticks = GetTick();
	uint32 next_tick = cur_ticks + 30;
	uint32 pal_tick = 0;
#ifdef _DEBUG
	uint32 et0, et, st0, st;
#endif
	int i;

#ifdef _DEBUG
	et0 = GetTick();
	st = 0;
#endif

	_screen.dst_ptr = _cocoa_video_data.pixels;
	DisplaySplashImage();
	QZ_CheckPaletteAnim();
	QZ_Draw();
	CSleep(1);

	for (i = 0; i < 2; i++) GameLoop();

	_screen.dst_ptr = _cocoa_video_data.pixels;
	UpdateWindows();
	QZ_CheckPaletteAnim();
	QZ_Draw();
	CSleep(1);

	for (;;) {
		uint32 prev_cur_ticks = cur_ticks; // to check for wrapping
		InteractiveRandom(); // randomness

		while (QZ_PollEvent()) {}

		if (_exit_game) break;

#if defined(_DEBUG)
		if (_cocoa_video_data.current_mods & NSShiftKeyMask)
#else
		if (_cocoa_video_data.tab_is_down)
#endif
		{
			if (!_networking && _game_mode != GM_MENU) _fast_forward |= 2;
		} else if (_fast_forward & 2) {
			_fast_forward = 0;
		}

		cur_ticks = GetTick();
		if (cur_ticks >= next_tick || (_fast_forward && !_pause_game) || cur_ticks < prev_cur_ticks) {
			next_tick = cur_ticks + 30;

			_ctrl_pressed = !!(_cocoa_video_data.current_mods & NSControlKeyMask);
			_shift_pressed = !!(_cocoa_video_data.current_mods & NSShiftKeyMask);
#ifdef _DEBUG
			_dbg_screen_rect = !!(_cocoa_video_data.current_mods & NSAlphaShiftKeyMask);
#endif

			GameLoop();

			_screen.dst_ptr = _cocoa_video_data.pixels;
			UpdateWindows();
			if (++pal_tick > 4) {
				QZ_CheckPaletteAnim();
				pal_tick = 1;
			}
			QZ_Draw();
		} else {
#ifdef _DEBUG
			st0 = GetTick();
#endif
			CSleep(1);
#ifdef _DEBUG
			st += GetTick() - st0;
#endif
			_screen.dst_ptr = _cocoa_video_data.pixels;
			DrawTextMessage();
			DrawMouseCursor();
			QZ_Draw();
		}
	}

#ifdef _DEBUG
	et = GetTick();

	DEBUG(driver, 1, "cocoa_v: nextEventMatchingMask took %i ms total", _cocoa_video_data.tEvent);
	DEBUG(driver, 1, "cocoa_v: game loop took %i ms total (%i ms without sleep)", et - et0, et - et0 - st);
	DEBUG(driver, 1, "cocoa_v: (nextEventMatchingMask total)/(game loop total) is %f%%", (double)_cocoa_video_data.tEvent / (double)(et - et0) * 100);
	DEBUG(driver, 1, "cocoa_v: (nextEventMatchingMask total)/(game loop without sleep total) is %f%%", (double)_cocoa_video_data.tEvent / (double)(et - et0 - st) * 100);
#endif
}


/******************************************************************************
 *                             Windowed mode                                  *
 ******************************************************************************/

/* This function makes the *game region* of the window 100% opaque.
 * The genie effect uses the alpha component. Otherwise,
 * it doesn't seem to matter what value it has.
 */
static void QZ_SetPortAlphaOpaque()
{
	if (_cocoa_video_data.device_bpp == 32) {
		uint32* pixels = (uint32*)_cocoa_video_data.realpixels;
		uint32 rowPixels = _cocoa_video_data.pitch / 4;
		uint32 i;
		uint32 j;

		for (i = 0; i < _cocoa_video_data.height; i++)
			for (j = 0; j < _cocoa_video_data.width; j++) {
			pixels[i * rowPixels + j] |= 0xFF000000;
		}
	}
}


@implementation OTTD_QuartzWindow

/* we override these methods to fix the miniaturize animation/dock icon bug */
- (void)miniaturize:(id)sender
{
	/* make the alpha channel opaque so anim won't have holes in it */
	QZ_SetPortAlphaOpaque ();

	/* window is hidden now */
	_cocoa_video_data.active = false;

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

	QZ_SetPortAlphaOpaque();

	/* save current visible surface */
	[ self cacheImageInRect:[ _cocoa_video_data.qdview frame ] ];

	/* let the window manager redraw controls, border, etc */
	[ super display ];

	/* restore visible surface */
	[ self restoreCachedImage ];

	/* window is visible again */
	_cocoa_video_data.active = true;
}

- (void)setFrame:(NSRect)frameRect display:(BOOL)flag
{
	NSRect newViewFrame;
	CGrafPtr thePort;

	[ super setFrame:frameRect display:flag ];

	/* Don't do anything if the window is currently beign created */
	if (_cocoa_video_data.issetting) return;

	if (_cocoa_video_data.window == nil) return;

	newViewFrame = [ _cocoa_video_data.qdview frame ];

	/* Update the pixels and pitch */
	thePort = (OpaqueGrafPtr*) [ _cocoa_video_data.qdview qdPort ];
	LockPortBits(thePort);

	_cocoa_video_data.realpixels = GetPixBaseAddr(GetPortPixMap(thePort));
	_cocoa_video_data.pitch      = GetPixRowBytes(GetPortPixMap(thePort));

	/* _cocoa_video_data.realpixels now points to the window's pixels
	 * We want it to point to the *view's* pixels
	 */
	{
		int vOffset = [ _cocoa_video_data.window frame ].size.height - newViewFrame.size.height - newViewFrame.origin.y;
		int hOffset = newViewFrame.origin.x;

		_cocoa_video_data.realpixels = (uint8*)_cocoa_video_data.realpixels + (vOffset * _cocoa_video_data.pitch) + hOffset * (_cocoa_video_data.device_bpp / 8);
	}

	UnlockPortBits(thePort);

	/* Allocate new buffer */
	free(_cocoa_video_data.pixels);
	_cocoa_video_data.pixels = (uint8*)malloc(newViewFrame.size.width * newViewFrame.size.height);
	assert(_cocoa_video_data.pixels != NULL);


	/* Tell the game that the resolution changed */
	_cocoa_video_data.width = newViewFrame.size.width;
	_cocoa_video_data.height = newViewFrame.size.height;

	_screen.width = _cocoa_video_data.width;
	_screen.height = _cocoa_video_data.height;
	_screen.pitch = _cocoa_video_data.width;

	GameSizeChanged();

	/* Redraw screen */
	_cocoa_video_data.num_dirty_rects = MAX_DIRTY_RECTS;
}

- (void)appDidHide:(NSNotification*)note
{
	_cocoa_video_data.active = false;
}


- (void)appWillUnhide:(NSNotification*)note
{
	QZ_SetPortAlphaOpaque ();

	/* save current visible surface */
	[ self cacheImageInRect:[ _cocoa_video_data.qdview frame ] ];
}

- (void)appDidUnhide:(NSNotification*)note
{
	/* restore cached image, since it may not be current, post expose event too */
	[ self restoreCachedImage ];

	_cocoa_video_data.active = true;
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
- (BOOL)windowShouldClose:(id)sender
{
	HandleExitGameRequest();

	return NO;
}

- (void)windowDidBecomeKey:(NSNotification*)aNotification
{
	_cocoa_video_data.active = true;
}

- (void)windowDidResignKey:(NSNotification*)aNotification
{
	_cocoa_video_data.active = false;
}

- (void)windowDidBecomeMain:(NSNotification*)aNotification
{
	_cocoa_video_data.active = true;
}

- (void)windowDidResignMain:(NSNotification*)aNotification
{
	_cocoa_video_data.active = false;
}

@end


static void QZ_UpdateWindowPalette(uint start, uint count)
{
	uint i;

	switch (_cocoa_video_data.device_bpp) {
		case 32:
			for (i = start; i < start + count; i++) {
				uint32 clr32 = 0xff000000;
				clr32 |= (uint32)_cur_palette[i].r << 16;
				clr32 |= (uint32)_cur_palette[i].g << 8;
				clr32 |= (uint32)_cur_palette[i].b;
				_cocoa_video_data.palette32[i] = clr32;
			}
			break;
		case 16:
			for (i = start; i < start + count; i++) {
				uint16 clr16 = 0x0000;
				clr16 |= (uint16)((_cur_palette[i].r >> 3) & 0x1f) << 10;
				clr16 |= (uint16)((_cur_palette[i].g >> 3) & 0x1f) << 5;
				clr16 |= (uint16)((_cur_palette[i].b >> 3) & 0x1f);
				_cocoa_video_data.palette16[i] = clr16;
			}
			break;
	}

	_cocoa_video_data.num_dirty_rects = MAX_DIRTY_RECTS;
}

static inline void QZ_WindowBlitIndexedPixelsToView32(uint left, uint top, uint right, uint bottom)
{
	const uint32* pal = _cocoa_video_data.palette32;
	const uint8* src = _cocoa_video_data.pixels;
	uint32* dst = (uint32*)_cocoa_video_data.realpixels;
	uint width = _cocoa_video_data.width;
	uint pitch = _cocoa_video_data.pitch / 4;
	uint x;
	uint y;

	for (y = top; y < bottom; y++) {
		for (x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}

static inline void QZ_WindowBlitIndexedPixelsToView16(uint left, uint top, uint right, uint bottom)
{
	const uint16* pal = _cocoa_video_data.palette16;
	const uint8* src = _cocoa_video_data.pixels;
	uint16* dst = (uint16*)_cocoa_video_data.realpixels;
	uint width = _cocoa_video_data.width;
	uint pitch = _cocoa_video_data.pitch / 2;
	uint x;
	uint y;

	for (y = top; y < bottom; y++) {
		for (x = left; x < right; x++) {
			dst[y * pitch + x] = pal[src[y * width + x]];
		}
	}
}

static inline void QZ_WindowBlitIndexedPixelsToView(int left, int top, int right, int bottom)
{
	switch (_cocoa_video_data.device_bpp) {
		case 32: QZ_WindowBlitIndexedPixelsToView32(left, top, right, bottom); break;
		case 16: QZ_WindowBlitIndexedPixelsToView16(left, top, right, bottom); break;
	}
}

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

static void QZ_DrawResizeIcon()
{
	int xoff = _cocoa_video_data.width - 16;
	int yoff = _cocoa_video_data.height - 16;
	int x;
	int y;

	for (y = 0; y < 16; y++) {
		uint16* trg16 = (uint16*)_cocoa_video_data.realpixels + (yoff + y) * _cocoa_video_data.pitch / 2 + xoff;
		uint32* trg32 = (uint32*)_cocoa_video_data.realpixels + (yoff + y) * _cocoa_video_data.pitch / 4 + xoff;

		for (x = 0; x < 16; x++, trg16++, trg32++) {
			if (!_resize_icon[y * 16 + x]) continue;

			switch (_cocoa_video_data.device_bpp) {
				case 32: *trg32 = 0xff000000; break;
				case 16: *trg16 = 0x0000;     break;
			}
		}
	}
}

static void QZ_DrawWindow()
{
	int i;
	RgnHandle dirty, temp;

	/* Check if we need to do anything */
	if (_cocoa_video_data.num_dirty_rects == 0 ||
			[ _cocoa_video_data.window isMiniaturized ]) {
		return;
	}

	if (_cocoa_video_data.num_dirty_rects >= MAX_DIRTY_RECTS) {
		_cocoa_video_data.num_dirty_rects = 1;
		_cocoa_video_data.dirty_rects[0].left = 0;
		_cocoa_video_data.dirty_rects[0].top = 0;
		_cocoa_video_data.dirty_rects[0].right = _cocoa_video_data.width;
		_cocoa_video_data.dirty_rects[0].bottom = _cocoa_video_data.height;
	}

	dirty = NewRgn();
	temp  = NewRgn();

	SetEmptyRgn(dirty);

	/* Build the region of dirty rectangles */
	for (i = 0; i < _cocoa_video_data.num_dirty_rects; i++) {
		QZ_WindowBlitIndexedPixelsToView(
			_cocoa_video_data.dirty_rects[i].left,
			_cocoa_video_data.dirty_rects[i].top,
			_cocoa_video_data.dirty_rects[i].right,
			_cocoa_video_data.dirty_rects[i].bottom
		);

		MacSetRectRgn(
			temp,
			_cocoa_video_data.dirty_rects[i].left,
			_cocoa_video_data.dirty_rects[i].top,
			_cocoa_video_data.dirty_rects[i].right,
			_cocoa_video_data.dirty_rects[i].bottom
		);
		MacUnionRgn(dirty, temp, dirty);
	}

	QZ_DrawResizeIcon();

	/* Flush the dirty region */
	QDFlushPortBuffer( (OpaqueGrafPtr*) [ _cocoa_video_data.qdview qdPort ], dirty);
	DisposeRgn(dirty);
	DisposeRgn(temp);

	_cocoa_video_data.num_dirty_rects = 0;
}


extern const char _openttd_revision[];

static const char* QZ_SetVideoWindowed(uint width, uint height)
{
	char caption[50];
	NSString *nsscaption;
	unsigned int style;
	NSRect contentRect;
	BOOL isCustom = NO;

	if (width > _cocoa_video_data.device_width)
		width = _cocoa_video_data.device_width;
	if (height > _cocoa_video_data.device_height)
		height = _cocoa_video_data.device_height;

	_cocoa_video_data.width = width;
	_cocoa_video_data.height = height;

	contentRect = NSMakeRect(0, 0, width, height);

	/* Check if we should completely destroy the previous mode
	 * - If it is fullscreen
	 */
	if (_cocoa_video_data.isset && _cocoa_video_data.fullscreen)
		QZ_UnsetVideoMode();

	/* Check if we should recreate the window */
	if (_cocoa_video_data.window == nil) {
		/* Set the window style */
		style = NSTitledWindowMask;
		style |= (NSMiniaturizableWindowMask | NSClosableWindowMask);
		style |= NSResizableWindowMask;

		/* Manually create a window, avoids having a nib file resource */
		_cocoa_video_data.window = [ [ OTTD_QuartzWindow alloc ]
										initWithContentRect:contentRect
										styleMask:style
										backing:NSBackingStoreBuffered
										defer:NO ];

		if (_cocoa_video_data.window == nil)
			return "Could not create the Cocoa window";

		snprintf(caption, sizeof(caption), "OpenTTD %s", _openttd_revision);
		nsscaption = [ [ NSString alloc ] initWithCString:caption ];
		[ _cocoa_video_data.window setTitle:nsscaption ];
		[ _cocoa_video_data.window setMiniwindowTitle:nsscaption ];
		[ nsscaption release ];

		[ _cocoa_video_data.window setAcceptsMouseMovedEvents:YES ];
		[ _cocoa_video_data.window setViewsNeedDisplay:NO ];

		[ _cocoa_video_data.window setDelegate: [ [ [ OTTD_QuartzWindowDelegate alloc ] init ] autorelease ] ];
	} else {
		/* We already have a window, just change its size */
		if (!isCustom) {
			[ _cocoa_video_data.window setContentSize:contentRect.size ];
			[ _cocoa_video_data.qdview setFrameSize:contentRect.size ];
		}
	}

	[ _cocoa_video_data.window center ];

	/* Only recreate the view if it doesn't already exist */
	if (_cocoa_video_data.qdview == nil) {
		_cocoa_video_data.qdview = [ [ NSQuickDrawView alloc ] initWithFrame:contentRect ];
		[ _cocoa_video_data.qdview setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable ];
		[ [ _cocoa_video_data.window contentView ] addSubview:_cocoa_video_data.qdview ];
		[ _cocoa_video_data.qdview release ];
		[ _cocoa_video_data.window makeKeyAndOrderFront:nil ];
	}

	CGrafPtr thePort = (OpaqueGrafPtr*) [ _cocoa_video_data.qdview qdPort ];

	LockPortBits(thePort);
	_cocoa_video_data.realpixels = GetPixBaseAddr(GetPortPixMap(thePort));
	_cocoa_video_data.pitch = GetPixRowBytes(GetPortPixMap(thePort));
	UnlockPortBits(thePort);

	/* _cocoa_video_data.realpixels now points to the window's pixels
	 * We want it to point to the *view's* pixels
	 */
	{
		int vOffset = [ _cocoa_video_data.window frame ].size.height - [ _cocoa_video_data.qdview frame ].size.height - [ _cocoa_video_data.qdview frame ].origin.y;
		int hOffset = [ _cocoa_video_data.qdview frame ].origin.x;

		_cocoa_video_data.realpixels = (uint8*)_cocoa_video_data.realpixels + (vOffset * _cocoa_video_data.pitch) + hOffset * (_cocoa_video_data.device_bpp / 8);
	}

	free(_cocoa_video_data.pixels);
	_cocoa_video_data.pixels = (uint8*)malloc(width * height);
	if (_cocoa_video_data.pixels == NULL) return "Failed to allocate 8-bit buffer";

	_cocoa_video_data.fullscreen = false;

	return NULL;
}


/******************************************************************************
 *                             Fullscreen mode                                *
 ******************************************************************************/

/* Gamma functions to try to hide the flash from a rez switch
 * Fade the display from normal to black
 * Save gamma tables for fade back to normal
 */
static uint32 QZ_FadeGammaOut(OTTD_QuartzGammaTable* table)
{
	CGGammaValue redTable[QZ_GAMMA_TABLE_SIZE];
	CGGammaValue greenTable[QZ_GAMMA_TABLE_SIZE];
	CGGammaValue blueTable[QZ_GAMMA_TABLE_SIZE];
	float percent;
	int j;
	unsigned int actual;

	if (CGGetDisplayTransferByTable(
				_cocoa_video_data.display_id, QZ_GAMMA_TABLE_SIZE,
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
					_cocoa_video_data.display_id, QZ_GAMMA_TABLE_SIZE,
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
static uint32 QZ_FadeGammaIn(const OTTD_QuartzGammaTable* table)
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
					_cocoa_video_data.display_id, QZ_GAMMA_TABLE_SIZE,
					redTable, greenTable, blueTable
				) != CGDisplayNoErr) {
			CGDisplayRestoreColorSyncSettings();
			return 1;
		}

		CSleep(10);
	}

	return 0;
}

static const char* QZ_SetVideoFullScreen(int width, int height)
{
	const char* errstr = "QZ_SetVideoFullScreen error";
	int exact_match;
	CFNumberRef number;
	int bpp;
	int gamma_error;
	OTTD_QuartzGammaTable gamma_table;
	NSRect screen_rect;
	CGError error;
	NSPoint pt;

	/* Destroy any previous mode */
	if (_cocoa_video_data.isset) QZ_UnsetVideoMode();

	/* See if requested mode exists */
	_cocoa_video_data.mode = CGDisplayBestModeForParameters(_cocoa_video_data.display_id, 8, width, height, &exact_match);

	/* If the mode wasn't an exact match, check if it has the right bpp, and update width and height */
	if (!exact_match) {
		number = (const __CFNumber*) CFDictionaryGetValue(_cocoa_video_data.mode, kCGDisplayBitsPerPixel);
		CFNumberGetValue(number, kCFNumberSInt32Type, &bpp);
		if (bpp != 8) {
			errstr = "Failed to find display resolution";
			goto ERR_NO_MATCH;
		}

		number = (const __CFNumber*)CFDictionaryGetValue(_cocoa_video_data.mode, kCGDisplayWidth);
		CFNumberGetValue(number, kCFNumberSInt32Type, &width);

		number = (const __CFNumber*)CFDictionaryGetValue(_cocoa_video_data.mode, kCGDisplayHeight);
		CFNumberGetValue(number, kCFNumberSInt32Type, &height);
	}

	/* Fade display to zero gamma */
	gamma_error = QZ_FadeGammaOut(&gamma_table);

	/* Put up the blanking window (a window above all other windows) */
	error = CGDisplayCapture(_cocoa_video_data.display_id);

	if (CGDisplayNoErr != error) {
		errstr = "Failed capturing display";
		goto ERR_NO_CAPTURE;
	}

	/* Do the physical switch */
	if (CGDisplaySwitchToMode(_cocoa_video_data.display_id, _cocoa_video_data.mode) != CGDisplayNoErr) {
		errstr = "Failed switching display resolution";
		goto ERR_NO_SWITCH;
	}

	_cocoa_video_data.realpixels = (uint8*)CGDisplayBaseAddress(_cocoa_video_data.display_id);
	_cocoa_video_data.pitch  = CGDisplayBytesPerRow(_cocoa_video_data.display_id);

	_cocoa_video_data.width = CGDisplayPixelsWide(_cocoa_video_data.display_id);
	_cocoa_video_data.height = CGDisplayPixelsHigh(_cocoa_video_data.display_id);
	_cocoa_video_data.fullscreen = true;

	/* Setup double-buffer emulation */
	_cocoa_video_data.pixels = (uint8*)malloc(width * height);
	if (_cocoa_video_data.pixels == NULL) {
		errstr = "Failed to allocate memory for double buffering";
		goto ERR_DOUBLEBUF;
	}

	if (!CGDisplayCanSetPalette(_cocoa_video_data.display_id)) {
		errstr = "Not an indexed display mode.";
		goto ERR_NOT_INDEXED;
	}

	/* If we don't hide menu bar, it will get events and interrupt the program */
	HideMenuBar();

	/* Fade the display to original gamma */
	if (!gamma_error) QZ_FadeGammaIn(&gamma_table);

	/* There is a bug in Cocoa where NSScreen doesn't synchronize
	 * with CGDirectDisplay, so the main screen's frame is wrong.
	 * As a result, coordinate translation produces incorrect results.
	 * We can hack around this bug by setting the screen rect ourselves.
	 * This hack should be removed if/when the bug is fixed.
	 */
	screen_rect = NSMakeRect(0, 0, width, height);
	[ [ NSScreen mainScreen ] setFrame:screen_rect ];

	/* we're fullscreen, so flag all input states... */
	_cocoa_video_data.active = true;


	pt = [ NSEvent mouseLocation ];
	pt.y = CGDisplayPixelsHigh(_cocoa_video_data.display_id) - pt.y;
	if (QZ_MouseIsInsideView(&pt)) QZ_HideMouse();

	return NULL;

/* Since the blanking window covers *all* windows (even force quit) correct recovery is crucial */
ERR_NOT_INDEXED:
	free(_cocoa_video_data.pixels);
	_cocoa_video_data.pixels = NULL;
ERR_DOUBLEBUF:
	CGDisplaySwitchToMode(_cocoa_video_data.display_id, _cocoa_video_data.save_mode);
ERR_NO_SWITCH:
	CGReleaseAllDisplays();
ERR_NO_CAPTURE:
	if (!gamma_error) QZ_FadeGammaIn(&gamma_table);
ERR_NO_MATCH:
	return errstr;
}


static void QZ_UpdateFullscreenPalette(uint first_color, uint num_colors)
{
	CGTableCount  index;
	CGDeviceColor color;

	for (index = first_color; index < first_color+num_colors; index++) {
		/* Clamp colors between 0.0 and 1.0 */
		color.red   = _cur_palette[index].r / 255.0;
		color.blue  = _cur_palette[index].b / 255.0;
		color.green = _cur_palette[index].g / 255.0;

		CGPaletteSetColorAtIndex(_cocoa_video_data.palette, color, index);
	}

	CGDisplaySetPalette(_cocoa_video_data.display_id, _cocoa_video_data.palette);
}

/* Wait for the VBL to occur (estimated since we don't have a hardware interrupt) */
static void QZ_WaitForVerticalBlank()
{
	/* The VBL delay is based on Ian Ollmann's RezLib <iano@cco.caltech.edu> */
	double refreshRate;
	double linesPerSecond;
	double target;
	double position;
	double adjustment;
	CFNumberRef refreshRateCFNumber;

	refreshRateCFNumber = (const __CFNumber*)CFDictionaryGetValue(_cocoa_video_data.mode, kCGDisplayRefreshRate);
	if (refreshRateCFNumber == NULL) return;

	if (CFNumberGetValue(refreshRateCFNumber, kCFNumberDoubleType, &refreshRate) == 0)
		return;

	if (refreshRate == 0) return;

	linesPerSecond = refreshRate * _cocoa_video_data.height;
	target = _cocoa_video_data.height;

	/* Figure out the first delay so we start off about right */
	position = CGDisplayBeamPosition(_cocoa_video_data.display_id);
	if (position > target) position = 0;

	adjustment = (target - position) / linesPerSecond;

	CSleep((uint32)(adjustment * 1000));
}


static void QZ_DrawScreen()
{
	const uint8* src = _cocoa_video_data.pixels;
	uint8* dst       = (uint8*)_cocoa_video_data.realpixels;
	uint pitch       = _cocoa_video_data.pitch;
	uint width       = _cocoa_video_data.width;
	uint num_dirty   = _cocoa_video_data.num_dirty_rects;
	uint i;

	/* Check if we need to do anything */
	if (num_dirty == 0) return;

	if (num_dirty >= MAX_DIRTY_RECTS) {
		num_dirty = 1;
		_cocoa_video_data.dirty_rects[0].left   = 0;
		_cocoa_video_data.dirty_rects[0].top    = 0;
		_cocoa_video_data.dirty_rects[0].right  = _cocoa_video_data.width;
		_cocoa_video_data.dirty_rects[0].bottom = _cocoa_video_data.height;
	}

	QZ_WaitForVerticalBlank();
	/* Build the region of dirty rectangles */
	for (i = 0; i < num_dirty; i++) {
		uint y      = _cocoa_video_data.dirty_rects[i].top;
		uint left   = _cocoa_video_data.dirty_rects[i].left;
		uint length = _cocoa_video_data.dirty_rects[i].right - left;
		uint bottom = _cocoa_video_data.dirty_rects[i].bottom;

		for (; y < bottom; y++) {
			memcpy(dst + y * pitch + left, src + y * width + left, length);
		}
	}

	_cocoa_video_data.num_dirty_rects = 0;
}


static int QZ_ListFullscreenModes(OTTDPoint* mode_list, int max_modes)
{
	CFIndex num_modes;
	CFIndex i;
	int list_size = 0;

	num_modes = CFArrayGetCount(_cocoa_video_data.mode_list);

	/* Build list of modes with the requested bpp */
	for (i = 0; i < num_modes && list_size < max_modes; i++) {
		CFDictionaryRef onemode;
		CFNumberRef     number;
		int bpp;
		int intvalue;
		bool hasMode;
		uint16 width, height;

		onemode = (const __CFDictionary*)CFArrayGetValueAtIndex(_cocoa_video_data.mode_list, i);
		number = (const __CFNumber*)CFDictionaryGetValue(onemode, kCGDisplayBitsPerPixel);
		CFNumberGetValue (number, kCFNumberSInt32Type, &bpp);

		if (bpp != 8) continue;

		number = (const __CFNumber*)CFDictionaryGetValue(onemode, kCGDisplayWidth);
		CFNumberGetValue(number, kCFNumberSInt32Type, &intvalue);
		width = (uint16)intvalue;

		number = (const __CFNumber*)CFDictionaryGetValue(onemode, kCGDisplayHeight);
		CFNumberGetValue(number, kCFNumberSInt32Type, &intvalue);
		height = (uint16)intvalue;

		/* Check if mode is already in the list */
		{
			int i;
			hasMode = false;
			for (i = 0; i < list_size; i++) {
				if (mode_list[i].x == width &&  mode_list[i].y == height) {
					hasMode = true;
					break;
				}
			}
		}

		if (hasMode) continue;

		/* Add mode to the list */
		mode_list[list_size].x = width;
		mode_list[list_size].y = height;
		list_size++;
	}

	/* Sort list smallest to largest */
	{
		int i, j;
		for (i = 0; i < list_size; i++) {
			for (j = 0; j < list_size-1; j++) {
				if (mode_list[j].x > mode_list[j + 1].x || (
							mode_list[j].x == mode_list[j + 1].x &&
							mode_list[j].y >  mode_list[j + 1].y
						)) {
					uint tmpw = mode_list[j].x;
					uint tmph = mode_list[j].y;

					mode_list[j].x = mode_list[j + 1].x;
					mode_list[j].y = mode_list[j + 1].y;

					mode_list[j + 1].x = tmpw;
					mode_list[j + 1].y = tmph;
				}
			}
		}
	}

	return list_size;
}


/******************************************************************************
 *                             Windowed and fullscreen common code            *
 ******************************************************************************/

static void QZ_UpdatePalette(uint start, uint count)
{
	if (_cocoa_video_data.fullscreen) {
		QZ_UpdateFullscreenPalette(start, count);
	} else {
		QZ_UpdateWindowPalette(start, count);
	}
}

static void QZ_InitPalette()
{
	QZ_UpdatePalette(0, 256);
}

static void QZ_Draw()
{
	if (_cocoa_video_data.fullscreen) {
		QZ_DrawScreen();
	} else {
		QZ_DrawWindow();
	}
}


static const OTTDPoint _default_resolutions[] = {
	{ 640,  480},
	{ 800,  600},
	{1024,  768},
	{1152,  864},
	{1280,  800},
	{1280,  960},
	{1280, 1024},
	{1400, 1050},
	{1600, 1200},
	{1680, 1050},
	{1920, 1200}
};

static void QZ_UpdateVideoModes()
{
	uint i, j, count;
	OTTDPoint modes[32];
	const OTTDPoint *current_modes;

	if (_cocoa_video_data.fullscreen) {
		count = QZ_ListFullscreenModes(modes, 32);
		current_modes = modes;
	} else {
		count = lengthof(_default_resolutions);
		current_modes = _default_resolutions;
	}

	for (i = 0, j = 0; j < lengthof(_resolutions) && i < count; i++) {
		if (_cocoa_video_data.fullscreen || (
					(uint)current_modes[i].x < _cocoa_video_data.device_width &&
					(uint)current_modes[i].y < _cocoa_video_data.device_height)
				) {
			_resolutions[j][0] = current_modes[i].x;
			_resolutions[j][1] = current_modes[i].y;
			j++;
		}
	}

	_num_resolutions = j;
}

static void QZ_UnsetVideoMode()
{
	if (_cocoa_video_data.fullscreen) {
		/* Release fullscreen resources */
		OTTD_QuartzGammaTable gamma_table;
		int gamma_error;
		NSRect screen_rect;

		gamma_error = QZ_FadeGammaOut(&gamma_table);

		/* Restore original screen resolution/bpp */
		CGDisplaySwitchToMode(_cocoa_video_data.display_id, _cocoa_video_data.save_mode);
		CGReleaseAllDisplays();
		ShowMenuBar();
		/* Reset the main screen's rectangle
		 * See comment in QZ_SetVideoFullscreen for why we do this
		 */
		screen_rect = NSMakeRect(0,0,_cocoa_video_data.device_width,_cocoa_video_data.device_height);
		[ [ NSScreen mainScreen ] setFrame:screen_rect ];

		if (!gamma_error) QZ_FadeGammaIn(&gamma_table);
	} else {
		/* Release window mode resources */
		[ _cocoa_video_data.window close ];
		_cocoa_video_data.window = nil;
		_cocoa_video_data.qdview = nil;
	}

	free(_cocoa_video_data.pixels);
	_cocoa_video_data.pixels = NULL;

	/* Signal successful teardown */
	_cocoa_video_data.isset = false;

	QZ_ShowMouse();
}


static const char* QZ_SetVideoMode(uint width, uint height, bool fullscreen)
{
	const char *ret;

	_cocoa_video_data.issetting = true;
	if (fullscreen) {
		/* Setup full screen video */
		ret = QZ_SetVideoFullScreen(width, height);
	} else {
		/* Setup windowed video */
		ret = QZ_SetVideoWindowed(width, height);
	}
	_cocoa_video_data.issetting = false;
	if (ret != NULL) return ret;

	/* Signal successful completion (used internally) */
	_cocoa_video_data.isset = true;

	/* Tell the game that the resolution has changed */
	_screen.width = _cocoa_video_data.width;
	_screen.height = _cocoa_video_data.height;
	_screen.pitch = _cocoa_video_data.width;

	QZ_UpdateVideoModes();
	GameSizeChanged();

	QZ_InitPalette();

	return NULL;
}

static const char* QZ_SetVideoModeAndRestoreOnFailure(uint width, uint height, bool fullscreen)
{
	bool wasset = _cocoa_video_data.isset;
	uint32 oldwidth = _cocoa_video_data.width;
	uint32 oldheight = _cocoa_video_data.height;
	bool oldfullscreen = _cocoa_video_data.fullscreen;
	const char *ret;

	ret = QZ_SetVideoMode(width, height, fullscreen);
	if (ret != NULL && wasset) QZ_SetVideoMode(oldwidth, oldheight, oldfullscreen);

	return ret;
}

static void QZ_VideoInit()
{
	if (BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth() == 0) error("Can't use a blitter that blits 0 bpp for normal visuals");

	memset(&_cocoa_video_data, 0, sizeof(_cocoa_video_data));

	/* Initialize the video settings; this data persists between mode switches */
	_cocoa_video_data.display_id = kCGDirectMainDisplay;
	_cocoa_video_data.save_mode  = CGDisplayCurrentMode(_cocoa_video_data.display_id);
	_cocoa_video_data.mode_list  = CGDisplayAvailableModes(_cocoa_video_data.display_id);
	_cocoa_video_data.palette    = CGPaletteCreateDefaultColorPalette();

	/* Gather some information that is useful to know about the display */
	/* Maybe this should be moved to QZ_SetVideoMode, in case this is changed after startup */
	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(_cocoa_video_data.save_mode, kCGDisplayBitsPerPixel),
		kCFNumberSInt32Type, &_cocoa_video_data.device_bpp
	);

	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(_cocoa_video_data.save_mode, kCGDisplayWidth),
		kCFNumberSInt32Type, &_cocoa_video_data.device_width
	);

	CFNumberGetValue(
		(const __CFNumber*)CFDictionaryGetValue(_cocoa_video_data.save_mode, kCGDisplayHeight),
		kCFNumberSInt32Type, &_cocoa_video_data.device_height
	);

	_cocoa_video_data.cursor_visible = true;

	/* register for sleep notifications so wake from sleep generates SDL_VIDEOEXPOSE */
//	QZ_RegisterForSleepNotifications();
}


/* Convert local coordinate to window server (CoreGraphics) coordinate */
static CGPoint QZ_PrivateLocalToCG(NSPoint* p)
{
	CGPoint cgp;

	if (!_cocoa_video_data.fullscreen) {
		*p = [ _cocoa_video_data.qdview convertPoint:*p toView: nil ];
		*p = [ _cocoa_video_data.window convertBaseToScreen:*p ];
		p->y = _cocoa_video_data.device_height - p->y;
	}

	cgp.x = p->x;
	cgp.y = p->y;

	return cgp;
}

static void QZ_WarpCursor(int x, int y)
{
	NSPoint p;
	CGPoint cgp;

	/* Only allow warping when in foreground */
	if (![ NSApp isActive ]) return;

	p = NSMakePoint(x, y);
	cgp = QZ_PrivateLocalToCG(&p);

	/* this is the magic call that fixes cursor "freezing" after warp */
	CGSetLocalEventsSuppressionInterval(0.0);
	/* Do the actual warp */
	CGWarpMouseCursorPosition(cgp);

	/* Generate the mouse moved event */
}

static void QZ_ShowMouse()
{
	if (!_cocoa_video_data.cursor_visible) {
		[ NSCursor unhide ];
		_cocoa_video_data.cursor_visible = true;

		// Hide the openttd cursor when leaving the window
		if (_cocoa_video_data.isset)
			UndrawMouseCursor();
		_cursor.in_window = false;
	}
}

static void QZ_HideMouse()
{
	if (_cocoa_video_data.cursor_visible) {
#ifndef _DEBUG
		[ NSCursor hide ];
#endif
		_cocoa_video_data.cursor_visible = false;

		// Show the openttd cursor again
		_cursor.in_window = true;
	}
}


/******************************************************************************
 *                             OS X application creation                      *
 ******************************************************************************/

/* The main class of the application, the application's delegate */
@implementation OTTDMain
/* Called when the internal event loop has just started running */
- (void) applicationDidFinishLaunching: (NSNotification*) note
{
	/* Hand off to main application code */
	QZ_GameLoop();

	/* We're done, thank you for playing */
	[ NSApp stop:_ottd_main ];
}

/* Display the in game quit confirmation dialog */
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*) sender
{

	HandleExitGameRequest();

	return NSTerminateCancel; // NSTerminateLater ?
}
@end

static void setApplicationMenu()
{
	/* warning: this code is very odd */
	NSMenu *appleMenu;
	NSMenuItem *menuItem;
	NSString *title;
	NSString *appName;

	appName = @"OTTD";
	appleMenu = [[NSMenu alloc] initWithTitle:appName];

	/* Add menu items */
	title = [@"About " stringByAppendingString:appName];
	[appleMenu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

	[appleMenu addItem:[NSMenuItem separatorItem]];

	title = [@"Hide " stringByAppendingString:appName];
	[appleMenu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

	menuItem = (NSMenuItem*)[appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	[menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

	[appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

	[appleMenu addItem:[NSMenuItem separatorItem]];

	title = [@"Quit " stringByAppendingString:appName];
	[appleMenu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];


	/* Put menu into the menubar */
	menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	[menuItem setSubmenu:appleMenu];
	[[NSApp mainMenu] addItem:menuItem];

	/* Tell the application object that this is now the application menu */
	[NSApp setAppleMenu:appleMenu];

	/* Finally give up our references to the objects */
	[appleMenu release];
	[menuItem release];
}

/* Create a window menu */
static void setupWindowMenu()
{
	NSMenu* windowMenu;
	NSMenuItem* windowMenuItem;
	NSMenuItem* menuItem;

	windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

	/* "Minimize" item */
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
	[windowMenu addItem:menuItem];
	[menuItem release];

	/* Put menu into the menubar */
	windowMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
	[windowMenuItem setSubmenu:windowMenu];
	[[NSApp mainMenu] addItem:windowMenuItem];

	/* Tell the application object that this is now the window menu */
	[NSApp setWindowsMenu:windowMenu];

	/* Finally give up our references to the objects */
	[windowMenu release];
	[windowMenuItem release];
}

static void setupApplication()
{
	CPSProcessSerNum PSN;

	/* Ensure the application object is initialised */
	[NSApplication sharedApplication];

	/* Tell the dock about us */
	if (!CPSGetCurrentProcess(&PSN) &&
			!CPSEnableForegroundOperation(&PSN, 0x03, 0x3C, 0x2C, 0x1103) &&
			!CPSSetFrontProcess(&PSN)) {
		[NSApplication sharedApplication];
	}

	/* Set up the menubar */
	[NSApp setMainMenu:[[NSMenu alloc] init]];
	setApplicationMenu();
	setupWindowMenu();

	/* Create OTTDMain and make it the app delegate */
	_ottd_main = [[OTTDMain alloc] init];
	[NSApp setDelegate:_ottd_main];
}


/******************************************************************************
 *                             Video driver interface                         *
 ******************************************************************************/

static void CocoaVideoStop()
{
	if (!_cocoa_video_started) return;

	if (_cocoa_video_data.isset) QZ_UnsetVideoMode();

	[_ottd_main release];

	_cocoa_video_started = false;
}

static const char *CocoaVideoStart(const char * const *parm)
{
	const char *ret;

	if (_cocoa_video_started) return "Already started";
	_cocoa_video_started = true;

	memset(&_cocoa_video_data, 0, sizeof(_cocoa_video_data));

	setupApplication();

	/* Don't create a window or enter fullscreen if we're just going to show a dialog. */
	if (_cocoa_video_dialog) return NULL;

	QZ_VideoInit();

	ret = QZ_SetVideoMode(_cur_resolution[0], _cur_resolution[1], _fullscreen);
	if (ret != NULL) CocoaVideoStop();

	return ret;
}

static void CocoaVideoMakeDirty(int left, int top, int width, int height)
{
	if (_cocoa_video_data.num_dirty_rects < MAX_DIRTY_RECTS) {
		_cocoa_video_data.dirty_rects[_cocoa_video_data.num_dirty_rects].left = left;
		_cocoa_video_data.dirty_rects[_cocoa_video_data.num_dirty_rects].top = top;
		_cocoa_video_data.dirty_rects[_cocoa_video_data.num_dirty_rects].right = left + width;
		_cocoa_video_data.dirty_rects[_cocoa_video_data.num_dirty_rects].bottom = top + height;
	}
	_cocoa_video_data.num_dirty_rects++;
}

static void CocoaVideoMainLoop()
{
	/* Start the main event loop */
	[NSApp run];
}

static bool CocoaVideoChangeRes(int w, int h)
{
	const char *ret = QZ_SetVideoModeAndRestoreOnFailure((uint)w, (uint)h, _cocoa_video_data.fullscreen);
	if (ret != NULL) {
		DEBUG(driver, 0, "cocoa_v: CocoaVideoChangeRes failed with message: %s", ret);
	}

	return ret == NULL;
}

static void CocoaVideoFullScreen(bool full_screen)
{
	const char *ret = QZ_SetVideoModeAndRestoreOnFailure(_cocoa_video_data.width, _cocoa_video_data.height, full_screen);
	if (ret != NULL) {
		DEBUG(driver, 0, "cocoa_v: CocoaVideoFullScreen failed with message: %s", ret);
	}

	_fullscreen = _cocoa_video_data.fullscreen;
}

const HalVideoDriver _cocoa_video_driver = {
	CocoaVideoStart,
	CocoaVideoStop,
	CocoaVideoMakeDirty,
	CocoaVideoMainLoop,
	CocoaVideoChangeRes,
	CocoaVideoFullScreen,
};


/* This is needed since sometimes assert is called before the videodriver is initialized */
void CocoaDialog(const char* title, const char* message, const char* buttonLabel)
{
	bool wasstarted;

	_cocoa_video_dialog = true;

	wasstarted = _cocoa_video_started;
	if (!_cocoa_video_started && CocoaVideoStart(NULL) != NULL) {
		fprintf(stderr, "%s: %s\n", title, message);
		return;
	}

	NSRunAlertPanel([NSString stringWithCString: title], [NSString stringWithCString: message], [NSString stringWithCString: buttonLabel], nil, nil);

	if (!wasstarted) CocoaVideoStop();

	_cocoa_video_dialog = false;
}

/* This is needed since OS X application bundles do not have a
 * current directory and the data files are 'somewhere' in the bundle */
void cocoaSetApplicationBundleDir()
{
	char tmp[MAXPATHLEN];
	CFURLRef url = CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());
	if (CFURLGetFileSystemRepresentation(url, true, (unsigned char*)tmp, MAXPATHLEN)) {
		AppendPathSeparator(tmp, lengthof(tmp));
		_searchpaths[SP_APPLICATION_BUNDLE_DIR] = strdup(tmp);
	} else {
		_searchpaths[SP_APPLICATION_BUNDLE_DIR] = NULL;
	}

	CFRelease(url);
}

/* These are called from main() to prevent a _NSAutoreleaseNoPool error when
 * exiting before the cocoa video driver has been loaded
 */
void cocoaSetupAutoreleasePool()
{
	_ottd_autorelease_pool = [[NSAutoreleasePool alloc] init];
}

void cocoaReleaseAutoreleasePool()
{
	[_ottd_autorelease_pool release];
}

#endif /* WITH_COCOA */
