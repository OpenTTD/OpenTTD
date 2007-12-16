/* $Id$ */

/** @file strings.h */

#ifndef STRINGS_H
#define STRINGS_H

char *InlineString(char *buf, StringID string);
char *GetString(char *buffr, StringID string, const char *last);

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


/** Information about a language */
struct Language {
	char *name; ///< The internal name of the language
	char *file; ///< The name of the language as it appears on disk
};

/** Used for dynamic language support */
struct DynamicLanguages {
	int num;                         ///< Number of languages
	int curr;                        ///< Currently selected language index
	char curr_file[MAX_PATH];        ///< Currently selected language file name without path (needed for saving the filename of the loaded language).
	StringID dropdown[MAX_LANG + 1]; ///< List of languages in the settings gui
	Language ent[MAX_LANG];          ///< Information about the languages
};

extern DynamicLanguages _dynlang; // defined in strings.cpp

bool ReadLanguagePack(int index);
void InitializeLanguagePacks();

int CDECL StringIDSorter(const void *a, const void *b);

void CheckForMissingGlyphsInLoadedLanguagePack();

#endif /* STRINGS_H */
