/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string.cpp Handling of C-type strings (char*). */

#include "stdafx.h"
#include "debug.h"
#include "core/alloc_func.hpp"
#include "core/math_func.hpp"
#include "error_func.h"
#include "string_func.h"
#include "string_base.h"

#include "table/control_codes.h"

#include <sstream>
#include <iomanip>

#ifdef _MSC_VER
#	define strncasecmp strnicmp
#endif

#ifdef _WIN32
#	include "os/windows/win32.h"
#endif

#ifdef WITH_UNISCRIBE
#	include "os/windows/string_uniscribe.h"
#endif

#ifdef WITH_ICU_I18N
/* Required by StrNaturalCompare. */
#	include <unicode/ustring.h>
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
 * terminating null-character and the last pointer to the last element in
 * the destination buffer. If the last pointer is set to nullptr no boundary
 * check is performed.
 *
 * @note usage: strecpy(dst, src, lastof(dst));
 * @note lastof() applies only to fixed size arrays
 *
 * @param dst The destination buffer
 * @param src The buffer containing the string to copy
 * @param last The pointer to the last element of the destination buffer
 * @return The pointer to the terminating null-character in the destination buffer
 */
char *strecpy(char *dst, const char *src, const char *last)
{
	assert(dst <= last);
	while (dst != last && *src != '\0') {
		*dst++ = *src++;
	}
	*dst = '\0';

	if (dst == last && *src != '\0') {
#if defined(STRGEN) || defined(SETTINGSGEN)
		FatalError("String too long for destination buffer");
#else /* STRGEN || SETTINGSGEN */
		Debug(misc, 0, "String too long for destination buffer");
#endif /* STRGEN || SETTINGSGEN */
	}
	return dst;
}

/**
 * Format a byte array into a continuous hex string.
 * @param data Array to format
 * @return Converted string.
 */
std::string FormatArrayAsHex(span<const byte> data)
{
	std::string str;
	str.reserve(data.size() * 2 + 1);

	for (auto b : data) {
		fmt::format_to(std::back_inserter(str), "{:02X}", b);
	}

	return str;
}


/**
 * Copies the valid (UTF-8) characters from \c str up to \c last to the \c dst.
 * Depending on the \c settings invalid characters can be replaced with a
 * question mark, as well as determining what characters are deemed invalid.
 *
 * It is allowed for \c dst to be the same as \c src, in which case the string
 * is make valid in place.
 * @param dst The destination to write to.
 * @param str The string to validate.
 * @param last The last valid character of str.
 * @param settings The settings for the string validation.
 */
template <class T>
static void StrMakeValid(T &dst, const char *str, const char *last, StringValidationSettings settings)
{
	/* Assume the ABSOLUTE WORST to be in str as it comes from the outside. */

	while (str <= last && *str != '\0') {
		size_t len = Utf8EncodedCharLen(*str);
		char32_t c;
		/* If the first byte does not look like the first byte of an encoded
		 * character, i.e. encoded length is 0, then this byte is definitely bad
		 * and it should be skipped.
		 * When the first byte looks like the first byte of an encoded character,
		 * then the remaining bytes in the string are checked whether the whole
		 * encoded character can be there. If that is not the case, this byte is
		 * skipped.
		 * Finally we attempt to decode the encoded character, which does certain
		 * extra validations to see whether the correct number of bytes were used
		 * to encode the character. If that is not the case, the byte is probably
		 * invalid and it is skipped. We could emit a question mark, but then the
		 * logic below cannot just copy bytes, it would need to re-encode the
		 * decoded characters as the length in bytes may have changed.
		 *
		 * The goals here is to get as much valid Utf8 encoded characters from the
		 * source string to the destination string.
		 *
		 * Note: a multi-byte encoded termination ('\0') will trigger the encoded
		 * char length and the decoded length to differ, so it will be ignored as
		 * invalid character data. If it were to reach the termination, then we
		 * would also reach the "last" byte of the string and a normal '\0'
		 * termination will be placed after it.
		 */
		if (len == 0 || str + len > last + 1 || len != Utf8Decode(&c, str)) {
			/* Maybe the next byte is still a valid character? */
			str++;
			continue;
		}

		if ((IsPrintable(c) && (c < SCC_SPRITE_START || c > SCC_SPRITE_END)) || ((settings & SVS_ALLOW_CONTROL_CODE) != 0 && c == SCC_ENCODED)) {
			/* Copy the character back. Even if dst is current the same as str
			 * (i.e. no characters have been changed) this is quicker than
			 * moving the pointers ahead by len */
			do {
				*dst++ = *str++;
			} while (--len != 0);
		} else if ((settings & SVS_ALLOW_NEWLINE) != 0 && c == '\n') {
			*dst++ = *str++;
		} else {
			if ((settings & SVS_ALLOW_NEWLINE) != 0 && c == '\r' && str[1] == '\n') {
				str += len;
				continue;
			}
			str += len;
			if ((settings & SVS_REPLACE_TAB_CR_NL_WITH_SPACE) != 0 && (c == '\r' || c == '\n' || c == '\t')) {
				/* Replace the tab, carriage return or newline with a space. */
				*dst++ = ' ';
			} else if ((settings & SVS_REPLACE_WITH_QUESTION_MARK) != 0) {
				/* Replace the undesirable character with a question mark */
				*dst++ = '?';
			}
		}
	}

	/* String termination, if needed, is left to the caller of this function. */
}

/**
 * Scans the string for invalid characters and replaces then with a
 * question mark '?' (if not ignored).
 * @param str The string to validate.
 * @param last The last valid character of str.
 * @param settings The settings for the string validation.
 */
void StrMakeValidInPlace(char *str, const char *last, StringValidationSettings settings)
{
	char *dst = str;
	StrMakeValid(dst, str, last, settings);
	*dst = '\0';
}

/**
 * Scans the string for invalid characters and replaces then with a
 * question mark '?' (if not ignored).
 * Only use this function when you are sure the string ends with a '\0';
 * otherwise use StrMakeValidInPlace(str, last, settings) variant.
 * @param str The string (of which you are sure ends with '\0') to validate.
 */
void StrMakeValidInPlace(char *str, StringValidationSettings settings)
{
	/* We know it is '\0' terminated. */
	StrMakeValidInPlace(str, str + strlen(str), settings);
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
	if (str.empty()) return {};

	auto buf = str.data();
	auto last = buf + str.size() - 1;

	std::ostringstream dst;
	std::ostreambuf_iterator<char> dst_iter(dst);
	StrMakeValid(dst_iter, buf, last, settings);

	return dst.str();
}

/**
 * Checks whether the given string is valid, i.e. contains only
 * valid (printable) characters and is properly terminated.
 * @param str  The string to validate.
 * @param last The last character of the string, i.e. the string
 *             must be terminated here or earlier.
 */
bool StrValid(const char *str, const char *last)
{
	/* Assume the ABSOLUTE WORST to be in str as it comes from the outside. */

	while (str <= last && *str != '\0') {
		size_t len = Utf8EncodedCharLen(*str);
		/* Encoded length is 0 if the character isn't known.
		 * The length check is needed to prevent Utf8Decode to read
		 * over the terminating '\0' if that happens to be placed
		 * within the encoding of an UTF8 character. */
		if (len == 0 || str + len > last) return false;

		char32_t c;
		len = Utf8Decode(&c, str);
		if (!IsPrintable(c) || (c >= SCC_SPRITE_START && c <= SCC_SPRITE_END)) {
			return false;
		}

		str += len;
	}

	return *str == '\0';
}

/**
 * Trim the spaces from the begin of given string in place, i.e. the string buffer
 * that is passed will be modified whenever spaces exist in the given string.
 * When there are spaces at the begin, the whole string is moved forward.
 * @param str The string to perform the in place left trimming on.
 */
static void StrLeftTrimInPlace(std::string &str)
{
	size_t pos = str.find_first_not_of(' ');
	str.erase(0, pos);
}

/**
 * Trim the spaces from the end of given string in place, i.e. the string buffer
 * that is passed will be modified whenever spaces exist in the given string.
 * When there are spaces at the end, the '\0' will be moved forward.
 * @param str The string to perform the in place left trimming on.
 */
static void StrRightTrimInPlace(std::string &str)
{
	size_t pos = str.find_last_not_of(' ');
	if (pos != std::string::npos) str.erase(pos + 1);
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
	StrLeftTrimInPlace(str);
	StrRightTrimInPlace(str);
}

/**
 * Check whether the given string starts with the given prefix.
 * @param str    The string to look at.
 * @param prefix The prefix to look for.
 * @return True iff the begin of the string is the same as the prefix.
 */
bool StrStartsWith(const std::string_view str, const std::string_view prefix)
{
	size_t prefix_len = prefix.size();
	if (str.size() < prefix_len) return false;
	return str.compare(0, prefix_len, prefix, 0, prefix_len) == 0;
}

/**
 * Check whether the given string starts with the given prefix, ignoring case.
 * @param str    The string to look at.
 * @param prefix The prefix to look for.
 * @return True iff the begin of the string is the same as the prefix, ignoring case.
 */
bool StrStartsWithIgnoreCase(std::string_view str, const std::string_view prefix)
{
	if (str.size() < prefix.size()) return false;
	return StrEqualsIgnoreCase(str.substr(0, prefix.size()), prefix);
}

/**
 * Check whether the given string ends with the given suffix.
 * @param str    The string to look at.
 * @param suffix The suffix to look for.
 * @return True iff the end of the string is the same as the suffix.
 */
bool StrEndsWith(const std::string_view str, const std::string_view suffix)
{
	size_t suffix_len = suffix.size();
	if (str.size() < suffix_len) return false;
	return str.compare(str.size() - suffix_len, suffix_len, suffix, 0, suffix_len) == 0;
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
bool StrEndsWithIgnoreCase(std::string_view str, const std::string_view suffix)
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
int StrCompareIgnoreCase(const std::string_view str1, const std::string_view str2)
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
bool StrEqualsIgnoreCase(const std::string_view str1, const std::string_view str2)
{
	if (str1.size() != str2.size()) return false;
	return StrCompareIgnoreCase(str1, str2) == 0;
}

/**
 * Get the length of an UTF-8 encoded string in number of characters
 * and thus not the number of bytes that the encoded string contains.
 * @param s The string to get the length for.
 * @return The length of the string in characters.
 */
size_t Utf8StringLength(const char *s)
{
	size_t len = 0;
	const char *t = s;
	while (Utf8Consume(&t) != 0) len++;
	return len;
}

/**
 * Get the length of an UTF-8 encoded string in number of characters
 * and thus not the number of bytes that the encoded string contains.
 * @param s The string to get the length for.
 * @return The length of the string in characters.
 */
size_t Utf8StringLength(const std::string &str)
{
	return Utf8StringLength(str.c_str());
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


/* UTF-8 handling routines */


/**
 * Decode and consume the next UTF-8 encoded character.
 * @param c Buffer to place decoded character.
 * @param s Character stream to retrieve character from.
 * @return Number of characters in the sequence.
 */
size_t Utf8Decode(char32_t *c, const char *s)
{
	assert(c != nullptr);

	if (!HasBit(s[0], 7)) {
		/* Single byte character: 0xxxxxxx */
		*c = s[0];
		return 1;
	} else if (GB(s[0], 5, 3) == 6) {
		if (IsUtf8Part(s[1])) {
			/* Double byte character: 110xxxxx 10xxxxxx */
			*c = GB(s[0], 0, 5) << 6 | GB(s[1], 0, 6);
			if (*c >= 0x80) return 2;
		}
	} else if (GB(s[0], 4, 4) == 14) {
		if (IsUtf8Part(s[1]) && IsUtf8Part(s[2])) {
			/* Triple byte character: 1110xxxx 10xxxxxx 10xxxxxx */
			*c = GB(s[0], 0, 4) << 12 | GB(s[1], 0, 6) << 6 | GB(s[2], 0, 6);
			if (*c >= 0x800) return 3;
		}
	} else if (GB(s[0], 3, 5) == 30) {
		if (IsUtf8Part(s[1]) && IsUtf8Part(s[2]) && IsUtf8Part(s[3])) {
			/* 4 byte character: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
			*c = GB(s[0], 0, 3) << 18 | GB(s[1], 0, 6) << 12 | GB(s[2], 0, 6) << 6 | GB(s[3], 0, 6);
			if (*c >= 0x10000 && *c <= 0x10FFFF) return 4;
		}
	}

	*c = '?';
	return 1;
}


/**
 * Encode a unicode character and place it in the buffer.
 * @tparam T Type of the buffer.
 * @param buf Buffer to place character.
 * @param c   Unicode character to encode.
 * @return Number of characters in the encoded sequence.
 */
template <class T>
inline size_t Utf8Encode(T buf, char32_t c)
{
	if (c < 0x80) {
		*buf = c;
		return 1;
	} else if (c < 0x800) {
		*buf++ = 0xC0 + GB(c,  6, 5);
		*buf   = 0x80 + GB(c,  0, 6);
		return 2;
	} else if (c < 0x10000) {
		*buf++ = 0xE0 + GB(c, 12, 4);
		*buf++ = 0x80 + GB(c,  6, 6);
		*buf   = 0x80 + GB(c,  0, 6);
		return 3;
	} else if (c < 0x110000) {
		*buf++ = 0xF0 + GB(c, 18, 3);
		*buf++ = 0x80 + GB(c, 12, 6);
		*buf++ = 0x80 + GB(c,  6, 6);
		*buf   = 0x80 + GB(c,  0, 6);
		return 4;
	}

	*buf = '?';
	return 1;
}

size_t Utf8Encode(char *buf, char32_t c)
{
	return Utf8Encode<char *>(buf, c);
}

size_t Utf8Encode(std::ostreambuf_iterator<char> &buf, char32_t c)
{
	return Utf8Encode<std::ostreambuf_iterator<char> &>(buf, c);
}

size_t Utf8Encode(std::back_insert_iterator<std::string> &buf, char32_t c)
{
	return Utf8Encode<std::back_insert_iterator<std::string> &>(buf, c);
}

/**
 * Properly terminate an UTF8 string to some maximum length
 * @param s string to check if it needs additional trimming
 * @param maxlen the maximum length the buffer can have.
 * @return the new length in bytes of the string (eg. strlen(new_string))
 * @note maxlen is the string length _INCLUDING_ the terminating '\0'
 */
size_t Utf8TrimString(char *s, size_t maxlen)
{
	size_t length = 0;

	for (const char *ptr = strchr(s, '\0'); *s != '\0';) {
		size_t len = Utf8EncodedCharLen(*s);
		/* Silently ignore invalid UTF8 sequences, our only concern trimming */
		if (len == 0) len = 1;

		/* Take care when a hard cutoff was made for the string and
		 * the last UTF8 sequence is invalid */
		if (length + len >= maxlen || (s + len > ptr)) break;
		s += len;
		length += len;
	}

	*s = '\0';
	return length;
}

#ifdef DEFINE_STRCASESTR
char *strcasestr(const char *haystack, const char *needle)
{
	size_t hay_len = strlen(haystack);
	size_t needle_len = strlen(needle);
	while (hay_len >= needle_len) {
		if (strncasecmp(haystack, needle, needle_len) == 0) return const_cast<char *>(haystack);

		haystack++;
		hay_len--;
	}

	return nullptr;
}
#endif /* DEFINE_STRCASESTR */

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
	while (!str.empty() && (str[0] < '0' || IsInsideMM(str[0], ';', '@' + 1) || IsInsideMM(str[0], '[', '`' + 1) || IsInsideMM(str[0], '{', '~' + 1))) str.remove_prefix(1);
	return str;
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

#include <unicode/stsearch.h>

/**
 * Search if a string is contained in another string using the current locale.
 *
 * @param str String to search in.
 * @param value String to search for.
 * @param case_insensitive Search case-insensitive.
 * @return 1 if value was found, 0 if it was not found, or -1 if not supported by the OS.
 */
static int ICUStringContains(const std::string_view str, const std::string_view value, bool case_insensitive)
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
[[nodiscard]] bool StrNaturalContains(const std::string_view str, const std::string_view value)
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
[[nodiscard]] bool StrNaturalContainsIgnoreCase(const std::string_view str, const std::string_view value)
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


#ifdef WITH_UNISCRIBE

/* static */ std::unique_ptr<StringIterator> StringIterator::Create()
{
	return std::make_unique<UniscribeStringIterator>();
}

#elif defined(WITH_ICU_I18N)

#include <unicode/utext.h>
#include <unicode/brkiter.h>

/** String iterator using ICU as a backend. */
class IcuStringIterator : public StringIterator
{
	icu::BreakIterator *char_itr; ///< ICU iterator for characters.
	icu::BreakIterator *word_itr; ///< ICU iterator for words.

	std::vector<UChar> utf16_str;      ///< UTF-16 copy of the string.
	std::vector<size_t> utf16_to_utf8; ///< Mapping from UTF-16 code point position to index in the UTF-8 source string.

public:
	IcuStringIterator() : char_itr(nullptr), word_itr(nullptr)
	{
		UErrorCode status = U_ZERO_ERROR;
		this->char_itr = icu::BreakIterator::createCharacterInstance(icu::Locale(_current_language != nullptr ? _current_language->isocode : "en"), status);
		this->word_itr = icu::BreakIterator::createWordInstance(icu::Locale(_current_language != nullptr ? _current_language->isocode : "en"), status);

		this->utf16_str.push_back('\0');
		this->utf16_to_utf8.push_back(0);
	}

	~IcuStringIterator() override
	{
		delete this->char_itr;
		delete this->word_itr;
	}

	void SetString(const char *s) override
	{
		const char *string_base = s;

		/* Unfortunately current ICU versions only provide rudimentary support
		 * for word break iterators (especially for CJK languages) in combination
		 * with UTF-8 input. As a work around we have to convert the input to
		 * UTF-16 and create a mapping back to UTF-8 character indices. */
		this->utf16_str.clear();
		this->utf16_to_utf8.clear();

		while (*s != '\0') {
			size_t idx = s - string_base;

			char32_t c = Utf8Consume(&s);
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
		this->utf16_to_utf8.push_back(s - string_base);

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
	const char *string; ///< Current string.
	size_t len;         ///< String length.
	size_t cur_pos;     ///< Current iteration position.

public:
	DefaultStringIterator() : string(nullptr), len(0), cur_pos(0)
	{
	}

	void SetString(const char *s) override
	{
		this->string = s;
		this->len = strlen(s);
		this->cur_pos = 0;
	}

	size_t SetCurPosition(size_t pos) override
	{
		assert(this->string != nullptr && pos <= this->len);
		/* Sanitize in case we get a position inside an UTF-8 sequence. */
		while (pos > 0 && IsUtf8Part(this->string[pos])) pos--;
		return this->cur_pos = pos;
	}

	size_t Next(IterType what) override
	{
		assert(this->string != nullptr);

		/* Already at the end? */
		if (this->cur_pos >= this->len) return END;

		switch (what) {
			case ITER_CHARACTER: {
				char32_t c;
				this->cur_pos += Utf8Decode(&c, this->string + this->cur_pos);
				return this->cur_pos;
			}

			case ITER_WORD: {
				char32_t c;
				/* Consume current word. */
				size_t offs = Utf8Decode(&c, this->string + this->cur_pos);
				while (this->cur_pos < this->len && !IsWhitespace(c)) {
					this->cur_pos += offs;
					offs = Utf8Decode(&c, this->string + this->cur_pos);
				}
				/* Consume whitespace to the next word. */
				while (this->cur_pos < this->len && IsWhitespace(c)) {
					this->cur_pos += offs;
					offs = Utf8Decode(&c, this->string + this->cur_pos);
				}

				return this->cur_pos;
			}

			default:
				NOT_REACHED();
		}

		return END;
	}

	size_t Prev(IterType what) override
	{
		assert(this->string != nullptr);

		/* Already at the beginning? */
		if (this->cur_pos == 0) return END;

		switch (what) {
			case ITER_CHARACTER:
				return this->cur_pos = Utf8PrevChar(this->string + this->cur_pos) - this->string;

			case ITER_WORD: {
				const char *s = this->string + this->cur_pos;
				char32_t c;
				/* Consume preceding whitespace. */
				do {
					s = Utf8PrevChar(s);
					Utf8Decode(&c, s);
				} while (s > this->string && IsWhitespace(c));
				/* Consume preceding word. */
				while (s > this->string && !IsWhitespace(c)) {
					s = Utf8PrevChar(s);
					Utf8Decode(&c, s);
				}
				/* Move caret back to the beginning of the word. */
				if (IsWhitespace(c)) Utf8Consume(&s);

				return this->cur_pos = s - this->string;
			}

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
