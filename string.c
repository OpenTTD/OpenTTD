/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
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
	for (; *str != '\0'; str++)
		if (!IsValidAsciiChar(*str, CS_ALPHANUMERAL)) *str = '?';
}

/**
 * Only allow certain keys. You can define the filter to be used. This makes
 *  sure no invalid keys can get into an editbox, like BELL.
 * @param key character to be checked
 * @param afilter the filter to use
 * @return true or false depending if the character is printable/valid or not
 */
bool IsValidAsciiChar(byte key, CharSetFilter afilter)
{
	bool firsttest = false;

	switch (afilter) {
		case CS_ALPHANUMERAL:
			firsttest = (key >= ' ' && key < 127);
			break;

		/* We are very strict here */
		case CS_NUMERAL:
			return (key >= '0' && key <= '9');

		case CS_ALPHA:
		default:
			firsttest = ((key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z'));
			break;
	}

	/* Allow some special chars too that are non-ASCII but still valid (like '^' above 'a') */
	return (firsttest || (key >= 160 &&
		key != 0xAA && key != 0xAC && key != 0xAD && key != 0xAF &&
		key != 0xB5 && key != 0xB6 && key != 0xB7 && key != 0xB9));
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
