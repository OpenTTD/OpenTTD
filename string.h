/* $Id$ */

#ifndef STRING_H
#define STRING_H

/*
 * dst: destination buffer
 * src: string to copy/concatenate
 * size: size of the destination buffer
 * usage: ttd_strlcpy(dst, src, lengthof(dst));
 */
void ttd_strlcat(char *dst, const char *src, size_t size);
void ttd_strlcpy(char *dst, const char *src, size_t size);

/*
 * dst: destination buffer
 * src: string to copy
 * last: pointer to the last element in the dst array
 *       if NULL no boundary check is performed
 * returns a pointer to the terminating \0 in the destination buffer
 * usage: strecpy(dst, src, lastof(dst));
 */
char* strecat(char* dst, const char* src, const char* last);
char* strecpy(char* dst, const char* src, const char* last);

char* CDECL str_fmt(const char* str, ...);

#endif /* STRING_H */
