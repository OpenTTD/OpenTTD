/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_compare_type.hpp Comparator class for "const char *" so it can be used as a key for std::map */

#ifndef STRING_COMPARE_TYPE_HPP
#define STRING_COMPARE_TYPE_HPP

/** Comparator for strings. */
struct StringCompare {
	/**
	 * Compare two strings.
	 * @param a The first string.
	 * @param b The second string.
	 * @return True is the first string is deemed "lower" than the second string.
	 */
	bool operator () (const char *a, const char *b) const
	{
		return strcmp(a, b) < 0;
	}
};

#endif /* STRING_COMPARE_TYPE_HPP */
