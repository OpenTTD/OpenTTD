/* $Id$ */

/******************************************************************************
 *                             Cocoa video driver                             *
 * Known things left to do:                                                   *
 *  Nothing at the moment.                                                    *
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


/* Portions of CPS.h */
struct CPSProcessSerNum {
	UInt32 lo;
	UInt32 hi;
};

extern "C" OSErr CPSGetCurrentProcess(CPSProcessSerNum* psn);
extern "C" OSErr CPSEnableForegroundOperation(CPSProcessSerNum* psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern "C" OSErr CPSSetFrontProcess(CPSProcessSerNum* psn);

/* Disables a warning. This is needed since the method exists but has been dropped from the header, supposedly as of 10.4. */
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4)
@interface NSApplication(NSAppleMenu)
- (void)setAppleMenu:(NSMenu *)menu;
@end
#endif


/* Defined in stdbool.h */
#ifndef __cplusplus
# ifndef __BEOS__
#  undef bool
#  undef false
#  undef true
# endif
#endif


#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../debug.h"
#include "../../variables.h"
#include "../../core/geometry_type.hpp"
#include "cocoa_v.h"
#include "../../blitter/factory.hpp"
#include "../../fileio.h"
#include "../../gfx_func.h"
#include "../../functions.h"


@interface OTTDMain : NSObject
@end


static NSAutoreleasePool *_ottd_autorelease_pool;
static OTTDMain *_ottd_main;
static bool _cocoa_video_started = false;
static bool _cocoa_video_dialog = false;

CocoaSubdriver* _cocoa_subdriver = NULL;



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


static void QZ_UpdateVideoModes()
{
	uint i, count;
	OTTD_Point modes[32];

	assert(_cocoa_subdriver != NULL);

	count = _cocoa_subdriver->ListModes(modes, lengthof(modes));

	for (i = 0; i < count; i++) {
		_resolutions[i][0] = modes[i].x;
		_resolutions[i][1] = modes[i].y;
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
	_fullscreen = _cocoa_subdriver->IsFullscreen();

	GameSizeChanged();
}


static CocoaSubdriver *QZ_CreateWindowSubdriver(int width, int height, int bpp)
{
	CocoaSubdriver *ret;

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
	 * If we get here we are running 10.4 or earlier and either openttd was compiled without the quickdraw driver
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
	CocoaSubdriver *ret;

	ret = fullscreen ? QZ_CreateFullscreenSubdriver(width, height, bpp) : QZ_CreateWindowSubdriver(width, height, bpp);
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

	[_ottd_main release];

	_cocoa_video_started = false;
}

const char *VideoDriver_Cocoa::Start(const char * const *parm)
{
	int width, height, bpp;

	if (!MacOSVersionIsAtLeast(10, 3, 0)) return "The Cocoa video driver requires Mac OS X 10.3 or later.";

	if (_cocoa_video_started) return "Already started";
	_cocoa_video_started = true;

	setupApplication();

	/* Don't create a window or enter fullscreen if we're just going to show a dialog. */
	if (_cocoa_video_dialog) return NULL;

	width = _cur_resolution[0];
	height = _cur_resolution[1];
	bpp = BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth();

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
	[NSApp run];
}

bool VideoDriver_Cocoa::ChangeResolution(int w, int h)
{
	bool ret;

	assert(_cocoa_subdriver != NULL);

	ret = _cocoa_subdriver->ChangeResolution(w, h);

	QZ_GameSizeChanged();

	QZ_UpdateVideoModes();

	return ret;
}

bool VideoDriver_Cocoa::ToggleFullscreen(bool full_screen)
{
	bool oldfs;

	assert(_cocoa_subdriver != NULL);

	oldfs = _cocoa_subdriver->IsFullscreen();
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
void CocoaDialog(const char* title, const char* message, const char* buttonLabel)
{
	bool wasstarted;

	_cocoa_video_dialog = true;

	wasstarted = _cocoa_video_started;
	if (_video_driver == NULL) {
		setupApplication(); // Setup application before showing dialog
	} else if (!_cocoa_video_started && _video_driver->Start(NULL) != NULL) {
		fprintf(stderr, "%s: %s\n", title, message);
		return;
	}

	NSRunAlertPanel([NSString stringWithCString: title], [NSString stringWithCString: message], [NSString stringWithCString: buttonLabel], nil, nil);

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
	_ottd_autorelease_pool = [[NSAutoreleasePool alloc] init];
}

void cocoaReleaseAutoreleasePool()
{
	[_ottd_autorelease_pool release];
}

#endif /* WITH_COCOA */
