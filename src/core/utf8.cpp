/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file utf8.cpp Handling of UTF-8 encoded data.
 */

#include "../stdafx.h"
#include "utf8.hpp"
#include "../safeguards.h"

/**
 * Encode a character to UTF-8.
 * @param c Character
 * @return Binary data and length. Length is zero, if "c" is out of range.
 */
[[nodiscard]] std::pair<char[4], size_t> EncodeUtf8(char32_t c)
{
	std::pair<char[4], size_t> result{};
	auto &[buf, len] = result;
	if (c < 0x80) {
		buf[len++] = c;
	} else if (c < 0x800) {
		buf[len++] = 0xC0 + GB(c,  6, 5);
		buf[len++] = 0x80 + GB(c,  0, 6);
	} else if (c < 0x10000) {
		buf[len++] = 0xE0 + GB(c, 12, 4);
		buf[len++] = 0x80 + GB(c,  6, 6);
		buf[len++] = 0x80 + GB(c,  0, 6);
	} else if (c < 0x110000) {
		buf[len++] = 0xF0 + GB(c, 18, 3);
		buf[len++] = 0x80 + GB(c, 12, 6);
		buf[len++] = 0x80 + GB(c,  6, 6);
		buf[len++] = 0x80 + GB(c,  0, 6);
	}
	return result;
}

/**
 * Decode a character from UTF-8.
 * @param buf Binary data.
 * @return Length and character. Length is zero, if the input data is invalid.
 */
[[nodiscard]] std::pair<size_t, char32_t> DecodeUtf8(std::string_view buf)
{
	if (buf.size() >= 1 && !HasBit(buf[0], 7)) {
		/* Single byte character: 0xxxxxxx */
		char32_t c = buf[0];
		return {1, c};
	} else if (buf.size() >= 2 && GB(buf[0], 5, 3) == 6) {
		if (IsUtf8Part(buf[1])) {
			/* Double byte character: 110xxxxx 10xxxxxx */
			char32_t c = GB(buf[0], 0, 5) << 6 | GB(buf[1], 0, 6);
			if (c >= 0x80) return {2, c};
		}
	} else if (buf.size() >= 3 && GB(buf[0], 4, 4) == 14) {
		if (IsUtf8Part(buf[1]) && IsUtf8Part(buf[2])) {
			/* Triple byte character: 1110xxxx 10xxxxxx 10xxxxxx */
			char32_t c = GB(buf[0], 0, 4) << 12 | GB(buf[1], 0, 6) << 6 | GB(buf[2], 0, 6);
			if (c >= 0x800) return {3, c};
		}
	} else if (buf.size() >= 4 && GB(buf[0], 3, 5) == 30) {
		if (IsUtf8Part(buf[1]) && IsUtf8Part(buf[2]) && IsUtf8Part(buf[3])) {
			/* 4 byte character: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
			char32_t c = GB(buf[0], 0, 3) << 18 | GB(buf[1], 0, 6) << 12 | GB(buf[2], 0, 6) << 6 | GB(buf[3], 0, 6);
			if (c >= 0x10000 && c <= 0x10FFFF) return {4, c};
		}
	}
	return {};
}

/**
 * Create iterator pointing at codepoint, which occupies the byte position "offset".
 * "offset" does not need to point at the first byte of the UTF-8 sequence,
 * the iterator will still address the correct position of the first byte.
 * @param offset Byte offset into view.
 * @return Iterator pointing at start of codepoint, of which "offset" is part of.
 */
Utf8View::iterator Utf8View::GetIterAtByte(size_t offset) const
{
	assert(offset <= this->src.size());
	if (offset >= this->src.size()) return this->end();

	/* Sanitize iterator to point to the start of a codepoint */
	auto it = iterator(this->src, offset + 1);
	--it;
	return it;
}
