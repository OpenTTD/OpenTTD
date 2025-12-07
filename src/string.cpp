/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file string.cpp Handling of strings (std::string/std::string_view). */

#include "stdafx.h"
#include "debug.h"
#include "error_func.h"
#include "string_func.h"
#include "string_base.h"
#include "core/utf8.hpp"
#include "core/string_inplace.hpp"

#include "table/control_codes.h"

#ifdef _WIN32
#	include "os/windows/win32.h"
#endif

#ifdef WITH_UNISCRIBE
#	include "os/windows/string_uniscribe.h"
#endif

#ifdef WITH_ICU_I18N
/* Required by StrNaturalCompare. */
#	include <unicode/brkiter.h>
#	include <unicode/stsearch.h>
#	include <unicode/ustring.h>
#	include <unicode/utext.h>
#	include "language.h"
#	include "gfx_func.h"
#endif /* WITH_ICU_I18N */

#if defined(WITH_COCOA)
#	include "os/macosx/string_osx.h"
#endif

#include "safeguards.h"


/**
 * Copies characters from one buffer to another.
 *
 * Copies the source string to the destination buffer with respect of the
 * terminating null-character and the size of the destination buffer.
 *
 * @note usage: strecpy(dst, src);
 *
 * @param dst The destination buffer
 * @param src The buffer containing the string to copy
 */
void strecpy(std::span<char> dst, std::string_view src)
{
	/* Ensure source string fits with NUL terminator; dst must be at least 1 character longer than src. */
	if (std::empty(dst) || std::size(src) >= std::size(dst) - 1U) {
#if defined(STRGEN) || defined(SETTINGSGEN)
		FatalError("String too long for destination buffer");
#else /* STRGEN || SETTINGSGEN */
		Debug(misc, 0, "String too long for destination buffer");
		src = src.substr(0, std::size(dst) - 1U);
#endif /* STRGEN || SETTINGSGEN */
	}

	auto it = std::copy(std::begin(src), std::end(src), std::begin(dst));
	*it = '\0';
}

/**
 * Format a byte array into a continuous hex string.
 * @param data Array to format
 * @return Converted string.
 */
std::string FormatArrayAsHex(std::span<const uint8_t> data)
{
	std::string str;
	str.reserve(data.size() * 2 + 1);

	for (auto b : data) {
		format_append(str, "{:02X}", b);
	}

	return str;
}

/**
 * Test if a character is (only) part of an encoded string.
 * @param c Character to test.
 * @returns True iff the character is an encoded string control code.
 */
static bool IsSccEncodedCode(char32_t c)
{
	switch (c) {
		case SCC_RECORD_SEPARATOR:
		case SCC_ENCODED:
		case SCC_ENCODED_INTERNAL:
		case SCC_ENCODED_NUMERIC:
		case SCC_ENCODED_STRING:
			return true;

		default:
			return false;
	}
}

/**
 * Copies the valid (UTF-8) characters from \c consumer to the \c builder.
 * Depending on the \c settings invalid characters can be replaced with a
 * question mark, as well as determining what characters are deemed invalid.
 *
 * @param builder The destination to write to.
 * @param consumer The string to validate.
 * @param settings The settings for the string validation.
 */
template <class Builder>
static void StrMakeValid(Builder &builder, StringConsumer &consumer, StringValidationSettings settings)
{
	/* Assume the ABSOLUTE WORST to be in str as it comes from the outside. */
	while (consumer.AnyBytesLeft()) {
		auto c = consumer.TryReadUtf8();
		if (!c.has_value()) {
			/* Maybe the next byte is still a valid character? */
			consumer.Skip(1);
			continue;
		}
		if (*c == 0) break;

		if ((IsPrintable(*c) && (*c < SCC_SPRITE_START || *c > SCC_SPRITE_END)) ||
				(settings.Test(StringValidationSetting::AllowControlCode) && IsSccEncodedCode(*c)) ||
				(settings.Test(StringValidationSetting::AllowNewline) && *c == '\n')) {
			builder.PutUtf8(*c);
		} else if (settings.Test(StringValidationSetting::AllowNewline) && *c == '\r' && consumer.PeekCharIf('\n')) {
			/* Skip \r, if followed by \n */
			/* continue */
		} else if (settings.Test(StringValidationSetting::ReplaceTabCrNlWithSpace) && (*c == '\r' || *c == '\n' || *c == '\t')) {
			/* Replace the tab, carriage return or newline with a space. */
			builder.PutChar(' ');
		} else if (settings.Test(StringValidationSetting::ReplaceWithQuestionMark)) {
			/* Replace the undesirable character with a question mark */
			builder.PutChar('?');
		}
	}

	/* String termination, if needed, is left to the caller of this function. */
}

/**
 * Scans the string for invalid characters and replaces them with a
 * question mark '?' (if not ignored).
 * @param str The string to validate.
 * @param settings The settings for the string validation.
 * @note The string must be properly NUL terminated.
 */
void StrMakeValidInPlace(char *str, StringValidationSettings settings)
{
	InPlaceReplacement inplace(std::span(str, strlen(str)));
	StrMakeValid(inplace.builder, inplace.consumer, settings);
	/* Add NUL terminator, if we ended up with less bytes than before */
	if (inplace.builder.AnyBytesUnused()) inplace.builder.PutChar('\0');
}

/**
 * Scans the string for invalid characters and replaces them with a
 * question mark '?' (if not ignored).
 * @param str The string to validate.
 * @param settings The settings for the string validation.
 * @note The string must be properly NUL terminated.
 */
void StrMakeValidInPlace(std::string &str, StringValidationSettings settings)
{
	if (str.empty()) return;

	InPlaceReplacement inplace(std::span(str.data(), str.size()));
	StrMakeValid(inplace.builder, inplace.consumer, settings);
	str.erase(inplace.builder.GetBytesWritten(), std::string::npos);
}

/**
 * Copies the valid (UTF-8) characters from \c str to the returned string.
 * Depending on the \c settings invalid characters can be replaced with a
 * question mark, as well as determining what characters are deemed invalid.
 * @param str The string to validate.
 * @param settings The settings for the string validation.
 */
std::string StrMakeValid(std::string_view str, StringValidationSettings settings)
{
	std::string result;
	StringBuilder builder(result);
	StringConsumer consumer(str);
	StrMakeValid(builder, consumer, settings);
	return result;
}

/**
 * Checks whether the given string is valid, i.e. contains only
 * valid (printable) characters and is properly terminated.
 * @note std::span is used instead of std::string_view as we are validating fixed-length string buffers, and
 * std::string_view's constructor will assume a C-string that ends with a NUL terminator, which is one of the things
 * we are checking.
 * @param str Span of chars to validate.
 */
bool StrValid(std::span<const char> str)
{
	/* Assume the ABSOLUTE WORST to be in str as it comes from the outside. */
	StringConsumer consumer(str);
	while (consumer.AnyBytesLeft()) {
		auto c = consumer.TryReadUtf8();
		if (!c.has_value()) return false; // invalid codepoint
		if (*c == 0) return true; // NUL termination
		if (!IsPrintable(*c) || (*c >= SCC_SPRITE_START && *c <= SCC_SPRITE_END)) {
			return false;
		}
	}

	return false; // missing NUL termination
}

/**
 * Trim the spaces from given string in place, i.e. the string buffer that
 * is passed will be modified whenever spaces exist in the given string.
 * When there are spaces at the begin, the whole string is moved forward
 * and when there are spaces at the back the '\0' termination is moved.
 * @param str The string to perform the in place trimming on.
 */
void StrTrimInPlace(std::string &str)
{
	size_t first_pos = str.find_first_not_of(StringConsumer::WHITESPACE_NO_NEWLINE);
	if (first_pos == std::string::npos) {
		str.clear();
		return;
	}
	str.erase(0, first_pos);

	size_t last_pos = str.find_last_not_of(StringConsumer::WHITESPACE_NO_NEWLINE);
	str.erase(last_pos + 1);
}

std::string_view StrTrimView(std::string_view str, std::string_view characters_to_trim)
{
	size_t first_pos = str.find_first_not_of(characters_to_trim);
	if (first_pos == std::string::npos) {
		return std::string_view{};
	}
	size_t last_pos = str.find_last_not_of(characters_to_trim);
	return str.substr(first_pos, last_pos - first_pos + 1);
}

/**
 * Check whether the given string starts with the given prefix, ignoring case.
 * @param str    The string to look at.
 * @param prefix The prefix to look for.
 * @return True iff the begin of the string is the same as the prefix, ignoring case.
 */
bool StrStartsWithIgnoreCase(std::string_view str, std::string_view prefix)
{
	if (str.size() < prefix.size()) return false;
	return StrEqualsIgnoreCase(str.substr(0, prefix.size()), prefix);
}

/** Case insensitive implementation of the standard character type traits. */
struct CaseInsensitiveCharTraits : public std::char_traits<char> {
	static bool eq(char c1, char c2) { return toupper(c1) == toupper(c2); }
	static bool ne(char c1, char c2) { return toupper(c1) != toupper(c2); }
	static bool lt(char c1, char c2) { return toupper(c1) <  toupper(c2); }

	static int compare(const char *s1, const char *s2, size_t n)
	{
		while (n-- != 0) {
			if (toupper(*s1) < toupper(*s2)) return -1;
			if (toupper(*s1) > toupper(*s2)) return 1;
			++s1; ++s2;
		}
		return 0;
	}

	static const char *find(const char *s, size_t n, char a)
	{
		for (; n > 0; --n, ++s) {
			if (toupper(*s) == toupper(a)) return s;
		}
		return nullptr;
	}
};

/** Case insensitive string view. */
typedef std::basic_string_view<char, CaseInsensitiveCharTraits> CaseInsensitiveStringView;

/**
 * Check whether the given string ends with the given suffix, ignoring case.
 * @param str    The string to look at.
 * @param suffix The suffix to look for.
 * @return True iff the end of the string is the same as the suffix, ignoring case.
 */
bool StrEndsWithIgnoreCase(std::string_view str, std::string_view suffix)
{
	if (str.size() < suffix.size()) return false;
	return StrEqualsIgnoreCase(str.substr(str.size() - suffix.size()), suffix);
}

/**
 * Compares two string( view)s, while ignoring the case of the characters.
 * @param str1 The first string.
 * @param str2 The second string.
 * @return Less than zero if str1 < str2, zero if str1 == str2, greater than
 *         zero if str1 > str2. All ignoring the case of the characters.
 */
int StrCompareIgnoreCase(std::string_view str1, std::string_view str2)
{
	CaseInsensitiveStringView ci_str1{ str1.data(), str1.size() };
	CaseInsensitiveStringView ci_str2{ str2.data(), str2.size() };
	return ci_str1.compare(ci_str2);
}

/**
 * Compares two string( view)s for equality, while ignoring the case of the characters.
 * @param str1 The first string.
 * @param str2 The second string.
 * @return True iff both strings are equal, barring the case of the characters.
 */
bool StrEqualsIgnoreCase(std::string_view str1, std::string_view str2)
{
	if (str1.size() != str2.size()) return false;
	return StrCompareIgnoreCase(str1, str2) == 0;
}

/**
 * Checks if a string is contained in another string, while ignoring the case of the characters.
 *
 * @param str The string to search in.
 * @param value The string to search for.
 * @return True if a match was found.
 */
bool StrContainsIgnoreCase(std::string_view str, std::string_view value)
{
	CaseInsensitiveStringView ci_str{ str.data(), str.size() };
	CaseInsensitiveStringView ci_value{ value.data(), value.size() };
	return ci_str.find(ci_value) != ci_str.npos;
}

/**
 * Get the length of an UTF-8 encoded string in number of characters
 * and thus not the number of bytes that the encoded string contains.
 * @param s The string to get the length for.
 * @return The length of the string in characters.
 */
size_t Utf8StringLength(std::string_view str)
{
	Utf8View view(str);
	return std::distance(view.begin(), view.end());
}

bool strtolower(std::string &str, std::string::size_type offs)
{
	bool changed = false;
	for (auto ch = str.begin() + offs; ch != str.end(); ++ch) {
		auto new_ch = static_cast<char>(tolower(static_cast<unsigned char>(*ch)));
		changed |= new_ch != *ch;
		*ch = new_ch;
	}
	return changed;
}

/**
 * Only allow certain keys. You can define the filter to be used. This makes
 *  sure no invalid keys can get into an editbox, like BELL.
 * @param key character to be checked
 * @param afilter the filter to use
 * @return true or false depending if the character is printable/valid or not
 */
bool IsValidChar(char32_t key, CharSetFilter afilter)
{
	switch (afilter) {
		case CS_ALPHANUMERAL:   return IsPrintable(key);
		case CS_NUMERAL:        return (key >= '0' && key <= '9');
		case CS_NUMERAL_SPACE:  return (key >= '0' && key <= '9') || key == ' ';
		case CS_NUMERAL_SIGNED: return (key >= '0' && key <= '9') || key == '-';
		case CS_ALPHA:          return IsPrintable(key) && !(key >= '0' && key <= '9');
		case CS_HEXADECIMAL:    return (key >= '0' && key <= '9') || (key >= 'a' && key <= 'f') || (key >= 'A' && key <= 'F');
		default: NOT_REACHED();
	}
}

/**
 * Test if a unicode character is considered garbage to be skipped.
 * @param c Character to test.
 * @returns true iff the character should be skipped.
 */
static bool IsGarbageCharacter(char32_t c)
{
	if (c >= '0' && c <= '9') return false;
	if (c >= 'A' && c <= 'Z') return false;
	if (c >= 'a' && c <= 'z') return false;
	if (c >= SCC_CONTROL_START && c <= SCC_CONTROL_END) return true;
	if (c >= 0xC0 && c <= 0x10FFFF) return false;

	return true;
}

/**
 * Skip some of the 'garbage' in the string that we don't want to use
 * to sort on. This way the alphabetical sorting will work better as
 * we would be actually using those characters instead of some other
 * characters such as spaces and tildes at the begin of the name.
 * @param str The string to skip the initial garbage of.
 * @return The string with the garbage skipped.
 */
static std::string_view SkipGarbage(std::string_view str)
{
	Utf8View view(str);
	auto it = view.begin();
	const auto end = view.end();
	while (it != end && IsGarbageCharacter(*it)) ++it;
	return str.substr(it.GetByteOffset());
}

/**
 * Compares two strings using case insensitive natural sort.
 *
 * @param s1 First string to compare.
 * @param s2 Second string to compare.
 * @param ignore_garbage_at_front Skip punctuation characters in the front
 * @return Less than zero if s1 < s2, zero if s1 == s2, greater than zero if s1 > s2.
 */
int StrNaturalCompare(std::string_view s1, std::string_view s2, bool ignore_garbage_at_front)
{
	if (ignore_garbage_at_front) {
		s1 = SkipGarbage(s1);
		s2 = SkipGarbage(s2);
	}

#ifdef WITH_ICU_I18N
	if (_current_collator) {
		UErrorCode status = U_ZERO_ERROR;
		int result = _current_collator->compareUTF8(icu::StringPiece(s1.data(), s1.size()), icu::StringPiece(s2.data(), s2.size()), status);
		if (U_SUCCESS(status)) return result;
	}
#endif /* WITH_ICU_I18N */

#if defined(_WIN32) && !defined(STRGEN) && !defined(SETTINGSGEN)
	int res = OTTDStringCompare(s1, s2);
	if (res != 0) return res - 2; // Convert to normal C return values.
#endif

#if defined(WITH_COCOA) && !defined(STRGEN) && !defined(SETTINGSGEN)
	int res = MacOSStringCompare(s1, s2);
	if (res != 0) return res - 2; // Convert to normal C return values.
#endif

	/* Do a normal comparison if ICU is missing or if we cannot create a collator. */
	return StrCompareIgnoreCase(s1, s2);
}

#ifdef WITH_ICU_I18N

/**
 * Search if a string is contained in another string using the current locale.
 *
 * @param str String to search in.
 * @param value String to search for.
 * @param case_insensitive Search case-insensitive.
 * @return 1 if value was found, 0 if it was not found, or -1 if not supported by the OS.
 */
static int ICUStringContains(std::string_view str, std::string_view value, bool case_insensitive)
{
	if (_current_collator) {
		std::unique_ptr<icu::RuleBasedCollator> coll(dynamic_cast<icu::RuleBasedCollator *>(_current_collator->clone()));
		if (coll) {
			UErrorCode status = U_ZERO_ERROR;
			coll->setStrength(case_insensitive ? icu::Collator::SECONDARY : icu::Collator::TERTIARY);
			coll->setAttribute(UCOL_NUMERIC_COLLATION, UCOL_OFF, status);

			auto u_str = icu::UnicodeString::fromUTF8(icu::StringPiece(str.data(), str.size()));
			auto u_value = icu::UnicodeString::fromUTF8(icu::StringPiece(value.data(), value.size()));
			icu::StringSearch u_searcher(u_value, u_str, coll.get(), nullptr, status);
			if (U_SUCCESS(status)) {
				auto pos = u_searcher.first(status);
				if (U_SUCCESS(status)) return pos != USEARCH_DONE ? 1 : 0;
			}
		}
	}

	return -1;
}
#endif /* WITH_ICU_I18N */

/**
 * Checks if a string is contained in another string with a locale-aware comparison that is case sensitive.
 *
 * @param str The string to search in.
 * @param value The string to search for.
 * @return True if a match was found.
 */
[[nodiscard]] bool StrNaturalContains(std::string_view str, std::string_view value)
{
#ifdef WITH_ICU_I18N
	int res_u = ICUStringContains(str, value, false);
	if (res_u >= 0) return res_u > 0;
#endif /* WITH_ICU_I18N */

#if defined(_WIN32) && !defined(STRGEN) && !defined(SETTINGSGEN)
	int res = Win32StringContains(str, value, false);
	if (res >= 0) return res > 0;
#endif

#if defined(WITH_COCOA) && !defined(STRGEN) && !defined(SETTINGSGEN)
	int res = MacOSStringContains(str, value, false);
	if (res >= 0) return res > 0;
#endif

	return str.find(value) != std::string_view::npos;
}

/**
 * Checks if a string is contained in another string with a locale-aware comparison that is case insensitive.
 *
 * @param str The string to search in.
 * @param value The string to search for.
 * @return True if a match was found.
 */
[[nodiscard]] bool StrNaturalContainsIgnoreCase(std::string_view str, std::string_view value)
{
#ifdef WITH_ICU_I18N
	int res_u = ICUStringContains(str, value, true);
	if (res_u >= 0) return res_u > 0;
#endif /* WITH_ICU_I18N */

#if defined(_WIN32) && !defined(STRGEN) && !defined(SETTINGSGEN)
	int res = Win32StringContains(str, value, true);
	if (res >= 0) return res > 0;
#endif

#if defined(WITH_COCOA) && !defined(STRGEN) && !defined(SETTINGSGEN)
	int res = MacOSStringContains(str, value, true);
	if (res >= 0) return res > 0;
#endif

	CaseInsensitiveStringView ci_str{ str.data(), str.size() };
	CaseInsensitiveStringView ci_value{ value.data(), value.size() };
	return ci_str.find(ci_value) != CaseInsensitiveStringView::npos;
}

/**
 * Convert a single hex-nibble to a byte.
 *
 * @param c The hex-nibble to convert.
 * @return The byte the hex-nibble represents, or -1 if it is not a valid hex-nibble.
 */
static int ConvertHexNibbleToByte(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c + 10 - 'A';
	if (c >= 'a' && c <= 'f') return c + 10 - 'a';
	return -1;
}

/**
 * Convert a hex-string to a byte-array, while validating it was actually hex.
 *
 * @param hex The hex-string to convert.
 * @param bytes The byte-array to write the result to.
 *
 * @note The length of the hex-string has to be exactly twice that of the length
 * of the byte-array, otherwise conversion will fail.
 *
 * @return True iff the hex-string was valid and the conversion succeeded.
 */
bool ConvertHexToBytes(std::string_view hex, std::span<uint8_t> bytes)
{
	if (bytes.size() != hex.size() / 2) {
		return false;
	}

	/* Hex-string lengths are always divisible by 2. */
	if (hex.size() % 2 != 0) {
		return false;
	}

	for (size_t i = 0; i < hex.size() / 2; i++) {
		auto hi = ConvertHexNibbleToByte(hex[i * 2]);
		auto lo = ConvertHexNibbleToByte(hex[i * 2 + 1]);

		if (hi < 0 || lo < 0) {
			return false;
		}

		bytes[i] = (hi << 4) | lo;
	}

	return true;
}

#ifdef WITH_UNISCRIBE

/* static */ std::unique_ptr<StringIterator> StringIterator::Create()
{
	return std::make_unique<UniscribeStringIterator>();
}

#elif defined(WITH_ICU_I18N)

/** String iterator using ICU as a backend. */
class IcuStringIterator : public StringIterator
{
	std::unique_ptr<icu::BreakIterator> char_itr; ///< ICU iterator for characters.
	std::unique_ptr<icu::BreakIterator> word_itr; ///< ICU iterator for words.

	std::vector<UChar> utf16_str;      ///< UTF-16 copy of the string.
	std::vector<size_t> utf16_to_utf8; ///< Mapping from UTF-16 code point position to index in the UTF-8 source string.

public:
	IcuStringIterator()
	{
		UErrorCode status = U_ZERO_ERROR;
		auto locale = icu::Locale(_current_language != nullptr ? _current_language->isocode : "en");
		this->char_itr.reset(icu::BreakIterator::createCharacterInstance(locale, status));
		this->word_itr.reset(icu::BreakIterator::createWordInstance(locale, status));

		this->utf16_str.push_back('\0');
		this->utf16_to_utf8.push_back(0);
	}

	~IcuStringIterator() override = default;

	void SetString(std::string_view s) override
	{
		/* Unfortunately current ICU versions only provide rudimentary support
		 * for word break iterators (especially for CJK languages) in combination
		 * with UTF-8 input. As a work around we have to convert the input to
		 * UTF-16 and create a mapping back to UTF-8 character indices. */
		this->utf16_str.clear();
		this->utf16_to_utf8.clear();

		Utf8View view(s);
		for (auto it = view.begin(), end = view.end(); it != end; ++it) {
			size_t idx = it.GetByteOffset();
			char32_t c = *it;
			if (c < 0x10000) {
				this->utf16_str.push_back((UChar)c);
			} else {
				/* Make a surrogate pair. */
				this->utf16_str.push_back((UChar)(0xD800 + ((c - 0x10000) >> 10)));
				this->utf16_str.push_back((UChar)(0xDC00 + ((c - 0x10000) & 0x3FF)));
				this->utf16_to_utf8.push_back(idx);
			}
			this->utf16_to_utf8.push_back(idx);
		}
		this->utf16_str.push_back('\0');
		this->utf16_to_utf8.push_back(s.size());

		UText text = UTEXT_INITIALIZER;
		UErrorCode status = U_ZERO_ERROR;
		utext_openUChars(&text, this->utf16_str.data(), this->utf16_str.size() - 1, &status);
		this->char_itr->setText(&text, status);
		this->word_itr->setText(&text, status);
		this->char_itr->first();
		this->word_itr->first();
	}

	size_t SetCurPosition(size_t pos) override
	{
		/* Convert incoming position to an UTF-16 string index. */
		uint utf16_pos = 0;
		for (uint i = 0; i < this->utf16_to_utf8.size(); i++) {
			if (this->utf16_to_utf8[i] == pos) {
				utf16_pos = i;
				break;
			}
		}

		/* isBoundary has the documented side-effect of setting the current
		 * position to the first valid boundary equal to or greater than
		 * the passed value. */
		this->char_itr->isBoundary(utf16_pos);
		return this->utf16_to_utf8[this->char_itr->current()];
	}

	size_t Next(IterType what) override
	{
		int32_t pos;
		switch (what) {
			case ITER_CHARACTER:
				pos = this->char_itr->next();
				break;

			case ITER_WORD:
				pos = this->word_itr->following(this->char_itr->current());
				/* The ICU word iterator considers both the start and the end of a word a valid
				 * break point, but we only want word starts. Move to the next location in
				 * case the new position points to whitespace. */
				while (pos != icu::BreakIterator::DONE &&
						IsWhitespace(Utf16DecodeChar((const uint16_t *)&this->utf16_str[pos]))) {
					int32_t new_pos = this->word_itr->next();
					/* Don't set it to DONE if it was valid before. Otherwise we'll return END
					 * even though the iterator wasn't at the end of the string before. */
					if (new_pos == icu::BreakIterator::DONE) break;
					pos = new_pos;
				}

				this->char_itr->isBoundary(pos);
				break;

			default:
				NOT_REACHED();
		}

		return pos == icu::BreakIterator::DONE ? END : this->utf16_to_utf8[pos];
	}

	size_t Prev(IterType what) override
	{
		int32_t pos;
		switch (what) {
			case ITER_CHARACTER:
				pos = this->char_itr->previous();
				break;

			case ITER_WORD:
				pos = this->word_itr->preceding(this->char_itr->current());
				/* The ICU word iterator considers both the start and the end of a word a valid
				 * break point, but we only want word starts. Move to the previous location in
				 * case the new position points to whitespace. */
				while (pos != icu::BreakIterator::DONE &&
						IsWhitespace(Utf16DecodeChar((const uint16_t *)&this->utf16_str[pos]))) {
					int32_t new_pos = this->word_itr->previous();
					/* Don't set it to DONE if it was valid before. Otherwise we'll return END
					 * even though the iterator wasn't at the start of the string before. */
					if (new_pos == icu::BreakIterator::DONE) break;
					pos = new_pos;
				}

				this->char_itr->isBoundary(pos);
				break;

			default:
				NOT_REACHED();
		}

		return pos == icu::BreakIterator::DONE ? END : this->utf16_to_utf8[pos];
	}
};

/* static */ std::unique_ptr<StringIterator> StringIterator::Create()
{
	return std::make_unique<IcuStringIterator>();
}

#else

/** Fallback simple string iterator. */
class DefaultStringIterator : public StringIterator
{
	Utf8View string; ///< Current string.
	Utf8View::iterator cur_pos; //< Current iteration position.

public:
	void SetString(std::string_view s) override
	{
		this->string = s;
		this->cur_pos = this->string.begin();
	}

	size_t SetCurPosition(size_t pos) override
	{
		this->cur_pos = this->string.GetIterAtByte(pos);
		return this->cur_pos.GetByteOffset();
	}

	size_t Next(IterType what) override
	{
		const auto end = this->string.end();
		/* Already at the end? */
		if (this->cur_pos >= end) return END;

		switch (what) {
			case ITER_CHARACTER:
				++this->cur_pos;
				return this->cur_pos.GetByteOffset();

			case ITER_WORD:
				/* Consume current word. */
				while (this->cur_pos != end && !IsWhitespace(*this->cur_pos)) {
					++this->cur_pos;
				}
				/* Consume whitespace to the next word. */
				while (this->cur_pos != end && IsWhitespace(*this->cur_pos)) {
					++this->cur_pos;
				}
				return this->cur_pos.GetByteOffset();

			default:
				NOT_REACHED();
		}

		return END;
	}

	size_t Prev(IterType what) override
	{
		const auto begin = this->string.begin();
		/* Already at the beginning? */
		if (this->cur_pos == begin) return END;

		switch (what) {
			case ITER_CHARACTER:
				--this->cur_pos;
				return this->cur_pos.GetByteOffset();

			case ITER_WORD:
				/* Consume preceding whitespace. */
				do {
					--this->cur_pos;
				} while (this->cur_pos != begin && IsWhitespace(*this->cur_pos));
				/* Consume preceding word. */
				while (this->cur_pos != begin && !IsWhitespace(*this->cur_pos)) {
					--this->cur_pos;
				}
				/* Move caret back to the beginning of the word. */
				if (IsWhitespace(*this->cur_pos)) ++this->cur_pos;
				return this->cur_pos.GetByteOffset();

			default:
				NOT_REACHED();
		}

		return END;
	}
};

#if defined(WITH_COCOA) && !defined(STRGEN) && !defined(SETTINGSGEN)
/* static */ std::unique_ptr<StringIterator> StringIterator::Create()
{
	std::unique_ptr<StringIterator> i = OSXStringIterator::Create();
	if (i != nullptr) return i;

	return std::make_unique<DefaultStringIterator>();
}
#else
/* static */ std::unique_ptr<StringIterator> StringIterator::Create()
{
	return std::make_unique<DefaultStringIterator>();
}
#endif /* defined(WITH_COCOA) && !defined(STRGEN) && !defined(SETTINGSGEN) */

#endif

/**
 * Get the environment variable using std::getenv and when it is an empty string (or nullptr), return \c std::nullopt instead.
 * @param variable The environment variable to read from.
 * @return The environment value, or \c std::nullopt.
 */
std::optional<std::string_view> GetEnv(const char *variable)
{
	auto val = std::getenv(variable);
	if (val == nullptr || *val == '\0') return std::nullopt;
	return val;
}
