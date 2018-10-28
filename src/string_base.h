/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef STRING_BASE_H
#define STRING_BASE_H

#include "string_type.h"

/** Class for iterating over different kind of parts of a string. */
class StringIterator {
public:
	/** Type of the iterator. */
	enum IterType {
		ITER_CHARACTER, ///< Iterate over characters (or more exactly grapheme clusters).
		ITER_WORD,      ///< Iterate over words.
	};

	/** Sentinel to indicate end-of-iteration. */
	static const size_t END = SIZE_MAX;

	/**
	 * Create a new iterator instance.
	 * @return New iterator instance.
	 */
	static StringIterator *Create();

	virtual ~StringIterator() {}

	/**
	 * Set a new iteration string. Must also be called if the string contents
	 * changed. The cursor is reset to the start of the string.
	 * @param s New string.
	 */
	virtual void SetString(const char *s) = 0;

	/**
	 * Change the current string cursor.
	 * @param pos New cursor position.
	 * @return Actual new cursor position at the next valid character boundary.
	 * @pre pos has to be inside the current string.
	 */
	virtual size_t SetCurPosition(size_t pos) = 0;

	/**
	 * Advance the cursor by one iteration unit.
	 * @return New cursor position (in bytes) or #END if the cursor is already at the end of the string.
	 */
	virtual size_t Next(IterType what = ITER_CHARACTER) = 0;

	/**
	 * Move the cursor back by one iteration unit.
	 * @return New cursor position (in bytes) or #END if the cursor is already at the start of the string.
	 */
	virtual size_t Prev(IterType what = ITER_CHARACTER) = 0;

protected:
	StringIterator() {}
};

#endif /* STRING_BASE_H */
