/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file string_func.h Functions related to low-level strings.
 *
 * @note Be aware of "dangerous" string functions; string functions that
 * have behaviour that could easily cause buffer overruns and such:
 * - strncpy: does not '\0' terminate when input string is longer than
 *   the size of the output string. Use strecpy instead.
 * - [v]snprintf: returns the length of the string as it would be written
 *   when the output is large enough, so it can be more than the size of
 *   the buffer and than can underflow size_t (uint-ish) which makes all
 *   subsequent snprintf alikes write outside of the buffer. Use
 *   [v]seprintf instead; it will return the number of bytes actually
 *   added so no [v]seprintf will cause outside of bounds writes.
 * - [v]sprintf: does not bounds checking: use [v]seprintf instead.
 */

#ifndef STRING_FUNC_H
#define STRING_FUNC_H

#include "core/bitmath_func.hpp"
#include "string_type.h"

/**
 * Appends characters from one string to another.
 *
 * Appends the source string to the destination string with respect of the
 * terminating null-character and the maximum size of the destination
 * buffer.
 *
 * @note usage ttd_strlcat(dst, src, lengthof(dst));
 * @note lengthof() applies only to fixed size arrays
 *
 * @param dst The buffer containing the target string
 * @param src The buffer containing the string to append
 * @param size The maximum size of the destination buffer
 */
void ttd_strlcat(char *dst, const char *src, size_t size);

/**
 * Copies characters from one buffer to another.
 *
 * Copies the source string to the destination buffer with respect of the
 * terminating null-character and the maximum size of the destination
 * buffer.
 *
 * @note usage ttd_strlcpy(dst, src, lengthof(dst));
 * @note lengthof() applies only to fixed size arrays
 *
 * @param dst The destination buffer
 * @param src The buffer containing the string to copy
 * @param size The maximum size of the destination buffer
 */
void ttd_strlcpy(char *dst, const char *src, size_t size);

/**
 * Appends characters from one string to another.
 *
 * Appends the source string to the destination string with respect of the
 * terminating null-character and and the last pointer to the last element
 * in the destination buffer. If the last pointer is set to NULL no
 * boundary check is performed.
 *
 * @note usage: strecat(dst, src, lastof(dst));
 * @note lastof() applies only to fixed size arrays
 *
 * @param dst The buffer containing the target string
 * @param src The buffer containing the string to append
 * @param last The pointer to the last element of the destination buffer
 * @return The pointer to the terminating null-character in the destination buffer
 */
char *strecat(char *dst, const char *src, const char *last);

/**
 * Copies characters from one buffer to another.
 *
 * Copies the source string to the destination buffer with respect of the
 * terminating null-character and the last pointer to the last element in
 * the destination buffer. If the last pointer is set to NULL no boundary
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
char *strecpy(char *dst, const char *src, const char *last);

int CDECL seprintf(char *str, const char *last, const char *format, ...) WARN_FORMAT(3, 4);

char *CDECL str_fmt(const char *str, ...) WARN_FORMAT(1, 2);

/**
 * Scans the string for valid characters and if it finds invalid ones,
 * replaces them with a question mark '?' (if not ignored)
 * @param str the string to validate
 * @param last the last valid character of str
 * @param allow_newlines whether newlines should be allowed or ignored
 * @param ignore whether to ignore or replace with a question mark
 */
void str_validate(char *str, const char *last, bool allow_newlines = false, bool ignore = false);

/** Scans the string for colour codes and strips them */
void str_strip_colours(char *str);

/** Convert the given string to lowercase, only works with ASCII! */
void strtolower(char *str);

/**
 * Checks whether the given string is valid, i.e. contains only
 * valid (printable) characters and is properly terminated.
 * @param str  The string to validate.
 * @param last The last character of the string, i.e. the string
 *             must be terminated here or earlier.
 */
bool StrValid(const char *str, const char *last);

/**
 * Check if a string buffer is empty.
 *
 * @param s The pointer to the firste element of the buffer
 * @return true if the buffer starts with the terminating null-character or
 *         if the given pointer points to NULL else return false
 */
static inline bool StrEmpty(const char *s)
{
	return s == NULL || s[0] == '\0';
}

/**
 * Get the length of a string, within a limited buffer.
 *
 * @param str The pointer to the firste element of the buffer
 * @param maxlen The maximum size of the buffer
 * @return The length of the string
 */
static inline size_t ttd_strnlen(const char *str, size_t maxlen)
{
	const char *t;
	for (t = str; (size_t)(t - str) < maxlen && *t != '\0'; t++) {}
	return t - str;
}

/** Convert the md5sum number to a 'hexadecimal' string, return next pos in buffer */
char *md5sumToString(char *buf, const char *last, const uint8 md5sum[16]);

/**
 * Only allow certain keys. You can define the filter to be used. This makes
 *  sure no invalid keys can get into an editbox, like BELL.
 * @param key character to be checked
 * @param afilter the filter to use
 * @return true or false depending if the character is printable/valid or not
 */
bool IsValidChar(WChar key, CharSetFilter afilter);

size_t Utf8Decode(WChar *c, const char *s);
size_t Utf8Encode(char *buf, WChar c);
size_t Utf8TrimString(char *s, size_t maxlen);


static inline WChar Utf8Consume(const char **s)
{
	WChar c;
	*s += Utf8Decode(&c, *s);
	return c;
}


/**
 * Return the length of a UTF-8 encoded character.
 * @param c Unicode character.
 * @return Length of UTF-8 encoding for character.
 */
static inline int8 Utf8CharLen(WChar c)
{
	if (c < 0x80)       return 1;
	if (c < 0x800)      return 2;
	if (c < 0x10000)    return 3;
	if (c < 0x110000)   return 4;

	/* Invalid valid, we encode as a '?' */
	return 1;
}


/**
 * Return the length of an UTF-8 encoded value based on a single char. This
 * char should be the first byte of the UTF-8 encoding. If not, or encoding
 * is invalid, return value is 0
 * @param c char to query length of
 * @return requested size
 */
static inline int8 Utf8EncodedCharLen(char c)
{
	if (GB(c, 3, 5) == 0x1E) return 4;
	if (GB(c, 4, 4) == 0x0E) return 3;
	if (GB(c, 5, 3) == 0x06) return 2;
	if (GB(c, 7, 1) == 0x00) return 1;

	/* Invalid UTF8 start encoding */
	return 0;
}


/* Check if the given character is part of a UTF8 sequence */
static inline bool IsUtf8Part(char c)
{
	return GB(c, 6, 2) == 2;
}

/**
 * Retrieve the previous UNICODE character in an UTF-8 encoded string.
 * @param s char pointer pointing to (the first char of) the next character
 * @return a pointer in 's' to the previous UNICODE character's first byte
 * @note The function should not be used to determine the length of the previous
 * encoded char because it might be an invalid/corrupt start-sequence
 */
static inline char *Utf8PrevChar(char *s)
{
	char *ret = s;
	while (IsUtf8Part(*--ret)) {}
	return ret;
}

size_t Utf8StringLength(const char *s);

/**
 * Is the given character a text direction character.
 * @param c The character to test.
 * @return true iff the character is used to influence
 *         the text direction.
 */
static inline bool IsTextDirectionChar(WChar c)
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

static inline bool IsPrintable(WChar c)
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
static inline bool IsWhitespace(WChar c)
{
	return c == 0x0020 /* SPACE */ || c == 0x3000; /* IDEOGRAPHIC SPACE */
}

/* Needed for NetBSD version (so feature) testing */
#if defined(__NetBSD__) || defined(__FreeBSD__)
#include <sys/param.h>
#endif

/* strndup is a GNU extension */
#if defined(_GNU_SOURCE) || (defined(__NetBSD_Version__) && 400000000 <= __NetBSD_Version__) || (defined(__FreeBSD_version) && 701101 <= __FreeBSD_version)
#	undef DEFINE_STRNDUP
#else
#	define DEFINE_STRNDUP
char *strndup(const char *s, size_t len);
#endif /* strndup is available */

/* strcasestr is available for _GNU_SOURCE, BSD and some Apple */
#if defined(_GNU_SOURCE) || (defined(__BSD_VISIBLE) && __BSD_VISIBLE) || (defined(__APPLE__) && (!defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE))) || defined(_NETBSD_SOURCE)
#	undef DEFINE_STRCASESTR
#else
#	define DEFINE_STRCASESTR
char *strcasestr(const char *haystack, const char *needle);
#endif /* strcasestr is available */

#endif /* STRING_FUNC_H */
