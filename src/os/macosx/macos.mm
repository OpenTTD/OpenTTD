/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../stdafx.h"
#include "../../core/bitmath_func.hpp"
#include "../../rev.h"

#define Rect  OTTDRect
#define Point OTTDPoint
#include <AppKit/AppKit.h>
#undef Rect
#undef Point

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/machine.h>

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

void ToggleFullScreen(bool fs);

static char *GetOSString()
{
	static char buffer[175];
	const char *CPU;
	char newgrf[125];
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

	/* Get the version of OSX */
	char OS[20];
	int version_major, version_minor, version_bugfix;
	GetMacOSVersion(&version_major, &version_minor, &version_bugfix);

	if (version_major != -1 && version_minor != -1 && version_bugfix != -1) {
		snprintf(OS, lengthof(OS), "%d.%d.%d", version_major, version_minor, version_bugfix);
	} else {
		snprintf(OS, lengthof(OS), "uncertain %d.%d.%d", version_major, version_minor, version_bugfix);
	}

	// make a list of used newgrf files
/*	if (_first_grffile != NULL) {
		char *n = newgrf;
		const GRFFile *file;

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

void ShowMacDialog(const char *title, const char *message, const char *buttonLabel)
{
	NSRunAlertPanel([NSString stringWithCString: title], [NSString stringWithCString: message], [NSString stringWithCString: buttonLabel], nil, nil);
}

#elif defined WITH_COCOA

void CocoaDialog(const char *title, const char *message, const char *buttonLabel);

void ShowMacDialog(const char *title, const char *message, const char *buttonLabel)
{
	CocoaDialog(title, message, buttonLabel);
}


#else

void ShowMacDialog(const char *title, const char *message, const char *buttonLabel)
{
	fprintf(stderr, "%s: %s\n", title, message);
}

#endif

void ShowMacAssertDialog(const char *function, const char *file, const int line, const char *expression)
{
	const char *buffer =
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
	const char *buffer =
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
	NSUserDefaults *defs = [NSUserDefaults standardUserDefaults];
	NSArray *languages = [defs objectForKey:@"AppleLanguages"];
	NSString *preferredLang = [languages objectAtIndex:0];
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


