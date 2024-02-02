/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strings_interal.h Types and functions related to the internal workings of formatting OpenTTD's strings. */

#ifndef STRINGS_INTERNAL_H
#define STRINGS_INTERNAL_H

#include "strings_func.h"
#include "string_func.h"

/**
 * Equivalent to the std::back_insert_iterator in function, with some
 * convenience helpers for string concatenation.
 */
class StringBuilder {
	std::string *string;

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
	StringBuilder(std::string &string) : string(&string) {}

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
		this->string->push_back(value);
		return *this;
	}

	/**
	 * Operator to append the given string to the output buffer.
	 * @param str The string to add.
	 * @return Reference to this inserter.
	 */
	StringBuilder &operator+=(std::string_view str)
	{
		*this->string += str;
		return *this;
	}

	/**
	 * Encode the given Utf8 character into the output buffer.
	 * @param c The character to encode.
	 */
	void Utf8Encode(char32_t c)
	{
		auto iterator = std::back_inserter(*this->string);
		::Utf8Encode(iterator, c);
	}

	/**
	 * Remove the given amount of characters from the back of the string.
	 * @param amount The amount of characters to remove.
	 * @return true iff there was enough space and the character got added.
	 */
	void RemoveElementsFromBack(size_t amount)
	{
		this->string->erase(this->string->size() - std::min(amount, this->string->size()));
	}

	/**
	 * Get the current index in the string.
	 * @return The index.
	 */
	size_t CurrentIndex()
	{
		return this->string->size();
	}

	/**
	 * Get the reference to the character at the given index.
	 * @return The reference to the character.
	 */
	char &operator[](size_t index)
	{
		return (*this->string)[index];
	}
};

void GetStringWithArgs(StringBuilder &builder, StringID string, StringParameters &args, uint case_index = 0, bool game_script = false);
std::string GetStringWithArgs(StringID string, StringParameters &args);

/* Do not leak the StringBuilder to everywhere. */
void GenerateTownNameString(StringBuilder &builder, size_t lang, uint32_t seed);
void GetTownName(StringBuilder &builder, const struct Town *t);
void GRFTownNameGenerate(StringBuilder &builder, uint32_t grfid, uint16_t gen, uint32_t seed);

uint RemapNewGRFStringControlCode(uint scc, const char **str, StringParameters &parameters, bool modify_parameters);

#endif /* STRINGS_INTERNAL_H */
