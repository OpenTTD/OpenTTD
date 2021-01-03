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

class CocoaSubdriver;

extern NSString *OTTDMainLaunchGameEngine;

/** Category of NSCursor to allow cursor showing/hiding */
@interface NSCursor (OTTD_QuickdrawCursor)
+ (NSCursor *) clearCocoaCursor;
@end

/** Subclass of NSWindow to cater our special needs */
@interface OTTD_CocoaWindow : NSWindow {
	CocoaSubdriver *driver;
}

- (void)setDriver:(CocoaSubdriver*)drv;

- (void)miniaturize:(id)sender;
- (void)display;
- (void)setFrame:(NSRect)frameRect display:(BOOL)flag;
- (void)appDidHide:(NSNotification*)note;
- (void)appDidUnhide:(NSNotification*)note;
- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingType defer:(BOOL)flag;
@end

/** Subclass of NSView to fix Quartz rendering and mouse awareness */
@interface OTTD_CocoaView : NSView <NSTextInputClient>
{
	CocoaSubdriver *driver;
	NSTrackingRectTag trackingtag;
}
- (void)setDriver:(CocoaSubdriver*)drv;
- (void)drawRect:(NSRect)rect;
- (BOOL)isOpaque;
- (BOOL)acceptsFirstResponder;
- (BOOL)becomeFirstResponder;
- (void)setTrackingRect;
- (void)clearTrackingRect;
- (void)resetCursorRects;
- (void)viewWillMoveToWindow:(NSWindow *)win;
- (void)viewDidMoveToWindow;
- (void)mouseEntered:(NSEvent *)theEvent;
- (void)mouseExited:(NSEvent *)theEvent;
@end

/** Delegate for our NSWindow to send ask for quit on close */
@interface OTTD_CocoaWindowDelegate : NSObject <NSWindowDelegate>
{
	CocoaSubdriver *driver;
}

- (void)setDriver:(CocoaSubdriver*)drv;

- (BOOL)windowShouldClose:(id)sender;
- (void)windowDidEnterFullScreen:(NSNotification *)aNotification;
- (void)windowDidChangeScreenProfile:(NSNotification *)aNotification;
- (NSApplicationPresentationOptions)window:(NSWindow *)window willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions;
@end


bool CocoaSetupApplication();
void CocoaExitApplication();

#endif /* COCOA_WND_H */
