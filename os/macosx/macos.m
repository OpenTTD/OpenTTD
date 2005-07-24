#include <AppKit/AppKit.h>

/*
 * This file contains objective C
 * Apple uses objective C instead of plain C to interact with OS specific/native functions
 *
 * Note: TrueLight's crosscompiler can handle this, but it likely needs a manual modification for each change in this file.
 * To insure that the crosscompiler still works, let him try any changes before they are committed
 */

void ShowMacDialog ( const char *title, const char *message, const char *buttonLabel )
{
	NSRunAlertPanel([NSString stringWithCString: title], [NSString stringWithCString: message], [NSString stringWithCString: buttonLabel], nil, nil);
}

