/* $Id$ */

#ifndef MACOS_H
#define MACOS_H

void ShowMacDialog ( const char *title, const char *message, const char *buttonLabel );
void ShowMacAssertDialog ( const char *function, const char *file, const int line, const char *expression );

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
