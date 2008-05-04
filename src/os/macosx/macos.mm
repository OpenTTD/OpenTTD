/* $Id$ */

#include <AvailabilityMacros.h>

#include <AppKit/AppKit.h>

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/machine.h>
#include <stdio.h>
#include "../../stdafx.h"
#include "../../core/bitmath_func.hpp"
#include "../../rev.h"

#ifndef CPU_SUBTYPE_POWERPC_970
#define CPU_SUBTYPE_POWERPC_970 ((cpu_subtype_t) 100)
#endif

/*
 * This file contains objective C
 * Apple uses objective C instead of plain C to interact with OS specific/native functions
 *
 * Note: TrueLight's crosscompiler can handle this, but it likely needs a manual modification for each change in this file.
 * To insure that the crosscompiler still works, let him try any changes before they are committed
 */

void ToggleFullScreen(bool fs);

static char *GetOSString()
{
	static char buffer[175];
	const char* CPU;
	char OS[20];
	char newgrf[125];
	long sysVersion;

	// get the hardware info
	host_basic_info_data_t hostInfo;
	mach_msg_type_number_t infoCount;

	infoCount = HOST_BASIC_INFO_COUNT;
	host_info(
		mach_host_self(), HOST_BASIC_INFO, (host_info_t)&hostInfo, &infoCount
	);

	// replace the hardware info with strings, that tells a bit more than just an int
	switch (hostInfo.cpu_subtype) {
#ifdef __POWERPC__
		case CPU_SUBTYPE_POWERPC_750:  CPU = "G3"; break;
		case CPU_SUBTYPE_POWERPC_7400:
		case CPU_SUBTYPE_POWERPC_7450: CPU = "G4"; break;
		case CPU_SUBTYPE_POWERPC_970:  CPU = "G5"; break;
		default:                       CPU = "Unknown PPC"; break;
#else
		/* it looks odd to have a switch for two cases, but it leaves room for easy
		 * expansion. Odds are that Apple will some day use newer CPUs than i686
		 */
		case CPU_SUBTYPE_PENTPRO: CPU = "i686"; break;
		default:                  CPU = "Unknown Intel"; break;
#endif
	}

	// get the version of OSX
	if (Gestalt(gestaltSystemVersion, &sysVersion) != noErr) {
		sprintf(OS, "Undetected");
	} else {
		int majorHiNib = GB(sysVersion, 12, 4);
		int majorLoNib = GB(sysVersion,  8, 4);
		int minorNib   = GB(sysVersion,  4, 4);
		int bugNib     = GB(sysVersion,  0, 4);

		sprintf(OS, "%d%d.%d.%d", majorHiNib, majorLoNib, minorNib, bugNib);
	}

	// make a list of used newgrf files
/*	if (_first_grffile != NULL) {
		char* n = newgrf;
		const GRFFile* file;

		for (file = _first_grffile; file != NULL; file = file->next) {
			n = strecpy(n, " ", lastof(newgrf));
			n = strecpy(n, file->filename, lastof(newgrf));
		}
	} else {*/
		sprintf(newgrf, "none");
//	}

	snprintf(
		buffer, lengthof(buffer),
		"Please add this info: (tip: copy-paste works)\n"
		"CPU: %s, OSX: %s, OpenTTD version: %s\n"
		"NewGRF files:%s",
		CPU, OS, _openttd_revision, newgrf
	);
	return buffer;
}


#ifdef WITH_SDL

void ShowMacDialog(const char* title, const char* message, const char* buttonLabel)
{
	NSRunAlertPanel([NSString stringWithCString: title], [NSString stringWithCString: message], [NSString stringWithCString: buttonLabel], nil, nil);
}

#elif defined WITH_COCOA

void CocoaDialog(const char* title, const char* message, const char* buttonLabel);

void ShowMacDialog(const char* title, const char* message, const char* buttonLabel)
{
	CocoaDialog(title, message, buttonLabel);
}


#else

void ShowMacDialog(const char* title, const char* message, const char* buttonLabel)
{
	fprintf(stderr, "%s: %s\n", title, message);
}

#endif

void ShowMacAssertDialog(const char* function, const char* file, const int line, const char* expression)
{
	const char* buffer =
		[[NSString stringWithFormat:@
			"An assertion has failed and OpenTTD must quit.\n"
			"%s in %s (line %d)\n"
			"\"%s\"\n"
			"\n"
			"You should report this error the OpenTTD developers if you think you found a bug.\n"
			"\n"
			"%s",
			function, file, line, expression, GetOSString()] cString
		];
	NSLog(@"%s", buffer);
	ToggleFullScreen(0);
	ShowMacDialog("Assertion Failed", buffer, "Quit");

	// abort so that a debugger has a chance to notice
	abort();
}


void ShowMacErrorDialog(const char *error)
{
	const char* buffer =
		[[NSString stringWithFormat:@
			"Please update to the newest version of OpenTTD\n"
			"If the problem presists, please report this to\n"
			"http://bugs.openttd.org\n"
			"\n"
			"%s",
			GetOSString()] cString
		];
	ToggleFullScreen(0);
	ShowMacDialog(error, buffer, "Quit");
	abort();
}


/** Determine the current user's locale. */
const char *GetCurrentLocale(const char *)
{
	static char retbuf[32] = { '\0' };
	NSUserDefaults* defs = [NSUserDefaults standardUserDefaults];
	NSArray* languages = [defs objectForKey:@"AppleLanguages"];
	NSString* preferredLang = [languages objectAtIndex:0];
	/* preferredLang is either 2 or 5 characters long ("xx" or "xx_YY"). */

	/* MacOS 10.3.9 can't handle encoding:NSASCIIStringEncoding
	 * we will completely disable compiling it for such old targets to avoid a warning */
#if (MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3)
	/* Note: MAC_OS_X_VERSION_MAX_ALLOWED is the current OSX version/SDK by default */
	if (MacOSVersionIsAtLeast(10, 4, 0)) {
		[ preferredLang getCString:retbuf maxLength:32 encoding:NSASCIIStringEncoding ];
	} else {
#else
	/* 10.3.9 needs to start the { too */
	{
#endif
		[ preferredLang getCString:retbuf maxLength:32 ];
	}
	return retbuf;
}


/*
 * This will only give an accurate result for versions before OS X 10.8 since it uses bcd encoding
 * for the minor and bugfix version numbers and a scheme of representing all numbers from 9 and up
 * with 9. This means we can't tell OS X 10.9 from 10.9 or 10.11. Please use GetMacOSVersionMajor()
 * and GetMacOSVersionMinor() instead.
 */
static long GetMacOSVersion()
{
	static long sysVersion = -1;

	if (sysVersion != -1) return sysVersion;

	if (Gestalt(gestaltSystemVersion, &sysVersion) != noErr) sysVersion = -1;
	 return sysVersion;
}

long GetMacOSVersionMajor()
{
	static long sysVersion = -1;

	if (sysVersion != -1) return sysVersion;

	sysVersion = GetMacOSVersion();
	if (sysVersion == -1) return -1;

	if (sysVersion >= 0x1040) {
		if (Gestalt(gestaltSystemVersionMajor, &sysVersion) != noErr) sysVersion = -1;
	} else {
		sysVersion = GB(sysVersion, 12, 4) * 10 + GB(sysVersion,  8, 4);
	}

	return sysVersion;
}

long GetMacOSVersionMinor()
{
	static long sysVersion = -1;

	if (sysVersion != -1) return sysVersion;

	sysVersion = GetMacOSVersion();
	if (sysVersion == -1) return -1;

	if (sysVersion >= 0x1040) {
		if (Gestalt(gestaltSystemVersionMinor, &sysVersion) != noErr) sysVersion = -1;
	} else {
		sysVersion = GB(sysVersion,  4, 4);
	}

	return sysVersion;
}

long GetMacOSVersionBugfix()
{
	static long sysVersion = -1;

	if (sysVersion != -1) return sysVersion;

	sysVersion = GetMacOSVersion();
	if (sysVersion == -1) return -1;

	if (sysVersion >= 0x1040) {
		if (Gestalt(gestaltSystemVersionBugFix, &sysVersion) != noErr) sysVersion = -1;
	} else {
		sysVersion = GB(sysVersion,  0, 4);
	}

	return sysVersion;
}
