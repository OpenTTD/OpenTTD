/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file string_func.h Functions related to low-level strings.
 */

#ifndef STRING_FUNC_H
#define STRING_FUNC_H

#include <iosfwd>

#include "string_type.h"

void strecpy(std::span<char> dst, std::string_view src);

std::string FormatArrayAsHex(std::span<const uint8_t> data);

void StrMakeValidInPlace(char *str, StringValidationSettings settings = StringValidationSetting::ReplaceWithQuestionMark);
void StrMakeValidInPlace(std::string &str, StringValidationSettings settings = StringValidationSetting::ReplaceWithQuestionMark);

[[nodiscard]] std::string StrMakeValid(std::string_view str, StringValidationSettings settings = StringValidationSetting::ReplaceWithQuestionMark);
[[nodiscard]] inline std::string StrMakeValid(std::string &&str, StringValidationSettings settings = StringValidationSetting::ReplaceWithQuestionMark)
{
	StrMakeValidInPlace(str, settings);
	return std::move(str);
}

bool strtolower(std::string &str, std::string::size_type offs = 0);

[[nodiscard]] bool StrValid(std::span<const char> str);
void StrTrimInPlace(std::string &str);
[[nodiscard]] std::string_view StrTrimView(std::string_view str, std::string_view characters_to_trim);

[[nodiscard]] bool StrStartsWithIgnoreCase(std::string_view str, std::string_view prefix);
[[nodiscard]] bool StrEndsWithIgnoreCase(std::string_view str, std::string_view suffix);

[[nodiscard]] int StrCompareIgnoreCase(std::string_view str1, std::string_view str2);
[[nodiscard]] bool StrEqualsIgnoreCase(std::string_view str1, std::string_view str2);
[[nodiscard]] bool StrContainsIgnoreCase(std::string_view str, std::string_view value);
[[nodiscard]] int StrNaturalCompare(std::string_view s1, std::string_view s2, bool ignore_garbage_at_front = false);
[[nodiscard]] bool StrNaturalContains(std::string_view str, std::string_view value);
[[nodiscard]] bool StrNaturalContainsIgnoreCase(std::string_view str, std::string_view value);

bool ConvertHexToBytes(std::string_view hex, std::span<uint8_t> bytes);

/** Case insensitive comparator for strings, for example for use in std::map. */
struct CaseInsensitiveComparator {
	bool operator()(std::string_view s1, std::string_view s2) const { return StrCompareIgnoreCase(s1, s2) < 0; }
};

bool IsValidChar(char32_t key, CharSetFilter afilter);

size_t Utf8StringLength(std::string_view str);

/**
 * Is the given character a lead surrogate code point?
 * @param c The character to test.
 * @return True if the character is a lead surrogate code point.
 */
inline bool Utf16IsLeadSurrogate(uint c)
{
	return c >= 0xD800 && c <= 0xDBFF;
}

/**
 * Is the given character a lead surrogate code point?
 * @param c The character to test.
 * @return True if the character is a lead surrogate code point.
 */
inline bool Utf16IsTrailSurrogate(uint c)
{
	return c >= 0xDC00 && c <= 0xDFFF;
}

/**
 * Convert an UTF-16 surrogate pair to the corresponding Unicode character.
 * @param lead Lead surrogate code point.
 * @param trail Trail surrogate code point.
 * @return Decoded Unicode character.
 */
inline char32_t Utf16DecodeSurrogate(uint lead, uint trail)
{
	return 0x10000 + (((lead - 0xD800) << 10) | (trail - 0xDC00));
}

/**
 * Decode an UTF-16 character.
 * @param c Pointer to one or two UTF-16 code points.
 * @return Decoded Unicode character.
 */
inline char32_t Utf16DecodeChar(const uint16_t *c)
{
	if (Utf16IsLeadSurrogate(c[0])) {
		return Utf16DecodeSurrogate(c[0], c[1]);
	} else {
		return *c;
	}
}

/**
 * Is the given character a text direction character.
 * @param c The character to test.
 * @return true iff the character is used to influence
 *         the text direction.
 */
inline bool IsTextDirectionChar(char32_t c)
{
	switch (c) {
		case CHAR_TD_LRM:
		case CHAR_TD_RLM:
		case CHAR_TD_LRE:
		case CHAR_TD_RLE:
		case CHAR_TD_LRO:
		case CHAR_TD_RLO:
		case CHAR_TD_PDF:
			return true;

		default:
			return false;
	}
}

inline bool IsPrintable(char32_t c)
{
	if (c < 0x20)   return false;
	if (c < 0xE000) return true;
	if (c < 0xE200) return false;
	return true;
}

/**
 * Check whether UNICODE character is whitespace or not, i.e. whether
 * this is a potential line-break character.
 * @param c UNICODE character to check
 * @return a boolean value whether 'c' is a whitespace character or not
 * @see http://www.fileformat.info/info/unicode/category/Zs/list.htm
 */
inline bool IsWhitespace(char32_t c)
{
	return c == 0x0020 /* SPACE */ || c == 0x3000; /* IDEOGRAPHIC SPACE */
}

/* Needed for NetBSD version (so feature) testing */
#if defined(__NetBSD__) || defined(__FreeBSD__)
#include <sys/param.h>
#endif

std::optional<std::string_view> GetEnv(const char *variable);

#endif /* STRING_FUNC_H */
