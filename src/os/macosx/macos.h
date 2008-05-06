/* $Id$ */

/** @file macos.h Functions related to MacOS support. */

#ifndef MACOS_H
#define MACOS_H

/*
 * Functions to show the popup window
 * use ShowMacDialog when you want to control title, message and text on the button
 * ShowMacAssertDialog is used by assert
 * ShowMacErrorDialog should be used when an unrecoverable error shows up. It only contains the title, which will should tell what went wrong
 * the function then adds text that tells the user to update and then report the bug if it's present in the newest version
 * It also quits in a nice way since we call it when we know something happened that will crash OpenTTD (like a needed pointer turns out to be NULL or similar)
 */
void ShowMacDialog ( const char *title, const char *message, const char *buttonLabel );
void ShowMacAssertDialog ( const char *function, const char *file, const int line, const char *expression );
void ShowMacErrorDialog(const char *error);

// Since MacOS X users will never see an assert unless they started the game from a terminal
// we're using a custom assert(e) macro.
#undef assert

#ifdef NDEBUG
#define assert(e)       ((void)0)
#else

#define assert(e) \
		(__builtin_expect(!(e), 0) ? ShowMacAssertDialog ( __func__, __FILE__, __LINE__, #e ): (void)0 )
#endif



/**
 * Get the major version of Mac OS we are running under. Useful for things like the cocoa driver.
 * @return major version of the os. This would be 10 in the case of 10.4.11.
 */
long GetMacOSVersionMajor();

/**
 * Get the minor version of Mac OS we are running under. Useful for things like the cocoa driver.
 * @return minor version of the os. This would be 4 in the case of 10.4.11.
 */
long GetMacOSVersionMinor();

/**
 * Get the bugfix version of Mac OS we are running under. Useful for things like the cocoa driver.
 * @return bugfix version of the os. This would be 11 in the case of 10.4.11.
 */
long GetMacOSVersionBugfix();

/**
 * Check if we are at least running on the specified version of Mac OS.
 * @param major major version of the os. This would be 10 in the case of 10.4.11.
 * @param minor minor version of the os. This would be 4 in the case of 10.4.11.
 * @param bugfix bugfix version of the os. This would be 11 in the case of 10.4.11.
 * @return true if the running os is at least what we asked, false otherwise.
 */
static inline bool MacOSVersionIsAtLeast(long major, long minor, long bugfix)
{
	long maj = GetMacOSVersionMajor();
	long min = GetMacOSVersionMinor();
	long bf = GetMacOSVersionBugfix();

	if (maj < major) return false;
	if (maj == major && min < minor) return false;
	if (maj == major && min == minor && bf < bugfix) return false;

	return true;
}

#endif /* MACOS_H */
