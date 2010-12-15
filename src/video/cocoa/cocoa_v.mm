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
#include "../../core/geometry_type.hpp"
#include "cocoa_v.h"
#include "../../blitter/factory.hpp"
#include "../../fileio_func.h"
#include "../../gfx_func.h"
#include "../../functions.h"

#import <sys/param.h> /* for MAXPATHLEN */

/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */


@interface OTTDMain : NSObject
@end


static NSAutoreleasePool *_ottd_autorelease_pool;
static OTTDMain *_ottd_main;
static bool _cocoa_video_started = false;
static bool _cocoa_video_dialog = false;

CocoaSubdriver *_cocoa_subdriver = NULL;



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
	NSString *appName = @"OTTD";
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
	[ NSApp performSelector:@selector(setAppleMenu:) withObject:appleMenu ];

	/* Finally give up our references to the objects */
	[ appleMenu release ];
	[ menuItem release ];
}

/* Create a window menu */
static void setupWindowMenu()
{
	NSMenu *windowMenu = [ [ NSMenu alloc ] initWithTitle:@"Window" ];

	/* "Minimize" item */
	[ windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m" ];

	/* Put menu into the menubar */
	NSMenuItem *menuItem = [ [ NSMenuItem alloc ] initWithTitle:@"Window" action:nil keyEquivalent:@"" ];
	[ menuItem setSubmenu:windowMenu ];
	[ [ NSApp mainMenu ] addItem:menuItem ];

	/* Tell the application object that this is now the window menu */
	[ NSApp setWindowsMenu:windowMenu ];

	/* Finally give up our references to the objects */
	[ windowMenu release ];
	[ menuItem release ];
}

static void setupApplication()
{
	ProcessSerialNumber psn = { 0, kCurrentProcess };

	/* Ensure the application object is initialised */
	[ NSApplication sharedApplication ];

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_3
	/* Tell the dock about us */
	if (MacOSVersionIsAtLeast(10, 3, 0)) {
		OSStatus returnCode = TransformProcessType(&psn, kProcessTransformToForegroundApplication);
		if (returnCode != 0) DEBUG(driver, 0, "Could not change to foreground application. Error %d", (int)returnCode);
	}
#endif

	/* Become the front process, important when start from the command line. */
	OSErr err = SetFrontProcess(&psn);
	if (err != 0) DEBUG(driver, 0, "Could not bring the application to front. Error %d", (int)err);

	/* Set up the menubar */
	[ NSApp setMainMenu:[ [ NSMenu alloc ] init ] ];
	setApplicationMenu();
	setupWindowMenu();

	/* Create OTTDMain and make it the app delegate */
	_ottd_main = [ [ OTTDMain alloc ] init ];
	[ NSApp setDelegate:_ottd_main ];
}


static void QZ_UpdateVideoModes()
{
	assert(_cocoa_subdriver != NULL);

	OTTD_Point modes[32];
	uint count = _cocoa_subdriver->ListModes(modes, lengthof(modes));

	for (uint i = 0; i < count; i++) {
		_resolutions[i].width  = modes[i].x;
		_resolutions[i].height = modes[i].y;
	}

	_num_resolutions = count;
}


void QZ_GameSizeChanged()
{
	if (_cocoa_subdriver == NULL) return;

	/* Tell the game that the resolution has changed */
	_screen.width = _cocoa_subdriver->GetWidth();
	_screen.height = _cocoa_subdriver->GetHeight();
	_screen.pitch = _cocoa_subdriver->GetWidth();
	_screen.dst_ptr = _cocoa_subdriver->GetPixelBuffer();
	_fullscreen = _cocoa_subdriver->IsFullscreen();

	BlitterFactoryBase::GetCurrentBlitter()->PostResize();

	GameSizeChanged();
}


static CocoaSubdriver *QZ_CreateWindowSubdriver(int width, int height, int bpp)
{
#if defined(ENABLE_COCOA_QUARTZ) || defined(ENABLE_COCOA_QUICKDRAW)
	CocoaSubdriver *ret;
#endif

#ifdef ENABLE_COCOA_QUARTZ
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
	/* The reason for the version mismatch is due to the fact that the 10.4 binary needs to work on 10.5 as well. */
	if (MacOSVersionIsAtLeast(10, 5, 0)) {
		ret = QZ_CreateWindowQuartzSubdriver(width, height, bpp);
		if (ret != NULL) return ret;
	}
#endif
#endif

#ifdef ENABLE_COCOA_QUICKDRAW
	ret = QZ_CreateWindowQuickdrawSubdriver(width, height, bpp);
	if (ret != NULL) return ret;
#endif

#ifdef ENABLE_COCOA_QUARTZ
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
	/*
	 * If we get here we are running 10.4 or earlier and either openttd was compiled without the QuickDraw driver
	 * or it failed to load for some reason. Fall back to Quartz if possible even though that driver is slower.
	 */
	if (MacOSVersionIsAtLeast(10, 4, 0)) {
		ret = QZ_CreateWindowQuartzSubdriver(width, height, bpp);
		if (ret != NULL) return ret;
	}
#endif
#endif

	return NULL;
}


static CocoaSubdriver *QZ_CreateSubdriver(int width, int height, int bpp, bool fullscreen, bool fallback)
{
	CocoaSubdriver *ret = fullscreen ? QZ_CreateFullscreenSubdriver(width, height, bpp) : QZ_CreateWindowSubdriver(width, height, bpp);
	if (ret != NULL) return ret;

	if (!fallback) return NULL;

	/* Try again in 640x480 windowed */
	DEBUG(driver, 0, "Setting video mode failed, falling back to 640x480 windowed mode.");
	ret = QZ_CreateWindowSubdriver(640, 480, bpp);
	if (ret != NULL) return ret;

#ifdef _DEBUG
	/* Try fullscreen too when in debug mode */
	DEBUG(driver, 0, "Setting video mode failed, falling back to 640x480 fullscreen mode.");
	ret = QZ_CreateFullscreenSubdriver(640, 480, bpp);
	if (ret != NULL) return ret;
#endif

	return NULL;
}


static FVideoDriver_Cocoa iFVideoDriver_Cocoa;

void VideoDriver_Cocoa::Stop()
{
	if (!_cocoa_video_started) return;

	delete _cocoa_subdriver;
	_cocoa_subdriver = NULL;

	[ _ottd_main release ];

	_cocoa_video_started = false;
}

const char *VideoDriver_Cocoa::Start(const char * const *parm)
{
	if (!MacOSVersionIsAtLeast(10, 3, 0)) return "The Cocoa video driver requires Mac OS X 10.3 or later.";

	if (_cocoa_video_started) return "Already started";
	_cocoa_video_started = true;

	setupApplication();

	/* Don't create a window or enter fullscreen if we're just going to show a dialog. */
	if (_cocoa_video_dialog) return NULL;

	int width  = _cur_resolution.width;
	int height = _cur_resolution.height;
	int bpp = BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth();

	_cocoa_subdriver = QZ_CreateSubdriver(width, height, bpp, _fullscreen, true);
	if (_cocoa_subdriver == NULL) {
		Stop();
		return "Could not create subdriver";
	}

	QZ_GameSizeChanged();
	QZ_UpdateVideoModes();

	return NULL;
}

void VideoDriver_Cocoa::MakeDirty(int left, int top, int width, int height)
{
	assert(_cocoa_subdriver != NULL);

	_cocoa_subdriver->MakeDirty(left, top, width, height);
}

void VideoDriver_Cocoa::MainLoop()
{
	/* Start the main event loop */
	[ NSApp run ];
}

bool VideoDriver_Cocoa::ChangeResolution(int w, int h)
{
	assert(_cocoa_subdriver != NULL);

	bool ret = _cocoa_subdriver->ChangeResolution(w, h);

	QZ_GameSizeChanged();
	QZ_UpdateVideoModes();

	return ret;
}

bool VideoDriver_Cocoa::ToggleFullscreen(bool full_screen)
{
	assert(_cocoa_subdriver != NULL);

	bool oldfs = _cocoa_subdriver->IsFullscreen();
	if (full_screen != oldfs) {
		int width  = _cocoa_subdriver->GetWidth();
		int height = _cocoa_subdriver->GetHeight();
		int bpp    = BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth();

		delete _cocoa_subdriver;
		_cocoa_subdriver = NULL;

		_cocoa_subdriver = QZ_CreateSubdriver(width, height, bpp, full_screen, false);
		if (_cocoa_subdriver == NULL) {
			_cocoa_subdriver = QZ_CreateSubdriver(width, height, bpp, oldfs, true);
			if (_cocoa_subdriver == NULL) error("Cocoa: Failed to create subdriver");
		}
	}

	QZ_GameSizeChanged();
	QZ_UpdateVideoModes();

	return _cocoa_subdriver->IsFullscreen() == full_screen;
}


/* This is needed since sometimes assert is called before the videodriver is initialized */
void CocoaDialog(const char *title, const char *message, const char *buttonLabel)
{
	_cocoa_video_dialog = true;

	bool wasstarted = _cocoa_video_started;
	if (_video_driver == NULL) {
		setupApplication(); // Setup application before showing dialog
	} else if (!_cocoa_video_started && _video_driver->Start(NULL) != NULL) {
		fprintf(stderr, "%s: %s\n", title, message);
		return;
	}

	QZ_ShowMouse();
	NSRunAlertPanel([ NSString stringWithUTF8String:title ], [ NSString stringWithUTF8String:message ], [ NSString stringWithUTF8String:buttonLabel ], nil, nil);

	if (!wasstarted && _video_driver != NULL) _video_driver->Stop();

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
	_ottd_autorelease_pool = [ [ NSAutoreleasePool alloc ] init ];
}

void cocoaReleaseAutoreleasePool()
{
	[ _ottd_autorelease_pool release ];
}

#endif /* WITH_COCOA */
