/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file macos.mm Code related to MacOSX.
 *
 * This file contains objective C.
 * Apple uses objective C instead of plain C to interact with OS specific/native functions.
 */

#include "../../stdafx.h"
#include "../../core/bitmath_func.hpp"
#include "../../rev.h"
#include "macos.h"
#include "../../string_func.h"
#include "../../fileio_func.h"
#include <pthread.h>
#include "macos_objective_c.h"

#ifdef WITH_COCOA
static NSAutoreleasePool *_ottd_autorelease_pool;
#endif

/**
 * Get the version of the MacOS we are running under.
 * @return Tuple with major, minor and patch of the MacOS version.
 */
std::tuple<int, int, int> GetMacOSVersion()
{
	NSOperatingSystemVersion ver = [ [ NSProcessInfo processInfo ] operatingSystemVersion ];
	return { static_cast<int>(ver.majorVersion), static_cast<int>(ver.minorVersion), static_cast<int>(ver.patchVersion) };
}

#ifdef WITH_COCOA
extern void CocoaDialog(std::string_view title, std::string_view message, std::string_view buttonLabel);
#endif

/**
 * Show the system dialogue message, uses Cocoa if available and console otherwise.
 * @param title Window title.
 * @param message Message text.
 * @param buttonLabel Button text.
 */
void ShowMacDialog(std::string_view title, std::string_view message, std::string_view buttonLabel)
{
#ifdef WITH_COCOA
	CocoaDialog(title, message, buttonLabel);
#else
	fmt::print(stderr, "{}: {}\n", title, message);
#endif
}

/**
 * Show an error message.
 * @param buf Text with error message.
 * @param system Whether message text originates from OS.
 */
void ShowOSErrorBox(std::string_view buf, bool system)
{
	/* Display the error in the best way possible. */
	if (system) {
		ShowMacDialog("OpenTTD has encountered an error", buf, "Quit");
	} else {
		ShowMacDialog(buf, "See the readme for more info.", "Quit");
	}
}

/**
 * Opens browser on MacOS.
 * @param url Web page address to open.
 */
void OSOpenBrowser(const std::string &url)
{
	[ [ NSWorkspace sharedWorkspace ] openURL:[ NSURL URLWithString:[ NSString stringWithUTF8String:url.c_str() ] ] ];
}

/**
 * Determine and return the current user's charset.
 * @return String containing current charset, or std::nullopt if not-determinable.
 */
std::optional<std::string> GetCurrentLocale(const char *)
{
	NSUserDefaults *defs = [ NSUserDefaults standardUserDefaults ];
	NSArray *languages = [ defs objectForKey:@"AppleLanguages" ];
	NSString *preferredLang = [ languages objectAtIndex:0 ];
	/* preferredLang is either 2 or 5 characters long ("xx" or "xx_YY"). */

	std::string retbuf{32, '\0'};
	[ preferredLang getCString:retbuf.data() maxLength:retbuf.size() encoding:NSASCIIStringEncoding ];
	auto end = retbuf.find('\0');
	if (end == 0) return std::nullopt;
	if (end != std::string::npos) retbuf.erase(end);
	return retbuf;
}


#ifdef WITH_COCOA
/**
 * Return the contents of the clipboard (COCOA).
 *
 * @return The (optional) clipboard contents.
 */
std::optional<std::string> GetClipboardContents()
{
	NSPasteboard *pb = [ NSPasteboard generalPasteboard ];
	NSArray *types = [ NSArray arrayWithObject:NSPasteboardTypeString ];
	NSString *bestType = [ pb availableTypeFromArray:types ];

	/* Clipboard has no text data available. */
	if (bestType == nil) return std::nullopt;

	NSString *string = [ pb stringForType:NSPasteboardTypeString ];
	if (string == nil || [ string length ] == 0) return std::nullopt;

	return [ string UTF8String ];
}

/** Set the application's bundle directory.
 *
 * This is needed since OS X application bundles do not have a
 * current directory and the data files are 'somewhere' in the bundle.
 */
void CocoaSetApplicationBundleDir()
{
	extern std::array<std::string, NUM_SEARCHPATHS> _searchpaths;

	char tmp[MAXPATHLEN];
	CFAutoRelease<CFURLRef> url(CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle()));
	if (CFURLGetFileSystemRepresentation(url.get(), true, (unsigned char *)tmp, MAXPATHLEN)) {
		_searchpaths[SP_APPLICATION_BUNDLE_DIR] = tmp;
		AppendPathSeparator(_searchpaths[SP_APPLICATION_BUNDLE_DIR]);
	} else {
		_searchpaths[SP_APPLICATION_BUNDLE_DIR].clear();
	}
}

/**
 * Setup autorelease for the application pool.
 *
 * These are called from main() to prevent a _NSAutoreleaseNoPool error when
 * exiting before the cocoa video driver has been loaded
 */
void CocoaSetupAutoreleasePool()
{
	_ottd_autorelease_pool = [ [ NSAutoreleasePool alloc ] init ];
}

/**
 * Autorelease the application pool.
 */
void CocoaReleaseAutoreleasePool()
{
	[ _ottd_autorelease_pool release ];
}

#endif

/**
 * Check if a font is a monospace font.
 * @param name Name of the font.
 * @return True if the font is a monospace font.
 */
bool IsMonospaceFont(CFStringRef name)
{
	NSFont *font = [ NSFont fontWithName:(__bridge NSString *)name size:0.0f ];

	return font != nil ? [ font isFixedPitch ] : false;
}

/**
 * Set the name of the current thread for the debugger.
 * @param name The new name of the current thread.
 */
void MacOSSetThreadName(const std::string &name)
{
	pthread_setname_np(name.c_str());

	NSThread *cur = [ NSThread currentThread ];
	if (cur != nil && [ cur respondsToSelector:@selector(setName:) ]) {
		[ cur performSelector:@selector(setName:) withObject:[ NSString stringWithUTF8String:name.c_str() ] ];
	}
}

/**
 * Ask OS how much RAM it has physically attached.
 * @return Number of available bytes.
 */
uint64_t MacOSGetPhysicalMemory()
{
	return [ [ NSProcessInfo processInfo ] physicalMemory ];
}
