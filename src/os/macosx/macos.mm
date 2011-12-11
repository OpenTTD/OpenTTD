/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file macos.mm Code related to MacOSX. */

#include "../../stdafx.h"
#include "../../core/bitmath_func.hpp"
#include "../../rev.h"
#include "macos.h"
#include "../../string_func.h"

#define Rect  OTTDRect
#define Point OTTDPoint
#include <AppKit/AppKit.h>
#undef Rect
#undef Point

/*
 * This file contains objective C
 * Apple uses objective C instead of plain C to interact with OS specific/native functions
 */


/**
 * Get the version of the MacOS we are running under. Code adopted
 * from http://www.cocoadev.com/index.pl?DeterminingOSVersion
 * @param return_major major version of the os. This would be 10 in the case of 10.4.11
 * @param return_minor minor version of the os. This would be 4 in the case of 10.4.11
 * @param return_bugfix bugfix version of the os. This would be 11 in the case of 10.4.11
 * A return value of -1 indicates that something went wrong and we don't know.
 */
void GetMacOSVersion(int *return_major, int *return_minor, int *return_bugfix)
{
	*return_major = -1;
	*return_minor = -1;
	*return_bugfix = -1;
	SInt32 systemVersion, version_major, version_minor, version_bugfix;
	if (Gestalt(gestaltSystemVersion, &systemVersion) == noErr) {
		if (systemVersion >= 0x1040) {
			if (Gestalt(gestaltSystemVersionMajor,  &version_major) == noErr) *return_major = (int)version_major;
			if (Gestalt(gestaltSystemVersionMinor,  &version_minor) == noErr) *return_minor = (int)version_minor;
			if (Gestalt(gestaltSystemVersionBugFix, &version_bugfix) == noErr) *return_bugfix = (int)version_bugfix;
		} else {
			*return_major = (int)(GB(systemVersion, 12, 4) * 10 + GB(systemVersion, 8, 4));
			*return_minor = (int)GB(systemVersion, 4, 4);
			*return_bugfix = (int)GB(systemVersion, 0, 4);
		}
	}
}

#ifdef WITH_SDL

/**
 * Show the system dialogue message (SDL on MacOSX).
 *
 * @param title Window title.
 * @param message Message text.
 * @param buttonLabel Button text.
 */
void ShowMacDialog(const char *title, const char *message, const char *buttonLabel)
{
	NSRunAlertPanel([ NSString stringWithUTF8String:title ], [ NSString stringWithUTF8String:message ], [ NSString stringWithUTF8String:buttonLabel ], nil, nil);
}

#elif defined WITH_COCOA

extern void CocoaDialog(const char *title, const char *message, const char *buttonLabel);

/**
 * Show the system dialogue message (Cocoa on MacOSX).
 *
 * @param title Window title.
 * @param message Message text.
 * @param buttonLabel Button text.
 */
void ShowMacDialog(const char *title, const char *message, const char *buttonLabel)
{
	CocoaDialog(title, message, buttonLabel);
}


#else

/**
 * Show the system dialogue message (console on MacOSX).
 *
 * @param title Window title.
 * @param message Message text.
 * @param buttonLabel Button text.
 */
void ShowMacDialog(const char *title, const char *message, const char *buttonLabel)
{
	fprintf(stderr, "%s: %s\n", title, message);
}

#endif


/**
 * Show an error message.
 *
 * @param buf error message text.
 * @param system message text originates from OS.
 */
void ShowOSErrorBox(const char *buf, bool system)
{
	/* Display the error in the best way possible. */
	if (system) {
		ShowMacDialog("OpenTTD has encountered an error", buf, "Quit");
	} else {
		ShowMacDialog(buf, "See the readme for more info.", "Quit");
	}
}

void OSOpenBrowser(const char *url)
{
	[ [ NSWorkspace sharedWorkspace ] openURL:[ NSURL URLWithString:[ NSString stringWithUTF8String:url ] ] ];
}

/**
 * Determine and return the current user's locale.
 */
const char *GetCurrentLocale(const char *)
{
	static char retbuf[32] = { '\0' };
	NSUserDefaults *defs = [ NSUserDefaults standardUserDefaults ];
	NSArray *languages = [ defs objectForKey:@"AppleLanguages" ];
	NSString *preferredLang = [ languages objectAtIndex:0 ];
	/* preferredLang is either 2 or 5 characters long ("xx" or "xx_YY"). */

	/* Since Apple introduced encoding to CString in OSX 10.4 we have to make a few conditions
	 * to get the right code for the used version of OSX. */
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4)
	if (MacOSVersionIsAtLeast(10, 4, 0)) {
		[ preferredLang getCString:retbuf maxLength:32 encoding:NSASCIIStringEncoding ];
	} else
#endif
	{
#if (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_4)
		/* maxLength does not include the \0 char in contrast to the call above. */
		[ preferredLang getCString:retbuf maxLength:31 ];
#endif
	}
	return retbuf;
}


#ifdef WITH_COCOA
/**
 * Return the contents of the clipboard (COCOA).
 *
 * @param buffer Clipboard content..
 * @param buff_len Length of the clipboard content..
 * @return Whether clipboard is empty or not.
 */
bool GetClipboardContents(char *buffer, size_t buff_len)
{
	NSPasteboard *pb = [ NSPasteboard generalPasteboard ];
	NSArray *types = [ NSArray arrayWithObject:NSStringPboardType ];
	NSString *bestType = [ pb availableTypeFromArray:types ];

	/* Clipboard has no text data available. */
	if (bestType == nil) return false;

	NSString *string = [ pb stringForType:NSStringPboardType ];
	if (string == nil || [ string length ] == 0) return false;

	ttd_strlcpy(buffer, [ string UTF8String ], buff_len);

	return true;
}
#endif

uint GetCPUCoreCount()
{
	uint count = 1;
#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
	if (MacOSVersionIsAtLeast(10, 5, 0)) {
		count = [ [ NSProcessInfo processInfo ] activeProcessorCount ];
	} else
#endif
	{
#if (MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5)
		count = MPProcessorsScheduled();
#endif
	}

	return count;
}
