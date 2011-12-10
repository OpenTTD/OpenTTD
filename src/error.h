/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file error.h Functions related to errors. */

#ifndef ERROR_H
#define ERROR_H

#include "strings_type.h"

/** Message severity/type */
enum WarningLevel {
	WL_INFO,     ///< Used for DoCommand-like (and some nonfatal AI GUI) errors/information
	WL_WARNING,  ///< Other information
	WL_ERROR,    ///< Errors (eg. saving/loading failed)
	WL_CRITICAL, ///< Critical errors, the MessageBox is shown in all cases
};

void ShowErrorMessage(StringID summary_msg, StringID detailed_msg, WarningLevel wl, int x = 0, int y = 0, uint textref_stack_size = 0, const uint32 *textref_stack = NULL);

#endif /* ERROR_H */
