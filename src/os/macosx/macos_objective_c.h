/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file macos_objective_c.h Includes of mac os specific headers wich contain objective c. */

#ifndef MACOS_OBJECTIVE_C_H
#define MACOS_OBJECTIVE_C_H

/** Macro that prevents name conflicts between included headers. */
#define Rect OTTDRect
/** Macro that prevents name conflicts between included headers. */
#define Point OTTDPoint
#include <AppKit/AppKit.h>

#ifdef WITH_COCOA
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#endif /* WITH_COCOA */

#undef Rect
#undef Point

#endif /* MACOS_OBJECTIVE_C_H */

