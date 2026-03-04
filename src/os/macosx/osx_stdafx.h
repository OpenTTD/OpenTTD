/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file osx_stdafx.h OSX is different on some places. */

#ifndef MACOS_STDAFX_H
#define MACOS_STDAFX_H


#include <AvailabilityMacros.h>

#define __STDC_LIMIT_MACROS
#include <stdint.h>

/* Some gcc versions include assert.h via this header. As this would interfere
 * with our own assert redefinition, include this header first. */
#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3))
#	include <debug/debug.h>
#endif

/* Check for mismatching 'architectures' */
#if !defined(STRGEN) && !defined(SETTINGSGEN) && ((defined(__LP64__) && !defined(POINTER_IS_64BIT)) || (!defined(__LP64__) && defined(POINTER_IS_64BIT)))
#	error "Compiling 64 bits without POINTER_IS_64BIT set! (or vice versa)"
#endif

/* Name conflict */
#define Rect        OTTDRect
#define Point       OTTDPoint
#define WindowClass OTTDWindowClass
#define ScriptOrder OTTDScriptOrder
#define Palette     OTTDPalette
#define GlyphID     OTTDGlyphID

#include <CoreServices/CoreServices.h>
#include <ApplicationServices/ApplicationServices.h>

#undef Rect
#undef Point
#undef WindowClass
#undef ScriptOrder
#undef Palette
#undef GlyphID

/* remove the variables that CoreServices defines, but we define ourselves too */
#undef bool
#undef false
#undef true

/* Name conflict */
#define GetTime OTTD_GetTime

#define SL_ERROR OSX_SL_ERROR

/* NSInteger and NSUInteger are part of 10.5 and higher. */
#ifndef NSInteger
#ifdef __LP64__
typedef long NSInteger;
typedef unsigned long NSUInteger;
#else
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif /* __LP64__ */
#endif /* NSInteger */

#ifndef CGFLOAT_DEFINED
#ifdef __LP64__
typedef double CGFloat;
#else
typedef float CGFloat;
#endif /* __LP64__ */
#endif /* CGFLOAT_DEFINED */

/** OS X has a non-const iconv. */
#define HAVE_NON_CONST_ICONV

#endif /* MACOS_STDAFX_H */
