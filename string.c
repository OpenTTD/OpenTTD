/* $Id$ */

#include "stdafx.h"
#include "string.h"

#include <stdarg.h>
#if defined(UNIX) || defined(__OS2__)
#include <ctype.h> // required for tolower()
#endif

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
		if (!IsValidAsciiChar(*str)) *str = '?';
}

void strtolower(char *str)
{
	for (; *str != '\0'; str++) *str = tolower(*str);
}
