#include "stdafx.h"
#include "string.h"

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
