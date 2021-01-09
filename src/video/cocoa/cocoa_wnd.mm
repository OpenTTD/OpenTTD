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
#include "../../string_func.h"
#include "../../gfx_func.h"
#include "../../window_func.h"
#include "../../window_gui.h"


/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */

@interface OTTDMain : NSObject <NSApplicationDelegate>
@end

NSString *OTTDMainLaunchGameEngine = @"ottdmain_launch_game_engine";

static bool _cocoa_video_dialog = false;
static OTTDMain *_ottd_main;

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
	NSEvent *event = [ NSEvent otherEventWithType:NSApplicationDefined location:NSMakePoint(0, 0) modifierFlags:0 timestamp:0.0 windowNumber:0 context:nil subtype:0 data1:0 data2:0 ];
	[ NSApp postEvent:event atStart:YES ];
}

/**
 * Start the game loop.
 */
- (void)launchGameEngine: (NSNotification*) note
{
	/* Setup cursor for the current _game_mode. */
	[ _cocoa_subdriver->cocoaview resetCursorRects ];

	/* Hand off to main application code. */
	static_cast<VideoDriver_Cocoa *>(VideoDriver::GetInstance())->GameLoop();

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

	return NSTerminateCancel; // NSTerminateLater ?
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
	[ menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask | NSCommandKeyMask) ];

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
	if (returnCode != 0) DEBUG(driver, 0, "Could not change to foreground application. Error %d", (int)returnCode);

	/* Disable the system-wide tab feature as we only have one window. */
	if ([ NSWindow respondsToSelector:@selector(setAllowsAutomaticWindowTabbing:) ]) {
		/* We use nil instead of NO as withObject requires an id. */
		[ NSWindow performSelector:@selector(setAllowsAutomaticWindowTabbing:) withObject:nil];
	}

	/* Become the front process, important when start from the command line. */
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
 *
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
		fprintf(stderr, "%s: %s\n", title, message);
		return;
	}

	NSAlert *alert = [ [ NSAlert alloc ] init ];
	[ alert setAlertStyle: NSCriticalAlertStyle ];
	[ alert setMessageText:[ NSString stringWithUTF8String:title ] ];
	[ alert setInformativeText:[ NSString stringWithUTF8String:message ] ];
	[ alert addButtonWithTitle: [ NSString stringWithUTF8String:buttonLabel ] ];
	[ alert runModal ];
	[ alert release ];

	if (!wasstarted && VideoDriver::GetInstance() != NULL) VideoDriver::GetInstance()->Stop();

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

@implementation OTTD_CocoaWindow

- (void)setDriver:(WindowQuartzSubdriver *)drv
{
	driver = drv;
}
/**
 * Minimize the window
 */
- (void)miniaturize:(id)sender
{
	/* window is hidden now */
	driver->active = false;

	[ super miniaturize:sender ];
}

/**
 * This method fires just before the window deminaturizes from the Dock.
 * We'll save the current visible surface, let the window manager redraw any
 * UI elements, and restore the surface. This way, no expose event
 * is required, and the deminiaturize works perfectly.
 */
- (void)display
{
	/* save current visible surface */
	[ self cacheImageInRect:[ driver->cocoaview frame ] ];

	/* let the window manager redraw controls, border, etc */
	[ super display ];

	/* restore visible surface */
	[ self restoreCachedImage ];

	/* window is visible again */
	driver->active = true;
}
/**
 * Define the rectangle we draw our window in
 */
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag
{
	[ super setFrame:frameRect display:flag ];

	/* Don't do anything if the window is currently being created */
	if (driver->setup) return;

	if (!driver->WindowResized()) error("Cocoa: Failed to resize window.");
}
/**
 * Handle hiding of the application
 */
- (void)appDidHide:(NSNotification*)note
{
	driver->active = false;
}
/**
 * Unhide and restore display plane and re-activate driver
 */
- (void)appDidUnhide:(NSNotification*)note
{
	driver->active = true;
}
/**
 * Initialize event system for the application rectangle
 */
- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag
{
	/* Make our window subclass receive these application notifications */
	[ [ NSNotificationCenter defaultCenter ] addObserver:self
		selector:@selector(appDidHide:) name:NSApplicationDidHideNotification object:NSApp ];

	[ [ NSNotificationCenter defaultCenter ] addObserver:self
		selector:@selector(appDidUnhide:) name:NSApplicationDidUnhideNotification object:NSApp ];

	return [ super initWithContentRect:contentRect styleMask:styleMask backing:backingType defer:flag ];
}

@end



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
		WChar c;
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
		WChar c;
		size_t len = Utf8Decode(&c, str);
		i += len < 4 ? 1 : 2; // Watch for surrogates.
		str += len;
	}

	return str;
}

@implementation OTTD_CocoaView
/**
 * Initialize the driver
 */
- (void)setDriver:(WindowQuartzSubdriver *)drv
{
	driver = drv;
}
/**
 * Define the opaqueness of the window / screen
 * @return opaqueness of window / screen
 */
- (BOOL)isOpaque
{
	return YES;
}
/**
 * Draws a rectangle on the screen.
 * It's overwritten by the individual drivers but must be defined
 */
- (void)drawRect:(NSRect)invalidRect
{
	return;
}
/**
 * Allow to handle events
 */
- (BOOL)acceptsFirstResponder
{
	return YES;
}
/**
 * Actually handle events
 */
- (BOOL)becomeFirstResponder
{
	return YES;
}
/**
 * Define the rectangle where we draw our application window
 */
- (void)setTrackingRect
{
	NSPoint loc = [ self convertPoint:[ [ self window ] mouseLocationOutsideOfEventStream ] fromView:nil ];
	BOOL inside = ([ self hitTest:loc ]==self);
	if (inside) [ [ self window ] makeFirstResponder:self ];
	trackingtag = [ self addTrackingRect:[ self visibleRect ] owner:self userData:nil assumeInside:inside ];
}
/**
 * Return responsibility for the application window to system
 */
- (void)clearTrackingRect
{
	[ self removeTrackingRect:trackingtag ];
}
/**
 * Declare responsibility for the cursor within our application rect
 */
- (void)resetCursorRects
{
	[ super resetCursorRects ];
	[ self clearTrackingRect ];
	[ self setTrackingRect ];
	[ self addCursorRect:[ self bounds ] cursor:(_game_mode == GM_BOOTSTRAP ? [ NSCursor arrowCursor ] : [ NSCursor clearCocoaCursor ]) ];
}
/**
 * Prepare for moving the application window
 */
- (void)viewWillMoveToWindow:(NSWindow *)win
{
	if (!win && [ self window ]) [ self clearTrackingRect ];
}
/**
 * Restore our responsibility for our application window after moving
 */
- (void)viewDidMoveToWindow
{
	if ([ self window ]) [ self setTrackingRect ];
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
	if (_cocoa_subdriver != NULL) UndrawMouseCursor();
	_cursor.in_window = false;
}


/** Insert the given text at the given range. */
- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange
{
	if (!EditBoxInGlobalFocus()) return;

	NSString *s = [ aString isKindOfClass:[ NSAttributedString class ] ] ? [ aString string ] : (NSString *)aString;

	const char *insert_point = NULL;
	const char *replace_range = NULL;
	if (replacementRange.location != NSNotFound) {
		/* Calculate the part to be replaced. */
		insert_point = Utf8AdvanceByUtf16Units(_focused_window->GetFocusedText(), replacementRange.location);
		replace_range = Utf8AdvanceByUtf16Units(insert_point, replacementRange.length);
	}

	HandleTextInput(NULL, true);
	HandleTextInput([ s UTF8String ], false, NULL, insert_point, replace_range);
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
	if (utf8 != NULL) {
		const char *insert_point = NULL;
		const char *replace_range = NULL;
		if (replacementRange.location != NSNotFound) {
			/* Calculate the part to be replaced. */
			NSRange marked = [ self markedRange ];
			insert_point = Utf8AdvanceByUtf16Units(_focused_window->GetFocusedText(), replacementRange.location + (marked.location != NSNotFound ? marked.location : 0u));
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
	HandleTextInput(NULL, true);
}

/** Get the caret position. */
- (NSRange)selectedRange
{
	if (!EditBoxInGlobalFocus()) return NSMakeRange(NSNotFound, 0);

	NSUInteger start = CountUtf16Units(_focused_window->GetFocusedText(), _focused_window->GetCaret());
	return NSMakeRange(start, 0);
}

/** Get the currently marked range. */
- (NSRange)markedRange
{
	if (!EditBoxInGlobalFocus()) return NSMakeRange(NSNotFound, 0);

	size_t mark_len;
	const char *mark = _focused_window->GetMarkedText(&mark_len);
	if (mark != NULL) {
		NSUInteger start = CountUtf16Units(_focused_window->GetFocusedText(), mark);
		NSUInteger len = CountUtf16Units(mark, mark + mark_len);

		return NSMakeRange(start, len);
	}

	return NSMakeRange(NSNotFound, 0);
}

/** Is any text marked? */
- (BOOL)hasMarkedText
{
	if (!EditBoxInGlobalFocus()) return NO;

	size_t len;
	return _focused_window->GetMarkedText(&len) != NULL;
}

/** Get a string corresponding to the given range. */
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)theRange actualRange:(NSRangePointer)actualRange
{
	if (!EditBoxInGlobalFocus()) return nil;

	NSString *s = [ NSString stringWithUTF8String:_focused_window->GetFocusedText() ];
	NSRange valid_range = NSIntersectionRange(NSMakeRange(0, [ s length ]), theRange);

	if (actualRange != NULL) *actualRange = valid_range;
	if (valid_range.length == 0) return nil;

	return [ [ [ NSAttributedString alloc ] initWithString:[ s substringWithRange:valid_range ] ] autorelease ];
}

/** Get a string corresponding to the given range. */
- (NSAttributedString *)attributedSubstringFromRange:(NSRange)theRange
{
	return [ self attributedSubstringForProposedRange:theRange actualRange:NULL ];
}

/** Get the current edit box string. */
- (NSAttributedString *)attributedString
{
	if (!EditBoxInGlobalFocus()) return [ [ [ NSAttributedString alloc ] initWithString:@"" ] autorelease ];

	return [ [ [ NSAttributedString alloc ] initWithString:[ NSString stringWithUTF8String:_focused_window->GetFocusedText() ] ] autorelease ];
}

/** Get the character that is rendered at the given point. */
- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
	if (!EditBoxInGlobalFocus()) return NSNotFound;

	NSPoint view_pt = [ self convertRect:[ [ self window ] convertRectFromScreen:NSMakeRect(thePoint.x, thePoint.y, 0, 0) ] fromView:nil ].origin;

	Point pt = { (int)view_pt.x, (int)[ self frame ].size.height - (int)view_pt.y };

	const char *ch = _focused_window->GetTextCharacterAtPosition(pt);
	if (ch == NULL) return NSNotFound;

	return CountUtf16Units(_focused_window->GetFocusedText(), ch);
}

/** Get the bounding rect for the given range. */
- (NSRect)firstRectForCharacterRange:(NSRange)aRange
{
	if (!EditBoxInGlobalFocus()) return NSMakeRect(0, 0, 0, 0);

	/* Convert range to UTF-8 string pointers. */
	const char *start = Utf8AdvanceByUtf16Units(_focused_window->GetFocusedText(), aRange.location);
	const char *end = aRange.length != 0 ? Utf8AdvanceByUtf16Units(_focused_window->GetFocusedText(), aRange.location + aRange.length) : start;

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



@implementation OTTD_CocoaWindowDelegate
/** Initialize the video driver */
- (void)setDriver:(WindowQuartzSubdriver *)drv
{
	driver = drv;
}
/** Handle closure requests */
- (BOOL)windowShouldClose:(id)sender
{
	HandleExitGameRequest();

	return NO;
}
/** Handle key acceptance */
- (void)windowDidBecomeKey:(NSNotification*)aNotification
{
	driver->active = true;
}
/** Resign key acceptance */
- (void)windowDidResignKey:(NSNotification*)aNotification
{
	driver->active = false;
}
/** Handle becoming main window */
- (void)windowDidBecomeMain:(NSNotification*)aNotification
{
	driver->active = true;
}
/** Resign being main window */
- (void)windowDidResignMain:(NSNotification*)aNotification
{
	driver->active = false;
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
/** The colour profile of the screen the window is on changed. */
- (void)windowDidChangeScreenProfile:(NSNotification *)aNotification
{
	if (!driver->setup) driver->WindowResized();
}

/** Presentation options to use for fullsreen mode. */
- (NSApplicationPresentationOptions)window:(NSWindow *)window willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions
{
	return NSApplicationPresentationFullScreen | NSApplicationPresentationHideMenuBar | NSApplicationPresentationHideDock;
}

@end
#endif /* WITH_COCOA */
