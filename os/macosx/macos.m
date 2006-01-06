#include <AppKit/AppKit.h>

/*
 * This file contains objective C
 * Apple uses objective C instead of plain C to interact with OS specific/native functions
 *
 * Note: TrueLight's crosscompiler can handle this, but it likely needs a manual modification for each change in this file.
 * To insure that the crosscompiler still works, let him try any changes before they are committed
 */


#ifdef WITH_SDL

void ShowMacDialog ( const char *title, const char *message, const char *buttonLabel )
{
	NSRunAlertPanel([NSString stringWithCString: title], [NSString stringWithCString: message], [NSString stringWithCString: buttonLabel], nil, nil);
}

#elif defined WITH_COCOA

void CocoaDialog ( const char *title, const char *message, const char *buttonLabel );

void ShowMacDialog ( const char *title, const char *message, const char *buttonLabel )
{
	CocoaDialog(title, message, buttonLabel);
}


#else

void ShowMacDialog ( const char *title, const char *message, const char *buttonLabel )
{
	fprintf(stderr, "%s: %s\n", title, message);
}

#endif

void ShowMacAssertDialog ( const char *function, const char *file, const int line, const char *expression )
{
	const char *buffer =
			[[NSString stringWithFormat:@"An assertion has failed and OpenTTD must quit.\n%s in %s (line %d)\n\"%s\"\n\nYou should report this error the OpenTTD developers if you think you found a bug.",
			function, file, line, expression] cString];
	NSLog(@"%s", buffer);
	ShowMacDialog( "Assertion Failed", buffer, "Quit" );

	// abort so that a debugger has a chance to notice
	abort();
}
