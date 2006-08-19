/* $Id$ */

#include "stdafx.h"
#include "string.h"

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
	assert(last == NULL || dst <= last);
	for (; *dst != '\0'; ++dst)
		if (dst == last) return dst;
	for (; *src != '\0' && dst != last; ++dst, ++src) *dst = *src;
	*dst = '\0';
	return strecpy(dst, src, last);
}


char* strecpy(char* dst, const char* src, const char* last)
{
	assert(last == NULL || dst <= last);
	for (; *src != '\0' && dst != last; ++dst, ++src) *dst = *src;
	*dst = '\0';
	return dst;
}


char* CDECL str_fmt(const char* str, ...)
{
	char buf[4096];
	va_list va;
	int len;
	char* p;

	va_start(va, str);
	len = vsprintf(buf, str, va);
	va_end(va);
	p = malloc(len + 1);
	if (p != NULL) memcpy(p, buf, len + 1);
	return p;
}

void str_validate(char *str)
{
	for (; *str != '\0'; str++)
		if (!IsValidAsciiChar(*str, CS_ALPHANUMERAL)) *str = '?';
}

void strtolower(char *str)
{
	for (; *str != '\0'; str++) *str = tolower(*str);
}

/** Only allow valid ascii-function codes. Filter special codes like BELL and
 * so on [we need a special filter here later]
 * @param key character to be checked
 * @return true or false depending if the character is printable/valid or not */
bool IsValidAsciiChar(byte key, CharSetFilter afilter)
{
	// XXX This filter stops certain crashes, but may be too restrictive.
	bool firsttest = false;

	switch (afilter) {
		case CS_ALPHANUMERAL:
			firsttest = (key >= ' ' && key < 127);
			break;

		case CS_NUMERAL://we are quite strict, here
			return (key >= 48 && key <= 57);

		case CS_ALPHA:
		default:
			firsttest = ((key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z'));
			break;
	}

	return (firsttest || (key >= 160 &&
		key != 0xAA && key != 0xAC && key != 0xAD && key != 0xAF &&
		key != 0xB5 && key != 0xB6 && key != 0xB7 && key != 0xB9));
}
