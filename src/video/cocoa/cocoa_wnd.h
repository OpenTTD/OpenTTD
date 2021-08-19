/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cocoa_wnd.h OS interface for the cocoa video driver. */

#ifndef COCOA_WND_H
#define COCOA_WND_H

#import <Cocoa/Cocoa.h>
#include "toolbar_gui.h"
#include "table/sprites.h"

class VideoDriver_Cocoa;

/* Right Mouse Button Emulation enum */
enum RightMouseButtonEmulationState {
	RMBE_COMMAND = 0,
	RMBE_CONTROL = 1,
	RMBE_OFF     = 2,
};

extern NSString *OTTDMainLaunchGameEngine;

/** Category of NSCursor to allow cursor showing/hiding */
@interface NSCursor (OTTD_QuickdrawCursor)
+ (NSCursor *) clearCocoaCursor;
@end

#ifdef HAVE_OSX_1015_SDK
/* 9 items can be displayed on the touch bar when using default buttons. */
static NSArray *touchBarButtonIdentifiers = @[
	@"openttd.pause",
	@"openttd.fastforward",
	@"openttd.zoom_in",
	@"openttd.zoom_out",
	@"openttd.build_rail",
	@"openttd.build_road",
	@"openttd.build_tram",
	@"openttd.build_docks",
	@"openttd.build_airport",
	NSTouchBarItemIdentifierOtherItemsProxy
];

static NSDictionary *touchBarButtonSprites = @{
	@"openttd.pause":           [NSNumber numberWithInt:SPR_IMG_PAUSE],
	@"openttd.fastforward":     [NSNumber numberWithInt:SPR_IMG_FASTFORWARD],
	@"openttd.zoom_in":         [NSNumber numberWithInt:SPR_IMG_ZOOMIN],
	@"openttd.zoom_out":        [NSNumber numberWithInt:SPR_IMG_ZOOMOUT],
	@"openttd.build_rail":      [NSNumber numberWithInt:SPR_IMG_BUILDRAIL],
	@"openttd.build_road":      [NSNumber numberWithInt:SPR_IMG_BUILDROAD],
	@"openttd.build_tram":      [NSNumber numberWithInt:SPR_IMG_BUILDTRAMS],
	@"openttd.build_docks":     [NSNumber numberWithInt:SPR_IMG_BUILDWATER],
	@"openttd.build_airport":   [NSNumber numberWithInt:SPR_IMG_BUILDAIR],
};

static NSDictionary *touchBarButtonActions = @{
	@"openttd.pause":           [NSNumber numberWithInt:MTHK_PAUSE],
	@"openttd.fastforward":     [NSNumber numberWithInt:MTHK_FASTFORWARD],
	@"openttd.zoom_in":         [NSNumber numberWithInt:MTHK_ZOOM_IN],
	@"openttd.zoom_out":        [NSNumber numberWithInt:MTHK_ZOOM_OUT],
	@"openttd.build_rail":      [NSNumber numberWithInt:MTHK_BUILD_RAIL],
	@"openttd.build_road":      [NSNumber numberWithInt:MTHK_BUILD_ROAD],
	@"openttd.build_tram":      [NSNumber numberWithInt:MTHK_BUILD_TRAM],
	@"openttd.build_docks":     [NSNumber numberWithInt:MTHK_BUILD_DOCKS],
	@"openttd.build_airport":   [NSNumber numberWithInt:MTHK_BUILD_AIRPORT],
};

static NSDictionary *touchBarFallbackText = @{
	@"openttd.pause":           @"Pause",
	@"openttd.fastforward":     @"Fast Forward",
	@"openttd.zoom_in":         @"Zoom In",
	@"openttd.zoom_out":        @"Zoom Out",
	@"openttd.build_rail":      @"Rail",
	@"openttd.build_road":      @"Road",
	@"openttd.build_tram":      @"Tram",
	@"openttd.build_docks":     @"Docks",
	@"openttd.build_airport":   @"Airport",
};
#endif

/** Subclass of NSWindow to cater our special needs */
#ifdef HAVE_OSX_1015_SDK
@interface OTTD_CocoaWindow : NSWindow <NSTouchBarDelegate>
@property (strong) NSSet *touchbarItems;
- (NSImage*)generateImage:(int)spriteId;
#else
@interface OTTD_CocoaWindow : NSWindow
#endif

- (instancetype)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag driver:(VideoDriver_Cocoa *)drv;

- (void)setFrame:(NSRect)frameRect display:(BOOL)flag;
@end

/** Subclass of NSView to support mouse awareness and text input. */
@interface OTTD_CocoaView : NSView <NSTextInputClient>
- (NSRect)getRealRect:(NSRect)rect;
- (NSRect)getVirtualRect:(NSRect)rect;
- (CGFloat)getContentsScale;
- (NSPoint)mousePositionFromEvent:(NSEvent *)e;
@end

/** Delegate for our NSWindow to send ask for quit on close */
@interface OTTD_CocoaWindowDelegate : NSObject <NSWindowDelegate>
- (instancetype)initWithDriver:(VideoDriver_Cocoa *)drv;

- (BOOL)windowShouldClose:(id)sender;
- (void)windowDidEnterFullScreen:(NSNotification *)aNotification;
- (void)windowDidChangeBackingProperties:(NSNotification *)notification;
- (NSApplicationPresentationOptions)window:(NSWindow *)window willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions;
@end


extern bool _allow_hidpi_window;

bool CocoaSetupApplication();
void CocoaExitApplication();

#endif /* COCOA_WND_H */
