/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "string.h"
#include "macros.h"
#include "table/control_codes.h"

#include <stdarg.h>
#include <ctype.h> // required for tolower()

void ttd_strlcat(char *dst, const char *src, size_t size)
{
	assert(size > 0);
	for (; size > 0 && *dst != '\0'; --size, ++dst) {}
	assert(size > 0);
	while (--size > 0 && *src != '\0') *dst++ = *src++;
	*dst = '\0';
}


void ttd_strlcpy(char *dst, const char *src, size_t size)
{
	assert(size > 0);
	while (--size > 0 && *src != '\0') *dst++ = *src++;
	*dst = '\0';
}


char* strecat(char* dst, const char* src, const char* last)
{
	assert(dst <= last);
	for (; *dst != '\0'; ++dst)
		if (dst == last) return dst;
	for (; *src != '\0' && dst != last; ++dst, ++src) *dst = *src;
	*dst = '\0';
	return strecpy(dst, src, last);
}


char* strecpy(char* dst, const char* src, const char* last)
{
	assert(dst <= last);
	for (; *src != '\0' && dst != last; ++dst, ++src) *dst = *src;
	*dst = '\0';
#if 1
	if (dst == last && *src != '\0') {
		error("String too long for destination buffer");
	}
#endif
	return dst;
}


char* CDECL str_fmt(const char* str, ...)
{
	char buf[4096];
	va_list va;
	int len;
	char* p;

	va_start(va, str);
	len = vsnprintf(buf, lengthof(buf), str, va);
	va_end(va);
	p = malloc(len + 1);
	if (p != NULL) memcpy(p, buf, len + 1);
	return p;
}

void str_validate(char *str)
{
	char *dst = str;
	WChar c;
	size_t len = Utf8Decode(&c, str);

	for (; c != '\0'; len = Utf8Decode(&c, str)) {
		if (IsPrintable(c) && (c < SCC_SPRITE_START || c > SCC_SPRITE_END ||
			IsValidChar(c - SCC_SPRITE_START, CS_ALPHANUMERAL))) {
			/* Copy the character back. Even if dst is current the same as str
			 * (i.e. no characters have been changed) this is quicker than
			 * moving the pointers ahead by len */
			do {
				*dst++ = *str++;
			} while (--len);
		} else {
			/* Replace the undesirable character with a question mark */
			str += len;
			*dst++ = '?';
		}
	}

	*dst = '\0';
}

void str_strip_colours(char *str)
{
	char *dst = str;
	for (; *str != '\0';) {
		if (*str >= 15 && *str <= 31) { // magic colour codes
			str++;
		} else {
			*dst++ = *str++;
		}
	}
	*dst = '\0';
}

/**
 * Only allow certain keys. You can define the filter to be used. This makes
 *  sure no invalid keys can get into an editbox, like BELL.
 * @param key character to be checked
 * @param afilter the filter to use
 * @return true or false depending if the character is printable/valid or not
 */
bool IsValidChar(WChar key, CharSetFilter afilter)
{
	switch (afilter) {
		case CS_ALPHANUMERAL: return IsPrintable(key);
		case CS_NUMERAL:      return (key >= '0' && key <= '9');
		case CS_ALPHA:        return IsPrintable(key) && !(key >= '0' && key <= '9');
	}

	return false;
}

void strtolower(char *str)
{
	for (; *str != '\0'; str++) *str = tolower(*str);
}

#ifdef WIN32
int CDECL snprintf(char *str, size_t size, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vsnprintf(str, size, format, ap);
	va_end(ap);
	return ret;
}

#ifdef _MSC_VER
int CDECL vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	int ret;
	ret = _vsnprintf(str, size, format, ap);
	if (ret < 0) str[size - 1] = '\0';
	return ret;
}
#endif /* _MSC_VER */

#endif /* WIN32 */


/* UTF-8 handling routines */


/* Decode and consume the next UTF-8 encoded character
 * @param c Buffer to place decoded character.
 * @param s Character stream to retrieve character from.
 * @return Number of characters in the sequence.
 */
size_t Utf8Decode(WChar *c, const char *s)
{
	assert(c != NULL);

	if (!HASBIT(s[0], 7)) {
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

	//DEBUG(misc, 1) ("Invalid UTF-8 sequence");
	*c = '?';
	return 1;
}


/* Encode a unicode character and place it in the buffer
 * @param buf Buffer to place character.
 * @param c   Unicode character to encode.
 * @return Number of characters in the encoded sequence.
 */
size_t Utf8Encode(char *buf, WChar c)
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

	//DEBUG(misc, 1) ("Can't UTF-8 encode value 0x%X", c);
	*buf = '?';
	return 1;
}
