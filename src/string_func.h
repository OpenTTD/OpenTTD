/* $Id$ */

/** @file string_func.h Functions related to low-level strings. */

#ifndef STRING_FUNC_H
#define STRING_FUNC_H

#include "core/bitmath_func.hpp"
#include "string_type.h"

/**
 * usage ttd_strlcpy(dst, src, lengthof(dst));
 * @param dst destination buffer
 * @param src string to copy/concatenate
 * @param size size of the destination buffer
 */
void ttd_strlcat(char *dst, const char *src, size_t size);
void ttd_strlcpy(char *dst, const char *src, size_t size);

/**
 * usage: strecpy(dst, src, lastof(dst));
 * @param dst destination buffer
 * @param src string to copy
 * @param last pointer to the last element in the dst array
 *             if NULL no boundary check is performed
 * @return a pointer to the terminating \0 in the destination buffer
 */
char *strecat(char *dst, const char *src, const char *last);
char *strecpy(char *dst, const char *src, const char *last);

char *CDECL str_fmt(const char *str, ...);

/** Scans the string for valid characters and if it finds invalid ones,
 * replaces them with a question mark '?' */
void str_validate(char *str);

/** Scans the string for colour codes and strips them */
void str_strip_colours(char *str);

/** Convert the given string to lowercase, only works with ASCII! */
void strtolower(char *str);


static inline bool StrEmpty(const char *s) { return s == NULL || s[0] == '\0'; }


/** Get the length of a string, within a limited buffer */
static inline int ttd_strnlen(const char *str, int maxlen)
{
	const char *t;
	for (t = str; *t != '\0' && t - str < maxlen; t++);
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


/** Return the length of a UTF-8 encoded character.
 * @param c Unicode character.
 * @return Length of UTF-8 encoding for character.
 */
static inline size_t Utf8CharLen(WChar c)
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
static inline size_t Utf8EncodedCharLen(char c)
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
static inline char *Utf8PrevChar(const char *s)
{
	const char *ret = s;
	while (IsUtf8Part(*--ret));
	return (char*)ret;
}


static inline bool IsPrintable(WChar c)
{
	if (c < 0x20)   return false;
	if (c < 0xE000) return true;
	if (c < 0xE200) return false;
	return true;
}

/**
 * Check whether UNICODE character is whitespace or not
 * @param c UNICODE character to check
 * @return a boolean value whether 'c' is a whitespace character or not
 * @see http://www.fileformat.info/info/unicode/category/Zs/list.htm
 */
static inline bool IsWhitespace(WChar c)
{
	return
	  c == 0x0020 /* SPACE */ ||
	  c == 0x00A0 /* NO-BREAK SPACE */ ||
	  c == 0x3000 /* IDEOGRAPHIC SPACE */
	;
}

#endif /* STRING_FUNC_H */
