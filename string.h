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

/** Scans the string for colour codes and strips them */
void str_strip_colours(char *str);

/**
 * Valid filter types for IsValidAsciiChar.
 */
typedef enum CharSetFilter {
	CS_ALPHANUMERAL,      //! Both numeric and alphabetic and spaces and stuff
	CS_NUMERAL,           //! Only numeric ones
	CS_ALPHA,             //! Only alphabetic values
} CharSetFilter;

/**
 * Only allow certain keys. You can define the filter to be used. This makes
 *  sure no invalid keys can get into an editbox, like BELL.
 * @param key character to be checked
 * @param afilter the filter to use
 * @return true or false depending if the character is printable/valid or not
 */
bool IsValidAsciiChar(byte key, CharSetFilter afilter);

/** Convert the given string to lowercase */
void strtolower(char *str);

#endif /* STRING_H */
