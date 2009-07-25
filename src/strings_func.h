/* $Id$ */

/** @file strings_func.h Functions related to OTTD's strings. */

#ifndef STRINGS_FUNC_H
#define STRINGS_FUNC_H

#include "strings_type.h"

char *InlineString(char *buf, StringID string);
char *GetString(char *buffr, StringID string, const char *last);
const char *GetStringPtr(StringID string);

void InjectDParam(uint amount);

/**
 * Set a string parameter \a v at index \a n in a given array \a s.
 * @param s Array of string parameters.
 * @param n Index of the string parameter.
 * @param v Value of the string parameter.
 */
static inline void SetDParamX(uint64 *s, uint n, uint64 v)
{
	s[n] = v;
}

/**
 * Set a string parameter \a v at index \a n in the global string parameter array.
 * @param n Index of the string parameter.
 * @param v Value of the string parameter.
 */
static inline void SetDParam(uint n, uint64 v)
{
	extern uint64 _decode_parameters[20];

	assert(n < lengthof(_decode_parameters));
	_decode_parameters[n] = v;
}

void SetDParamStr(uint n, const char *str);

/**
 * Get the current string parameter at index \a n from parameter array \a s.
 * @param s Array of string parameters.
 * @param n Index of the string parameter.
 * @return Value of the requested string parameter.
 */
static inline uint64 GetDParamX(const uint64 *s, uint n)
{
	return s[n];
}

/**
 * Get the current string parameter at index \a n from the global string parameter array.
 * @param s Array of string parameters.
 * @param n Index of the string parameter.
 * @return Value of the requested string parameter.
 */
static inline uint64 GetDParam(uint n)
{
	extern uint64 _decode_parameters[20];

	assert(n < lengthof(_decode_parameters));
	return _decode_parameters[n];
}

/**
 * Copy \a num string parameters from array \a src into the global string parameter array.
 * @param offs Index in the global array to copy the first string parameter to.
 * @param src  Source array of string parameters.
 * @param num  Number of string parameters to copy.
 */
static inline void CopyInDParam(int offs, const uint64 *src, int num)
{
	extern uint64 _decode_parameters[20];
	memcpy(_decode_parameters + offs, src, sizeof(uint64) * (num));
}

/**
 * Copy \a num string parameters from the global string parameter array to the \a dst array.
 * @param dst  Destination array of string parameters.
 * @param offs Index in the global array to copy the first string parameter from.
 * @param num  Number of string parameters to copy.
 */
static inline void CopyOutDParam(uint64 *dst, int offs, int num)
{
	extern uint64 _decode_parameters[20];
	memcpy(dst, _decode_parameters + offs, sizeof(uint64) * (num));
}

extern DynamicLanguages _dynlang; // defined in strings.cpp

bool ReadLanguagePack(int index);
void InitializeLanguagePacks();

int CDECL StringIDSorter(const void *a, const void *b);

/** Key comparison function for std::map */
struct StringIDCompare
{
	bool operator()(StringID s1, StringID s2) const { return StringIDSorter(&s1, &s2) < 0; }
};

void CheckForMissingGlyphsInLoadedLanguagePack();

#endif /* STRINGS_TYPE_H */
