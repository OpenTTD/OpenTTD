/* $Id$ */

/** @file strings_func.h Functions related to OTTD's strings. */

#ifndef STRINGS_FUNC_H
#define STRINGS_FUNC_H

#include "strings_type.h"

char *InlineString(char *buf, StringID string);
char *GetString(char *buffr, StringID string, const char *last);
const char *GetStringPtr(StringID string);

extern char _userstring[128];

void InjectDParam(int amount);

static inline void SetDParamX(uint64 *s, uint n, uint64 v)
{
	s[n] = v;
}

static inline void SetDParam(uint n, uint64 v)
{
	extern uint64 _decode_parameters[20];

	assert(n < lengthof(_decode_parameters));
	_decode_parameters[n] = v;
}

/* Used to bind a C string name to a dparam number.
 * NOTE: This has a short lifetime. You can't
 *       use this string much later or it will be gone. */
void SetDParamStr(uint n, const char *str);

/** This function takes a C-string and allocates a temporary string ID.
 * The duration of the bound string is valid only until the next call to GetString,
 * so be careful. */
StringID BindCString(const char *str);

static inline uint64 GetDParamX(const uint64 *s, uint n)
{
	return s[n];
}

static inline uint64 GetDParam(uint n)
{
	extern uint64 _decode_parameters[20];

	assert(n < lengthof(_decode_parameters));
	return _decode_parameters[n];
}

static inline void CopyInDParam(int offs, const uint64 *src, int num)
{
	extern uint64 _decode_parameters[20];
	memcpy(_decode_parameters + offs, src, sizeof(uint64) * (num));
}

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

StringID RemapOldStringID(StringID s);
char *CopyFromOldName(StringID id);

#endif /* STRINGS_TYPE_H */
