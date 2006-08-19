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

/** Convert the given string to lowercase */
void strtolower(char *str);

typedef enum CharSetFilter {  //valid char filtering
	CS_ALPHANUMERAL,   //both numeric and alphabetic
	CS_NUMERAL,        //only numeric ones.
	CS_ALPHA,          //only alphabetic values
} CharSetFilter;

/** Only allow valid ascii-function codes. Filter special codes like BELL and
 * so on [we need a special filter here later]
 * @param key character to be checked
 * @return true or false depending if the character is printable/valid or not */
bool IsValidAsciiChar(byte key, CharSetFilter afilter);

#endif /* STRING_H */
