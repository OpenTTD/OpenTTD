/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_osx.h Functions related to localized text support on OSX. */

#ifndef STRING_OSX_H
#define STRING_OSX_H


void MacOSSetCurrentLocaleName(const char *iso_code);
int MacOSStringCompare(const char *s1, const char *s2);

#endif /* STRING_OSX_H */
