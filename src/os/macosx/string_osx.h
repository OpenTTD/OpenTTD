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

#include "../../gfx_layout.h"
#include "../../string_base.h"
#include <vector>

/** String iterator using CoreText as a backend. */
class OSXStringIterator : public StringIterator {
	/** Break info for a character. */
	struct CharInfo {
		bool word_stop : 1; ///< Code point is suitable as a word break.
		bool char_stop : 1; ///< Code point is the start of a grapheme cluster, i.e. a "character".
	};

	std::vector<CharInfo> str_info;      ///< Break information for each code point.
	std::vector<size_t>   utf16_to_utf8; ///< Mapping from UTF-16 code point position to index in the UTF-8 source string.

	size_t cur_pos; ///< Current iteration position.

public:
	virtual void SetString(const char *s);
	virtual size_t SetCurPosition(size_t pos);
	virtual size_t Next(IterType what);
	virtual size_t Prev(IterType what);

	static StringIterator *Create();
};


void MacOSSetCurrentLocaleName(const char *iso_code);
int MacOSStringCompare(const char *s1, const char *s2);

#endif /* STRING_OSX_H */
