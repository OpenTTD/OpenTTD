/* $Id$ */

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
#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
	void ShowMacDialog ( const char *title, const char *message, const char *buttonLabel );
	void ShowMacAssertDialog ( const char *function, const char *file, const int line, const char *expression );
	void ShowMacErrorDialog(const char *error);
#ifdef __cplusplus
}
#endif //__cplusplus

// Since MacOS X users will never see an assert unless they started the game from a terminal
// we're using a custom assert(e) macro.
#undef assert

#ifdef NDEBUG
#define assert(e)       ((void)0)
#else

#define assert(e) \
		(__builtin_expect(!(e), 0) ? ShowMacAssertDialog ( __func__, __FILE__, __LINE__, #e ): (void)0 )
#endif

#endif /* MACOS_H */
