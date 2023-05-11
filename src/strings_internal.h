/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strings_interal.h Types and functions related to the internal workings of formatting OpenTTD's strings. */

#ifndef STRINGS_INTERNAL_H
#define STRINGS_INTERNAL_H

/**
 * Equivalent to the std::back_insert_iterator in function, with some
 * convenience helpers for string concatenation.
 *
 * The formatter is currently backed by an external char buffer, and has some
 * extra functions to ease the migration from char buffers to std::string.
 */
class StringBuilder {
	char *current; ///< The current location to add strings
	const char *last; ///< The last element of the buffer.

public:
	/* Required type for this to be an output_iterator; mimics std::back_insert_iterator. */
	using value_type = void;
	using difference_type = void;
	using iterator_category = std::output_iterator_tag;
	using pointer = void;
	using reference = void;

	/**
	 * Create the builder of an external buffer.
	 * @param start The start location to write to.
	 * @param last  The last location to write to.
	 */
	StringBuilder(char *start, const char *last) : current(start), last(last) {}

	/* Required operators for this to be an output_iterator; mimics std::back_insert_iterator, which has no-ops. */
	StringBuilder &operator++() { return *this; }
	StringBuilder operator++(int) { return *this; }
	StringBuilder &operator*() { return *this; }

	/**
	 * Operator to add a character to the end of the buffer. Like the back
	 * insert iterators this also increases the position of the end of the
	 * buffer.
	 * @param value The character to add.
	 * @return Reference to this inserter.
	 */
	StringBuilder &operator=(const char value)
	{
		return this->operator+=(value);
	}

	/**
	 * Operator to add a character to the end of the buffer.
	 * @param value The character to add.
	 * @return Reference to this inserter.
	 */
	StringBuilder &operator+=(const char value)
	{
		if (this->current != this->last) *this->current++ = value;
		return *this;
	}

	/**
	 * Operator to append the given string to the output buffer.
	 * @param str The string to add.
	 * @return Reference to this inserter.
	 */
	StringBuilder &operator+=(const char *str)
	{
		this->current = strecpy(this->current, str, this->last);
		return *this;
	}

	/**
	 * Operator to append the given string to the output buffer.
	 * @param str The string to add.
	 * @return Reference to this inserter.
	 */
	StringBuilder &operator+=(const std::string &str)
	{
		return this->operator+=(str.c_str());
	}

	/**
	 * Encode the given Utf8 character into the output buffer.
	 * @param c The character to encode.
	 * @return true iff there was enough space and the character got added.
	 */
	bool Utf8Encode(WChar c)
	{
		if (this->Remaining() < Utf8CharLen(c)) return false;

		this->current += ::Utf8Encode(this->current, c);
		return true;
	}

	/**
	 * Get the pointer to the this->last written element in the buffer.
	 * This call does '\0' terminate the string, whereas other calls do not
	 * (necessarily) do this.
	 * @return The this->current end of the string.
	 */
	char *GetEnd()
	{
		*this->current = '\0';
		return this->current;
	}

	/**
	 * Get the remaining number of characters that can be placed.
	 * @return The number of characters.
	 */
	ptrdiff_t Remaining()
	{
		return (ptrdiff_t)(this->last - this->current);
	}
};

#endif /* STRINGS_INTERNAL_H */
