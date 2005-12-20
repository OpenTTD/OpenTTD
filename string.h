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

/** Scans the string for valid characters and if it finds invalid ones,
 * replaces them with a question mark '?' */
void str_validate(char *str);

/** Only allow valid ascii-function codes. Filter special codes like BELL and
 * so on [we need a special filter here later]
 * @param key character to be checked
 * @return true or false depending if the character is printable/valid or not */
static inline bool IsValidAsciiChar(byte key)
{
	// XXX This filter stops certain crashes, but may be too restrictive.
	return (key >= ' ' && key < 127) || (key >= 160 &&
		key != 0xAA && key != 0xAC && key != 0xAD && key != 0xAF &&
		key != 0xB5 && key != 0xB6 && key != 0xB7 && key != 0xB9);
}

#endif /* STRING_H */
