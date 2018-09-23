/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_osx.cpp Functions related to localized text support on OSX. */

#include "../../stdafx.h"
#include "string_osx.h"
#include "macos.h"

#include <CoreFoundation/CoreFoundation.h>


#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
static CFLocaleRef _osx_locale = NULL;

/** Store current language locale as a CoreFounation locale. */
void MacOSSetCurrentLocaleName(const char *iso_code)
{
	if (!MacOSVersionIsAtLeast(10, 5, 0)) return;

	if (_osx_locale != NULL) CFRelease(_osx_locale);

	CFStringRef iso = CFStringCreateWithCString(kCFAllocatorNull, iso_code, kCFStringEncodingUTF8);
	_osx_locale = CFLocaleCreate(kCFAllocatorDefault, iso);
	CFRelease(iso);
}

/**
 * Compares two strings using case insensitive natural sort.
 *
 * @param s1 First string to compare.
 * @param s2 Second string to compare.
 * @return 1 if s1 < s2, 2 if s1 == s2, 3 if s1 > s2, or 0 if not supported by the OS.
 */
int MacOSStringCompare(const char *s1, const char *s2)
{
	static bool supported = MacOSVersionIsAtLeast(10, 5, 0);
	if (!supported) return 0;

	CFStringCompareFlags flags = kCFCompareCaseInsensitive | kCFCompareNumerically | kCFCompareLocalized | kCFCompareWidthInsensitive | kCFCompareForcedOrdering;

	CFStringRef cf1 = CFStringCreateWithCString(kCFAllocatorDefault, s1, kCFStringEncodingUTF8);
	CFStringRef cf2 = CFStringCreateWithCString(kCFAllocatorDefault, s2, kCFStringEncodingUTF8);

	CFComparisonResult res = CFStringCompareWithOptionsAndLocale(cf1, cf2, CFRangeMake(0, CFStringGetLength(cf1)), flags, _osx_locale);

	CFRelease(cf1);
	CFRelease(cf2);

	return (int)res + 2;
}

#else
void MacOSSetCurrentLocaleName(const char *iso_code) {}

int MacOSStringCompare(const char *s1, const char *s2)
{
	return 0;
}
#endif /* (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5) */
