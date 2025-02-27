/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file stringfilter.cpp Searching and filtering using a stringterm. */

#include "stdafx.h"
#include "string_func.h"
#include "strings_func.h"
#include "stringfilter_type.h"
#include "gfx_func.h"

#include "safeguards.h"

static const char32_t STATE_WHITESPACE = ' ';
static const char32_t STATE_WORD = 'w';
static const char32_t STATE_QUOTE1 = '\'';
static const char32_t STATE_QUOTE2 = '"';

/**
 * Set the term to filter on.
 * @param str Filter term
 */
void StringFilter::SetFilterTerm(std::string_view str)
{
	this->word_index.clear();
	this->word_index.shrink_to_fit();
	this->word_matches = 0;

	char32_t state = STATE_WHITESPACE;
	auto pos = str.begin();
	auto word_begin = str.end();
	auto word_end = pos;

	/* Helper to prevent duplicating code. */
	auto add_word = [&] () {
		if (word_begin != str.end()) {
			this->word_index.emplace_back(std::string(word_begin, word_end + 1), false);
			word_begin = str.end();
		}
	};

	for (size_t len; pos < str.end(); pos += len) {
		char32_t c;
		len = Utf8Decode(&c, pos);

		if (state == STATE_WORD && IsWhitespace(c)) {
			/* Finish word */
			add_word();
			state = STATE_WHITESPACE;
			continue;
		}

		if (state == STATE_WHITESPACE) {
			/* Skip whitespace */
			if (IsWhitespace(c)) continue;
			state = STATE_WORD;
		}

		if (c == STATE_QUOTE1 || c == STATE_QUOTE2) {
			if (state == c) {
				/* Stop quoting */
				state = STATE_WORD;
				continue;
			} else if (state == STATE_WORD) {
				/* Start quoting */
				state = c;
				continue;
			}
		}

		/* Add to word */
		if (word_begin == str.end()) {
			word_begin = pos;
		}
		word_end = pos;
	}

	/* Add the last word of the string. */
	add_word();
}

/**
 * Reset the matching state to process a new item.
 */
void StringFilter::ResetState()
{
	this->word_matches = 0;
	for (WordState &ws : this->word_index) {
		ws.match = false;
	}
}

/**
 * Pass another text line from the current item to the filter.
 *
 * You can call this multiple times for a single item, if the filter shall apply to multiple things.
 * Before processing the next item you have to call ResetState().
 *
 * @param str Another line from the item.
 */
void StringFilter::AddLine(const char *str)
{
	if (str == nullptr) return;

	bool match_case = this->case_sensitive != nullptr && *this->case_sensitive;
	for (WordState &ws : this->word_index) {
		if (!ws.match) {
			if (this->locale_aware) {
				if (match_case ? StrNaturalContains(str, ws.word) : StrNaturalContainsIgnoreCase(str, ws.word)) {
					ws.match = true;
					this->word_matches++;
				}
			} else {
				if ((match_case ? strstr(str, ws.word.c_str()) : strcasestr(str, ws.word.c_str())) != nullptr) {
					ws.match = true;
					this->word_matches++;
				}
			}
		}
	}
}
