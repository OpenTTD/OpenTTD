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

#include "../../stdafx.h"
#include "../../os/macosx/macos.h"

#define Rect  OTTDRect
#define Point OTTDPoint
#import <Cocoa/Cocoa.h>
#undef Rect
#undef Point

#include "../../openttd.h"
#include "../../debug.h"
#include "cocoa_v.h"
#include "cocoa_wnd.h"
#include "../../settings_type.h"
#include "../../string_func.h"
#include "../../gfx_func.h"
#include "../../window_func.h"
#include "../../window_gui.h"
#include "../../spritecache.h"
#include "../../textbuf_type.h"
#include "../../toolbar_gui.h"

#include "table/sprites.h"

/* Table data for key mapping. */
#include "cocoa_keys.h"

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

#ifdef HAVE_TOUCHBAR_SUPPORT
struct TouchBarButton {
	NSTouchBarItemIdentifier key;
	SpriteID                 sprite;
	MainToolbarHotkeys       hotkey;
	NSString                *fallback_text;

	bool operator ==(const NSTouchBarItemIdentifier other) const { return this->key == other; }
};

/* 9 items can be displayed on the touch bar when using default buttons. */
static const std::array<TouchBarButton, 9> _touchbar_buttons{{
	{ @"openttd.pause",         SPR_IMG_PAUSE,       MTHK_PAUSE,         @"Pause" },
	{ @"openttd.fastforward",   SPR_IMG_FASTFORWARD, MTHK_FASTFORWARD,   @"Fast Forward" },
	{ @"openttd.zoom_in",       SPR_IMG_ZOOMIN,      MTHK_ZOOM_IN,       @"Zoom In" },
	{ @"openttd.zoom_out",      SPR_IMG_ZOOMOUT,     MTHK_ZOOM_OUT,      @"Zoom Out" },
	{ @"openttd.build_rail",    SPR_IMG_BUILDRAIL,   MTHK_BUILD_RAIL,    @"Rail" },
	{ @"openttd.build_road",    SPR_IMG_BUILDROAD,   MTHK_BUILD_ROAD,    @"Road" },
	{ @"openttd.build_tram",    SPR_IMG_BUILDTRAMS,  MTHK_BUILD_TRAM,    @"Tram" },
	{ @"openttd.build_docks",   SPR_IMG_BUILDWATER,  MTHK_BUILD_DOCKS,   @"Docks" },
	{ @"openttd.build_airport", SPR_IMG_BUILDAIR,    MTHK_BUILD_AIRPORT, @"Airport" }
}};

#endif

bool _allow_hidpi_window = true; // Referenced from table/misc_settings.ini

@interface OTTDMain : NSObject <NSApplicationDelegate>
@end

NSString *OTTDMainLaunchGameEngine = @"ottdmain_launch_game_engine";

bool _tab_is_down;

static bool _cocoa_video_dialog = false;
static OTTDMain *_ottd_main;


/**
 * Count the number of UTF-16 code points in a range of an UTF-8 string.
 * @param from Start of the range.
 * @param to End of the range.
 * @return Number of UTF-16 code points in the range.
 */
static NSUInteger CountUtf16Units(const char *from, const char *to)
{
	NSUInteger i = 0;

	while (from < to) {
		char32_t c;
		size_t len = Utf8Decode(&c, from);
		i += len < 4 ? 1 : 2; // Watch for surrogate pairs.
		from += len;
	}

	return i;
}

/**
 * Advance an UTF-8 string by a number of equivalent UTF-16 code points.
 * @param str UTF-8 string.
 * @param count Number of UTF-16 code points to advance the string by.
 * @return Advanced string pointer.
 */
static const char *Utf8AdvanceByUtf16Units(const char *str, NSUInteger count)
{
	for (NSUInteger i = 0; i < count && *str != '\0'; ) {
		char32_t c;
		size_t len = Utf8Decode(&c, str);
		i += len < 4 ? 1 : 2; // Watch for surrogates.
		str += len;
	}

	return str;
}

/**
 * Convert a NSString to an UTF-32 encoded string.
 * @param s String to convert.
 * @return Vector of UTF-32 characters.
 */
static std::vector<char32_t> NSStringToUTF32(NSString *s)
{
	std::vector<char32_t> unicode_str;

	unichar lead = 0;
	for (NSUInteger i = 0; i < s.length; i++) {
		unichar c = [ s characterAtIndex:i ];
		if (Utf16IsLeadSurrogate(c)) {
			lead = c;
			continue;
		} else if (Utf16IsTrailSurrogate(c)) {
			if (lead != 0) unicode_str.push_back(Utf16DecodeSurrogate(lead, c));
		} else {
			unicode_str.push_back(c);
		}
	}

	return unicode_str;
}

static void CGDataFreeCallback(void *, const void *data, size_t)
{
	delete[] (const uint32_t *)data;
}

/**
 * Render an OTTD sprite to a Cocoa image.
 * @param sprite_id Sprite to make a NSImage from.
 * @param zoom Zoom level to render the sprite in.
 * @return Autorelease'd image or nullptr on any error.
 */
static NSImage *NSImageFromSprite(SpriteID sprite_id, ZoomLevel zoom)
{
	if (!SpriteExists(sprite_id)) return nullptr;

	/* Fetch the sprite and create a new bitmap */
	Dimension dim = GetSpriteSize(sprite_id, nullptr, zoom);
	std::unique_ptr<uint32_t[]> buffer = DrawSpriteToRgbaBuffer(sprite_id, zoom);
	if (!buffer) return nullptr; // Failed to blit sprite for some reason.

	CFAutoRelease<CGDataProvider> data(CGDataProviderCreateWithData(nullptr, buffer.release(), dim.width * dim.height * 4, &CGDataFreeCallback));
	if (!data) return nullptr;

	CGBitmapInfo info = kCGImageAlphaFirst | kCGBitmapByteOrder32Host;
	CFAutoRelease<CGColorSpaceRef> color_space(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
	CFAutoRelease<CGImage> bitmap(CGImageCreate(dim.width, dim.height, 8, 32, dim.width * 4, color_space.get(), info, data.get(), nullptr, false, kCGRenderingIntentDefault));
	if (!bitmap) return nullptr;

	return [ [ [ NSImage alloc ] initWithCGImage:bitmap.get() size:NSZeroSize ] autorelease ];
}


/**
 * The main class of the application, the application's delegate.
 */
@implementation OTTDMain
/**
 * Stop the game engine. Must be called on main thread.
 */
- (void)stopEngine
{
	[ NSApp stop:self ];

	/* Send an empty event to return from the run loop. Without that, application is stuck waiting for an event. */
#ifdef HAVE_OSX_1012_SDK
	NSEventType type = NSEventTypeApplicationDefined;
#else
	NSEventType type = NSApplicationDefined;
#endif
	NSEvent *event = [ NSEvent otherEventWithType:type location:NSMakePoint(0, 0) modifierFlags:0 timestamp:0.0 windowNumber:0 context:nil subtype:0 data1:0 data2:0 ];
	[ NSApp postEvent:event atStart:YES ];
}

/**
 * Start the game loop.
 */
- (void)launchGameEngine: (NSNotification*) note
{
	auto *drv = static_cast<VideoDriver_Cocoa *>(VideoDriver::GetInstance());

	/* Setup cursor for the current _game_mode. */
	NSEvent *e = [ [ NSEvent alloc ] init ];
	[ drv->cocoaview cursorUpdate:e ];
	[ e release ];

	/* Hand off to main application code. */
	drv->MainLoopReal();

	/* We are done, thank you for playing. */
	[ self performSelectorOnMainThread:@selector(stopEngine) withObject:nil waitUntilDone:FALSE ];
}

/**
 * Called when the internal event loop has just started running.
 */
- (void) applicationDidFinishLaunching: (NSNotification*) note
{
	/* Add a notification observer so we can restart the game loop later on if necessary. */
	[ [ NSNotificationCenter defaultCenter ] addObserver:self selector:@selector(launchGameEngine:) name:OTTDMainLaunchGameEngine object:nil ];

	/* Start game loop. */
	[ [ NSNotificationCenter defaultCenter ] postNotificationName:OTTDMainLaunchGameEngine object:nil ];
}

/**
 * Display the in game quit confirmation dialog.
 */
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*) sender
{
	HandleExitGameRequest();

	return NSTerminateCancel;
}

/**
 * Remove ourself as a notification observer.
 */
- (void)unregisterObserver
{
	[ [ NSNotificationCenter defaultCenter ] removeObserver:self ];
}
@end

/**
 * Initialize the application menu shown in top bar.
 */
static void setApplicationMenu()
{
	NSString *appName = @"OpenTTD";
	NSMenu *appleMenu = [ [ NSMenu alloc ] initWithTitle:appName ];

	/* Add menu items */
	NSString *title = [ @"About " stringByAppendingString:appName ];
	[ appleMenu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@"" ];

	[ appleMenu addItem:[ NSMenuItem separatorItem ] ];

	title = [ @"Hide " stringByAppendingString:appName ];
	[ appleMenu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h" ];

	NSMenuItem *menuItem = [ appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h" ];
	[ menuItem setKeyEquivalentModifierMask:(NSEventModifierFlagOption | NSEventModifierFlagCommand) ];

	[ appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@"" ];

	[ appleMenu addItem:[ NSMenuItem separatorItem ] ];

	title = [ @"Quit " stringByAppendingString:appName ];
	[ appleMenu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q" ];

	/* Put menu into the menubar */
	menuItem = [ [ NSMenuItem alloc ] initWithTitle:@"" action:nil keyEquivalent:@"" ];
	[ menuItem setSubmenu:appleMenu ];
	[ [ NSApp mainMenu ] addItem:menuItem ];

	/* Tell the application object that this is now the application menu.
	 * This interesting Objective-C construct is used because not all SDK
	 * versions define this method publicly. */
	if ([ NSApp respondsToSelector:@selector(setAppleMenu:) ]) {
		[ NSApp performSelector:@selector(setAppleMenu:) withObject:appleMenu ];
	}

	/* Finally give up our references to the objects */
	[ appleMenu release ];
	[ menuItem release ];
}

/**
 * Create a window menu.
 */
static void setupWindowMenu()
{
	NSMenu *windowMenu = [ [ NSMenu alloc ] initWithTitle:@"Window" ];

	/* "Minimize" item */
	[ windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m" ];

	/* Put menu into the menubar */
	NSMenuItem *menuItem = [ [ NSMenuItem alloc ] initWithTitle:@"Window" action:nil keyEquivalent:@"" ];
	[ menuItem setSubmenu:windowMenu ];
	[ [ NSApp mainMenu ] addItem:menuItem ];

	if (MacOSVersionIsAtLeast(10, 7, 0)) {
		/* The OS will change the name of this menu item automatically */
		[ windowMenu addItemWithTitle:@"Fullscreen" action:@selector(toggleFullScreen:) keyEquivalent:@"^f" ];
	}

	/* Tell the application object that this is now the window menu */
	[ NSApp setWindowsMenu:windowMenu ];

	/* Finally give up our references to the objects */
	[ windowMenu release ];
	[ menuItem release ];
}

/**
 * Startup the application.
 */
bool CocoaSetupApplication()
{
	ProcessSerialNumber psn = { 0, kCurrentProcess };

	/* Ensure the application object is initialised */
	[ NSApplication sharedApplication ];

	/* Tell the dock about us */
	OSStatus returnCode = TransformProcessType(&psn, kProcessTransformToForegroundApplication);
	if (returnCode != 0) Debug(driver, 0, "Could not change to foreground application. Error {}", (int)returnCode);

	/* Disable the system-wide tab feature as we only have one window. */
	if ([ NSWindow respondsToSelector:@selector(setAllowsAutomaticWindowTabbing:) ]) {
		/* We use nil instead of NO as withObject requires an id. */
		[ NSWindow performSelector:@selector(setAllowsAutomaticWindowTabbing:) withObject:nil];
	}

	/* Become the front process, important when start from the command line. */
	[ [ NSApplication sharedApplication ] setActivationPolicy:NSApplicationActivationPolicyRegular ];
	[ [ NSApplication sharedApplication ] activateIgnoringOtherApps:YES ];

	/* Set up the menubar */
	[ NSApp setMainMenu:[ [ NSMenu alloc ] init ] ];
	setApplicationMenu();
	setupWindowMenu();

	/* Create OTTDMain and make it the app delegate */
	_ottd_main = [ [ OTTDMain alloc ] init ];
	[ NSApp setDelegate:_ottd_main ];

	return true;
}

/**
 * Deregister app delegate.
 */
void CocoaExitApplication()
{
	[ _ottd_main unregisterObserver ];
	[ _ottd_main release ];
}

/**
 * Catch asserts prior to initialization of the videodriver.
 *
 * @param title Window title.
 * @param message Message text.
 * @param buttonLabel Button text.
 *
 * @note This is needed since sometimes assert is called before the videodriver is initialized .
 */
void CocoaDialog(const char *title, const char *message, const char *buttonLabel)
{
	_cocoa_video_dialog = true;

	bool wasstarted = _cocoa_video_started;
	if (VideoDriver::GetInstance() == nullptr) {
		CocoaSetupApplication(); // Setup application before showing dialog
	} else if (!_cocoa_video_started && VideoDriver::GetInstance()->Start({}) != nullptr) {
		fmt::print(stderr, "{}: {}\n", title, message);
		return;
	}

	@autoreleasepool {
		NSAlert *alert = [ [ NSAlert alloc ] init ];
#ifdef HAVE_OSX_1012_SDK
		[ alert setAlertStyle: NSAlertStyleCritical ];
#else
		[ alert setAlertStyle: NSCriticalAlertStyle ];
#endif
		[ alert setMessageText:[ NSString stringWithUTF8String:title ] ];
		[ alert setInformativeText:[ NSString stringWithUTF8String:message ] ];
		[ alert addButtonWithTitle: [ NSString stringWithUTF8String:buttonLabel ] ];
		[ alert runModal ];
		[ alert release ];
	}

	if (!wasstarted && VideoDriver::GetInstance() != nullptr) VideoDriver::GetInstance()->Stop();

	_cocoa_video_dialog = false;
}


/**
 * Re-implement the system cursor in order to allow hiding and showing it nicely
 */
@implementation NSCursor (OTTD_CocoaCursor)
+ (NSCursor *) clearCocoaCursor
{
	/* RAW 16x16 transparent GIF */
	unsigned char clearGIFBytes[] = {
		0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0xF9, 0x04, 0x01, 0x00,
		0x00, 0x01, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00,
		0x00, 0x02, 0x0E, 0x8C, 0x8F, 0xA9, 0xCB, 0xED, 0x0F, 0xA3, 0x9C, 0xB4,
		0xDA, 0x8B, 0xB3, 0x3E, 0x05, 0x00, 0x3B};
	NSData *clearGIFData = [ NSData dataWithBytesNoCopy:&clearGIFBytes[0] length:55 freeWhenDone:NO ];
	NSImage *clearImg = [ [ NSImage alloc ] initWithData:clearGIFData ];
	return [ [ NSCursor alloc ] initWithImage:clearImg hotSpot:NSMakePoint(0.0,0.0) ];
}
@end

@implementation OTTD_CocoaWindow {
	VideoDriver_Cocoa *driver;
	bool touchbar_created;
}

/**
 * Initialize event system for the application rectangle
 */
- (instancetype)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag driver:(VideoDriver_Cocoa *)drv
{
	if (self = [ super initWithContentRect:contentRect styleMask:styleMask backing:backingType defer:flag ]) {
		self->driver = drv;
		self->touchbar_created = false;

		[ self setContentMinSize:NSMakeSize(64.0f, 64.0f) ];

		std::string caption = VideoDriver::GetCaption();
		NSString *nsscaption = [ [ NSString alloc ] initWithUTF8String:caption.c_str() ];
		[ self setTitle:nsscaption ];
		[ self setMiniwindowTitle:nsscaption ];
		[ nsscaption release ];
	}

	return self;
}

/**
 * Define the rectangle we draw our window in
 */
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag
{
	[ super setFrame:frameRect display:flag ];

	driver->AllocateBackingStore();
}

#ifdef HAVE_TOUCHBAR_SUPPORT

- (void)touchBarButtonAction:(id)sender
{
	NSButton *btn = (NSButton *)sender;
	if (auto item = std::find(_touchbar_buttons.cbegin(), _touchbar_buttons.cend(), (NSTouchBarItemIdentifier)btn.identifier); item != _touchbar_buttons.cend()) {
		HandleToolbarHotkey(item->hotkey);
	}
}

- (nullable NSTouchBar *)makeTouchBar
{
	/* Make button identifier array. */
	NSMutableArray<NSTouchBarItemIdentifier> *button_ids = [ [ NSMutableArray alloc ] init ];
	for (const auto &button : _touchbar_buttons) {
		[ button_ids addObject:button.key ];
	}
	[ button_ids addObject:NSTouchBarItemIdentifierOtherItemsProxy ];

	NSTouchBar *bar = [ [ NSTouchBar alloc ] init ];
	bar.delegate = self;
	bar.defaultItemIdentifiers = button_ids;
	[ button_ids release ];

	self->touchbar_created = true;

	return bar;
}

- (nullable NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier
{
	auto item = std::find(_touchbar_buttons.cbegin(), _touchbar_buttons.cend(), identifier);
	assert(item != _touchbar_buttons.cend());

	NSButton *button = [ NSButton buttonWithTitle:item->fallback_text target:self action:@selector(touchBarButtonAction:) ];
	button.identifier = identifier;
	button.imageScaling = NSImageScaleProportionallyDown;

	NSCustomTouchBarItem *tb_item = [ [ NSCustomTouchBarItem alloc] initWithIdentifier:identifier ];
	tb_item.view = button;
	return tb_item;
}

#endif /* HAVE_TOUCHBAR_SUPPORT */

- (void)refreshSystemSprites
{
#ifdef HAVE_TOUCHBAR_SUPPORT
	if (!self->touchbar_created || ![ self respondsToSelector:@selector(touchBar) ] || self.touchBar == nil) return;

	/* Re-create button images from OTTD sprites. */
	for (NSTouchBarItemIdentifier ident in self.touchBar.itemIdentifiers) {
		auto item = std::find(_touchbar_buttons.cbegin(), _touchbar_buttons.cend(), ident);
		if (item == _touchbar_buttons.cend()) continue;

		NSCustomTouchBarItem *tb_item = [ self.touchBar itemForIdentifier:ident ];
		NSButton *button = tb_item.view;

		NSImage *image = NSImageFromSprite(item->sprite, _settings_client.gui.zoom_min);
		if (image != nil) {
			/* Human Interface Guidelines: Maximum touch bar glyph size 22 pt. */
			CGFloat max_dim = std::max(image.size.width, image.size.height);
			if (max_dim > 0.0) {
				CGFloat scale = 22.0 / max_dim;
				image.size = NSMakeSize(image.size.width * scale, image.size.height * scale);
			}

			button.image = image;
			button.imagePosition = NSImageOnly;
		} else {
			button.image = nil;
			button.imagePosition = NSNoImage;
		}
	}
#endif /* HAVE_TOUCHBAR_SUPPORT */
}

@end

@implementation OTTD_CocoaView {
	float _current_magnification;
	NSUInteger _current_mods;
	bool _emulated_down;
	bool _use_hidpi; ///< Render content in native resolution?
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
	if (self = [ super initWithFrame:frameRect ]) {
		self->_use_hidpi = _allow_hidpi_window && [ self respondsToSelector:@selector(convertRectToBacking:) ] && [ self respondsToSelector:@selector(convertRectFromBacking:) ];
	}
	return self;
}

- (NSRect)getRealRect:(NSRect)rect
{
	return _use_hidpi ? [ self convertRectToBacking:rect ] : rect;
}

- (NSRect)getVirtualRect:(NSRect)rect
{
	return _use_hidpi ? [ self convertRectFromBacking:rect ] : rect;
}

- (CGFloat)getContentsScale
{
	return _use_hidpi && self.window != nil ? [ self.window backingScaleFactor ] : 1.0f;
}

/**
 * Allow to handle events
 */
- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)setNeedsDisplayInRect:(NSRect)invalidRect
{
	/* Drawing is handled by our sub-views. Just pass it along. */
	for ( NSView *v in [ self subviews ]) {
		[ v setNeedsDisplayInRect:[ v convertRect:invalidRect fromView:self ] ];
	}
}

/** Update mouse cursor to use for this view. */
- (void)cursorUpdate:(NSEvent *)event
{
	[ (_game_mode == GM_BOOTSTRAP ? [ NSCursor arrowCursor ] : [ NSCursor clearCocoaCursor ]) set ];
}

- (void)viewWillMoveToWindow:(NSWindow *)win
{
	for (NSTrackingArea *a in [ self trackingAreas ]) {
		[ self removeTrackingArea:a ];
	}
}

- (void)viewDidMoveToWindow
{
	/* Install mouse tracking area. */
	NSTrackingAreaOptions track_opt = NSTrackingInVisibleRect | NSTrackingActiveInActiveApp | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingCursorUpdate;
	NSTrackingArea *track = [ [ NSTrackingArea alloc ] initWithRect:[ self bounds ] options:track_opt owner:self userInfo:nil ];
	[ self addTrackingArea:track ];
	[ track release ];
}
/**
 * Make OpenTTD aware that it has control over the mouse
 */
- (void)mouseEntered:(NSEvent *)theEvent
{
	_cursor.in_window = true;
}
/**
 * Make OpenTTD aware that it has NOT control over the mouse
 */
- (void)mouseExited:(NSEvent *)theEvent
{
	if ([ self window ] != nil) UndrawMouseCursor();
	_cursor.in_window = false;
}

/**
 * Return the mouse location
 * @param event UI event
 * @return mouse location as NSPoint
 */
- (NSPoint)mousePositionFromEvent:(NSEvent *)e
{
	NSPoint pt = e.locationInWindow;
	if ([ e window ] == nil) pt = [ self.window convertRectFromScreen:NSMakeRect(pt.x, pt.y, 0, 0) ].origin;
	pt = [ self convertPoint:pt fromView:nil ];

	return [ self getRealRect:NSMakeRect(pt.x, self.bounds.size.height - pt.y, 0, 0) ].origin;
}

- (void)internalMouseMoveEvent:(NSEvent *)event
{
	if (_cursor.fix_at) {
		_cursor.UpdateCursorPositionRelative(event.deltaX * self.getContentsScale, event.deltaY * self.getContentsScale);
	} else {
		NSPoint pt = [ self mousePositionFromEvent:event ];
		_cursor.UpdateCursorPosition(pt.x, pt.y);
	}

	HandleMouseEvents();
}

- (void)internalMouseButtonEvent
{
	bool cur_fix = _cursor.fix_at;
	HandleMouseEvents();

	/* Cursor fix mode was changed, synchronize with OS. */
	if (cur_fix != _cursor.fix_at) CGAssociateMouseAndMouseCursorPosition(!_cursor.fix_at);
}

- (BOOL)emulateRightButton:(NSEvent *)event
{
	uint32_t keymask = 0;
	if (_settings_client.gui.right_mouse_btn_emulation == RMBE_COMMAND) keymask |= NSEventModifierFlagCommand;
	if (_settings_client.gui.right_mouse_btn_emulation == RMBE_CONTROL) keymask |= NSEventModifierFlagControl;

	return (event.modifierFlags & keymask) != 0;
}

- (void)mouseMoved:(NSEvent *)event
{
	[ self internalMouseMoveEvent:event ];
}

- (void)mouseDragged:(NSEvent *)event
{
	[ self internalMouseMoveEvent:event ];
}
- (void)mouseDown:(NSEvent *)event
{
	if ([ self emulateRightButton:event ]) {
		self->_emulated_down = true;
		[ self rightMouseDown:event ];
	} else {
		_left_button_down = true;
		[ self internalMouseButtonEvent ];
	}
}
- (void)mouseUp:(NSEvent *)event
{
	if (self->_emulated_down) {
		self->_emulated_down = false;
		[ self rightMouseUp:event ];
	} else {
		_left_button_down = false;
		_left_button_clicked = false;
		[ self internalMouseButtonEvent ];
	}
}

- (void)rightMouseDragged:(NSEvent *)event
{
	[ self internalMouseMoveEvent:event ];
}
- (void)rightMouseDown:(NSEvent *)event
{
	_right_button_down = true;
	_right_button_clicked = true;
	[ self internalMouseButtonEvent ];
}
- (void)rightMouseUp:(NSEvent *)event
{
	_right_button_down = false;
	[ self internalMouseButtonEvent ];
}

- (void)scrollWheel:(NSEvent *)event
{
	if ([ event deltaY ] > 0.0) { /* Scroll up */
		_cursor.wheel--;
	} else if ([ event deltaY ] < 0.0) { /* Scroll down */
		_cursor.wheel++;
	} /* else: deltaY was 0.0 and we don't want to do anything */

	/* Update the scroll count for 2D scrolling */
	CGFloat deltaX;
	CGFloat deltaY;

	/* Use precise scrolling-specific deltas if they're supported. */
	if ([ event respondsToSelector:@selector(hasPreciseScrollingDeltas) ]) {
		/* No precise deltas indicates a scroll wheel is being used, so we don't want 2D scrolling. */
		if (![ event hasPreciseScrollingDeltas ]) return;

		deltaX = [ event scrollingDeltaX ] * 0.5f;
		deltaY = [ event scrollingDeltaY ] * 0.5f;
	} else {
		deltaX = [ event deltaX ] * 5;
		deltaY = [ event deltaY ] * 5;
	}

	_cursor.h_wheel -= (int)(deltaX * _settings_client.gui.scrollwheel_multiplier);
	_cursor.v_wheel -= (int)(deltaY * _settings_client.gui.scrollwheel_multiplier);
}

- (void)magnifyWithEvent:(NSEvent *)event
{
	/* Pinch open or close gesture. */
	self->_current_magnification += [ event magnification ] * 5.0f;

	while (self->_current_magnification >= 1.0f) {
		self->_current_magnification -= 1.0f;
		_cursor.wheel--;
		HandleMouseEvents();
	}
	while (self->_current_magnification <= -1.0f) {
		self->_current_magnification += 1.0f;
		_cursor.wheel++;
		HandleMouseEvents();
	}
}

- (void)endGestureWithEvent:(NSEvent *)event
{
	/* Gesture ended. */
	self->_current_magnification = 0.0f;
}


- (BOOL)internalHandleKeycode:(unsigned short)keycode unicode:(char32_t)unicode pressed:(BOOL)down modifiers:(NSUInteger)modifiers
{
	switch (keycode) {
		case QZ_UP:    SB(_dirkeys, 1, 1, down); break;
		case QZ_DOWN:  SB(_dirkeys, 3, 1, down); break;
		case QZ_LEFT:  SB(_dirkeys, 0, 1, down); break;
		case QZ_RIGHT: SB(_dirkeys, 2, 1, down); break;

		case QZ_TAB: _tab_is_down = down; break;

		case QZ_RETURN:
		case QZ_f:
			if (down && (modifiers & NSEventModifierFlagCommand)) {
				VideoDriver::GetInstance()->ToggleFullscreen(!_fullscreen);
			}
			break;

		case QZ_v:
			if (down && EditBoxInGlobalFocus() && (modifiers & (NSEventModifierFlagCommand | NSEventModifierFlagControl))) {
				HandleKeypress(WKC_CTRL | 'V', unicode);
			}
			break;
		case QZ_u:
			if (down && EditBoxInGlobalFocus() && (modifiers & (NSEventModifierFlagCommand | NSEventModifierFlagControl))) {
				HandleKeypress(WKC_CTRL | 'U', unicode);
			}
			break;
	}

	BOOL interpret_keys = YES;
	if (down) {
		/* Map keycode to OTTD code. */
		auto vk = std::find_if(std::begin(_vk_mapping), std::end(_vk_mapping), [=](const CocoaVkMapping &m) { return m.vk_from == keycode; });
		uint32_t pressed_key = vk != std::end(_vk_mapping) ? vk->map_to : 0;

		if (modifiers & NSEventModifierFlagShift)   pressed_key |= WKC_SHIFT;
		if (modifiers & NSEventModifierFlagControl) pressed_key |= (_settings_client.gui.right_mouse_btn_emulation != RMBE_CONTROL ? WKC_CTRL : WKC_META);
		if (modifiers & NSEventModifierFlagOption)  pressed_key |= WKC_ALT;
		if (modifiers & NSEventModifierFlagCommand) pressed_key |= (_settings_client.gui.right_mouse_btn_emulation != RMBE_CONTROL ? WKC_META : WKC_CTRL);

		static bool console = false;

		/* The second backquote may have a character, which we don't want to interpret. */
		if (pressed_key == WKC_BACKQUOTE && (console || unicode == 0)) {
			if (!console) {
				/* Backquote is a dead key, require a double press for hotkey behaviour (i.e. console). */
				console = true;
				return YES;
			} else {
				/* Second backquote, don't interpret as text input. */
				interpret_keys = NO;
			}
		}
		console = false;

		/* Don't handle normal characters if an edit box has the focus. */
		if (!EditBoxInGlobalFocus() || IsInsideMM(pressed_key & ~WKC_SPECIAL_KEYS, WKC_F1, WKC_PAUSE + 1)) {
			HandleKeypress(pressed_key, unicode);
		}
		Debug(driver, 3, "cocoa_v: QZ_KeyEvent: {:x} ({:x}), down, mapping: {:x}", keycode, (int)unicode, pressed_key);
	} else {
		Debug(driver, 3, "cocoa_v: QZ_KeyEvent: {:x} ({:x}), up", keycode, (int)unicode);
	}

	return interpret_keys;
}

- (void)keyDown:(NSEvent *)event
{
	/* Quit, hide and minimize */
	switch (event.keyCode) {
		case QZ_q:
		case QZ_h:
		case QZ_m:
			if (event.modifierFlags & NSEventModifierFlagCommand) {
				[ self interpretKeyEvents:[ NSArray arrayWithObject:event ] ];
			}
			break;
	}

	/* Convert UTF-16 characters to UCS-4 chars. */
	std::vector<char32_t> unicode_str = NSStringToUTF32([ event characters ]);
	if (unicode_str.empty()) unicode_str.push_back(0);

	if (EditBoxInGlobalFocus()) {
		if ([ self internalHandleKeycode:event.keyCode unicode:unicode_str[0] pressed:YES modifiers:event.modifierFlags ]) {
			[ self interpretKeyEvents:[ NSArray arrayWithObject:event ] ];
		}
	} else {
		[ self internalHandleKeycode:event.keyCode unicode:unicode_str[0] pressed:YES modifiers:event.modifierFlags ];
		for (size_t i = 1; i < unicode_str.size(); i++) {
			[ self internalHandleKeycode:0 unicode:unicode_str[i] pressed:YES modifiers:event.modifierFlags ];
		}
	}
}

- (void)keyUp:(NSEvent *)event
{
	/* Quit, hide and minimize */
	switch (event.keyCode) {
		case QZ_q:
		case QZ_h:
		case QZ_m:
			if (event.modifierFlags & NSEventModifierFlagCommand) {
				[ self interpretKeyEvents:[ NSArray arrayWithObject:event ] ];
			}
			break;
	}

	/* Convert UTF-16 characters to UCS-4 chars. */
	std::vector<char32_t> unicode_str = NSStringToUTF32([ event characters ]);
	if (unicode_str.empty()) unicode_str.push_back(0);

	[ self internalHandleKeycode:event.keyCode unicode:unicode_str[0] pressed:NO modifiers:event.modifierFlags ];
}

- (void)flagsChanged:(NSEvent *)event
{
	const int mapping[] = { QZ_CAPSLOCK, QZ_LSHIFT, QZ_LCTRL, QZ_LALT, QZ_LMETA };

	NSUInteger newMods = event.modifierFlags;
	if (self->_current_mods == newMods) return;

	/* Iterate through the bits, testing each against the current modifiers */
	for (unsigned int i = 0, bit = NSEventModifierFlagCapsLock; bit <= NSEventModifierFlagCommand; bit <<= 1, ++i) {
		unsigned int currentMask, newMask;

		currentMask = self->_current_mods & bit;
		newMask     = newMods & bit;

		if (currentMask && currentMask != newMask) { // modifier up event
			[ self internalHandleKeycode:mapping[i] unicode:0 pressed:NO modifiers:newMods ];
		} else if (newMask && currentMask != newMask) { // modifier down event
			[ self internalHandleKeycode:mapping[i] unicode:0 pressed:YES modifiers:newMods ];
		}
	}

	_current_mods = newMods;
}


/** Insert the given text at the given range. */
- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange
{
	if (!EditBoxInGlobalFocus()) return;

	NSString *s = [ aString isKindOfClass:[ NSAttributedString class ] ] ? [ aString string ] : (NSString *)aString;

	const char *insert_point = nullptr;
	const char *replace_range = nullptr;
	if (replacementRange.location != NSNotFound) {
		/* Calculate the part to be replaced. */
		insert_point = Utf8AdvanceByUtf16Units(_focused_window->GetFocusedTextbuf()->GetText(), replacementRange.location);
		replace_range = Utf8AdvanceByUtf16Units(insert_point, replacementRange.length);
	}

	HandleTextInput(nullptr, true);
	HandleTextInput([ s UTF8String ], false, nullptr, insert_point, replace_range);
}

/** Insert the given text at the caret. */
- (void)insertText:(id)aString
{
	[ self insertText:aString replacementRange:NSMakeRange(NSNotFound, 0) ];
}

/** Set a new marked text and reposition the caret. */
- (void)setMarkedText:(id)aString selectedRange:(NSRange)selRange replacementRange:(NSRange)replacementRange
{
	if (!EditBoxInGlobalFocus()) return;

	NSString *s = [ aString isKindOfClass:[ NSAttributedString class ] ] ? [ aString string ] : (NSString *)aString;

	const char *utf8 = [ s UTF8String ];
	if (utf8 != nullptr) {
		const char *insert_point = nullptr;
		const char *replace_range = nullptr;
		if (replacementRange.location != NSNotFound) {
			/* Calculate the part to be replaced. */
			NSRange marked = [ self markedRange ];
			insert_point = Utf8AdvanceByUtf16Units(_focused_window->GetFocusedTextbuf()->GetText(), replacementRange.location + (marked.location != NSNotFound ? marked.location : 0u));
			replace_range = Utf8AdvanceByUtf16Units(insert_point, replacementRange.length);
		}

		/* Convert caret index into a pointer in the UTF-8 string. */
		const char *selection = Utf8AdvanceByUtf16Units(utf8, selRange.location);

		HandleTextInput(utf8, true, selection, insert_point, replace_range);
	}
}

/** Set a new marked text and reposition the caret. */
- (void)setMarkedText:(id)aString selectedRange:(NSRange)selRange
{
	[ self setMarkedText:aString selectedRange:selRange replacementRange:NSMakeRange(NSNotFound, 0) ];
}

/** Unmark the current marked text. */
- (void)unmarkText
{
	HandleTextInput(nullptr, true);
}

/** Get the caret position. */
- (NSRange)selectedRange
{
	if (!EditBoxInGlobalFocus()) return NSMakeRange(NSNotFound, 0);

	const Textbuf *text_buf = _focused_window->GetFocusedTextbuf();
	const char *text = text_buf->GetText();
	NSUInteger start = CountUtf16Units(text, text + text_buf->caretpos);
	return NSMakeRange(start, 0);
}

/** Get the currently marked range. */
- (NSRange)markedRange
{
	if (!EditBoxInGlobalFocus()) return NSMakeRange(NSNotFound, 0);

	const Textbuf *text_buf = _focused_window->GetFocusedTextbuf();
	if (text_buf->markend != 0) {
		const char *text = text_buf->GetText();
		const char *mark = text + text_buf->markpos;
		NSUInteger start = CountUtf16Units(text, mark);
		NSUInteger len = CountUtf16Units(mark, text + text_buf->markend);

		return NSMakeRange(start, len);
	}

	return NSMakeRange(NSNotFound, 0);
}

/** Is any text marked? */
- (BOOL)hasMarkedText
{
	if (!EditBoxInGlobalFocus()) return NO;

	return _focused_window->GetFocusedTextbuf()->markend != 0;
}

/** Get a string corresponding to the given range. */
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)theRange actualRange:(NSRangePointer)actualRange
{
	if (!EditBoxInGlobalFocus()) return nil;

	NSString *s = [ NSString stringWithUTF8String:_focused_window->GetFocusedTextbuf()->GetText() ];
	NSRange valid_range = NSIntersectionRange(NSMakeRange(0, [ s length ]), theRange);

	if (actualRange != nullptr) *actualRange = valid_range;
	if (valid_range.length == 0) return nil;

	return [ [ [ NSAttributedString alloc ] initWithString:[ s substringWithRange:valid_range ] ] autorelease ];
}

/** Get a string corresponding to the given range. */
- (NSAttributedString *)attributedSubstringFromRange:(NSRange)theRange
{
	return [ self attributedSubstringForProposedRange:theRange actualRange:nil ];
}

/** Get the current edit box string. */
- (NSAttributedString *)attributedString
{
	if (!EditBoxInGlobalFocus()) return [ [ [ NSAttributedString alloc ] initWithString:@"" ] autorelease ];

	return [ [ [ NSAttributedString alloc ] initWithString:[ NSString stringWithUTF8String:_focused_window->GetFocusedTextbuf()->GetText() ] ] autorelease ];
}

/** Get the character that is rendered at the given point. */
- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
	if (!EditBoxInGlobalFocus()) return NSNotFound;

	NSPoint view_pt = [ self convertRect:[ [ self window ] convertRectFromScreen:NSMakeRect(thePoint.x, thePoint.y, 0, 0) ] fromView:nil ].origin;

	Point pt = { (int)view_pt.x, (int)[ self frame ].size.height - (int)view_pt.y };

	auto index = _focused_window->GetTextCharacterAtPosition(pt);
	if (index == -1) return NSNotFound;

	auto text = _focused_window->GetFocusedTextbuf()->GetText();
	return CountUtf16Units(text, text + index);
}

/** Get the bounding rect for the given range. */
- (NSRect)firstRectForCharacterRange:(NSRange)aRange
{
	if (!EditBoxInGlobalFocus()) return NSMakeRect(0, 0, 0, 0);

	const char *focused_text = _focused_window->GetFocusedTextbuf()->GetText();
	/* Convert range to UTF-8 string pointers. */
	const char *start = Utf8AdvanceByUtf16Units(focused_text, aRange.location);
	const char *end = aRange.length != 0 ? Utf8AdvanceByUtf16Units(focused_text, aRange.location + aRange.length) : start;

	/* Get the bounding rect for the text range.*/
	Rect r = _focused_window->GetTextBoundingRect(start, end);
	NSRect view_rect = NSMakeRect(_focused_window->left + r.left, [ self frame ].size.height - _focused_window->top - r.bottom, r.right - r.left, r.bottom - r.top);

	return [ [ self window ] convertRectToScreen:[ self convertRect:view_rect toView:nil ] ];
}

/** Get the bounding rect for the given range. */
- (NSRect)firstRectForCharacterRange:(NSRange)aRange actualRange:(NSRangePointer)actualRange
{
	return [ self firstRectForCharacterRange:aRange ];
}

/** Get all string attributes that we can process for marked text. */
- (NSArray*)validAttributesForMarkedText
{
	return [ NSArray array ];
}

/** Delete single character left of the cursor. */
- (void)deleteBackward:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_BACKSPACE, 0);
}

/** Delete word left of the cursor. */
- (void)deleteWordBackward:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_BACKSPACE | WKC_CTRL, 0);
}

/** Delete single character right of the cursor. */
- (void)deleteForward:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_DELETE, 0);
}

/** Delete word right of the cursor. */
- (void)deleteWordForward:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_DELETE | WKC_CTRL, 0);
}

/** Move cursor one character left. */
- (void)moveLeft:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_LEFT, 0);
}

/** Move cursor one word left. */
- (void)moveWordLeft:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_LEFT | WKC_CTRL, 0);
}

/** Move cursor one character right. */
- (void)moveRight:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_RIGHT, 0);
}

/** Move cursor one word right. */
- (void)moveWordRight:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_RIGHT | WKC_CTRL, 0);
}

/** Move cursor one line up. */
- (void)moveUp:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_UP, 0);
}

/** Move cursor one line down. */
- (void)moveDown:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_DOWN, 0);
}

/** MScroll one line up. */
- (void)moveUpAndModifySelection:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_UP | WKC_SHIFT, 0);
}

/** Scroll one line down. */
- (void)moveDownAndModifySelection:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_DOWN | WKC_SHIFT, 0);
}

/** Move cursor to the start of the line. */
- (void)moveToBeginningOfLine:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_HOME, 0);
}

/** Move cursor to the end of the line. */
- (void)moveToEndOfLine:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_END, 0);
}

/** Scroll one page up. */
- (void)scrollPageUp:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_PAGEUP, 0);
}

/** Scroll one page down. */
- (void)scrollPageDown:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_PAGEDOWN, 0);
}

/** Move cursor (and selection) one page up. */
- (void)pageUpAndModifySelection:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_PAGEUP | WKC_SHIFT, 0);
}

/** Move cursor (and selection) one page down. */
- (void)pageDownAndModifySelection:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_PAGEDOWN | WKC_SHIFT, 0);
}

/** Scroll to the beginning of the document. */
- (void)scrollToBeginningOfDocument:(id)sender
{
	/* For compatibility with OTTD on Win/Linux. */
	[ self moveToBeginningOfLine:sender ];
}

/** Scroll to the end of the document. */
- (void)scrollToEndOfDocument:(id)sender
{
	/* For compatibility with OTTD on Win/Linux. */
	[ self moveToEndOfLine:sender ];
}

/** Return was pressed. */
- (void)insertNewline:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_RETURN, '\r');
}

/** Escape was pressed. */
- (void)cancelOperation:(id)sender
{
	if (EditBoxInGlobalFocus()) HandleKeypress(WKC_ESC, 0);
}

/** Invoke the selector if we implement it. */
- (void)doCommandBySelector:(SEL)aSelector
{
	if ([ self respondsToSelector:aSelector ]) [ self performSelector:aSelector ];
}

@end


@implementation OTTD_CocoaWindowDelegate {
	VideoDriver_Cocoa *driver;
}

/** Initialize the video driver */
- (instancetype)initWithDriver:(VideoDriver_Cocoa *)drv
{
	if (self = [ super init ]) {
		self->driver = drv;
	}
	return self;
}
/** Handle closure requests */
- (BOOL)windowShouldClose:(id)sender
{
	HandleExitGameRequest();

	return NO;
}
/** Window entered fullscreen mode (10.7). */
- (void)windowDidEnterFullScreen:(NSNotification *)aNotification
{
	NSPoint loc = [ driver->cocoaview convertPoint:[ [ aNotification object ] mouseLocationOutsideOfEventStream ] fromView:nil ];
	BOOL inside = ([ driver->cocoaview hitTest:loc ] == driver->cocoaview);

	if (inside) {
		/* We don't care about the event, but the compiler does. */
		NSEvent *e = [ [ NSEvent alloc ] init ];
		[ driver->cocoaview mouseEntered:e ];
		[ e release ];
	}
}
/** Screen the window is on changed. */
- (void)windowDidChangeBackingProperties:(NSNotification *)notification
{
	bool did_adjust = AdjustGUIZoom(true);

	/* Reallocate screen buffer if necessary. */
	driver->AllocateBackingStore();

	if (did_adjust) ReInitAllWindows(true);
}

/** Presentation options to use for full screen mode. */
- (NSApplicationPresentationOptions)window:(NSWindow *)window willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions
{
	return NSApplicationPresentationFullScreen | NSApplicationPresentationHideMenuBar | NSApplicationPresentationHideDock;
}

@end

#endif /* WITH_COCOA */
