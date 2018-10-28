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

/**
 * Helper class to construct a new #CoreTextParagraphLayout.
 */
class CoreTextParagraphLayoutFactory {
public:
	/** Helper for GetLayouter, to get the right type. */
	typedef UniChar CharType;
	/** Helper for GetLayouter, to get whether the layouter supports RTL. */
	static const bool SUPPORTS_RTL = true;

	/**
	 * Get the actual ParagraphLayout for the given buffer.
	 * @param buff The begin of the buffer.
	 * @param buff_end The location after the last element in the buffer.
	 * @param fontMapping THe mapping of the fonts.
	 * @return The ParagraphLayout instance.
	 */
	static ParagraphLayouter *GetParagraphLayout(CharType *buff, CharType *buff_end, FontMap &fontMapping);

	/**
	 * Append a wide character to the internal buffer.
	 * @param buff        The buffer to append to.
	 * @param buffer_last The end of the buffer.
	 * @param c           The character to add.
	 * @return The number of buffer spaces that were used.
	 */
	static size_t AppendToBuffer(CharType *buff, const CharType *buffer_last, WChar c)
	{
		if (c >= 0x010000U) {
			/* Character is encoded using surrogates in UTF-16. */
			if (buff + 1 <= buffer_last) {
				buff[0] = (CharType)(((c - 0x010000U) >> 10) + 0xD800);
				buff[1] = (CharType)(((c - 0x010000U) & 0x3FF) + 0xDC00);
			} else {
				/* Not enough space in buffer. */
				*buff = 0;
			}
			return 2;
		} else {
			*buff = (CharType)(c & 0xFFFF);
			return 1;
		}
	}
};

void MacOSResetScriptCache(FontSize size);
void MacOSSetCurrentLocaleName(const char *iso_code);
int MacOSStringCompare(const char *s1, const char *s2);

#endif /* STRING_OSX_H */
