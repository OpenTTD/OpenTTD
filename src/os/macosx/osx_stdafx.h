/* $Id$ */

/** @file osx_stdafx.h OSX is different on some places. */

#ifndef MACOS_STDAFX_H
#define MACOS_STDAFX_H

#include <CoreServices/CoreServices.h>
// remove the variables that CoreServices defines, but we define ourselves too
#undef bool
#undef false
#undef true

/* Name conflict */
#define GetTime		OTTD_GetTime

#define SL_ERROR OSX_SL_ERROR

#endif /* MACOS_STDAFX_H */
