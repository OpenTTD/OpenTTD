/* $Id$ */

/** @file
 * Implementation of  Action 04 "universal holder" structure and functions.
 * This file implements a linked-lists of strings,
 * holding everything that the newgrf action 04 will send over to OpenTTD.
 * One of the biggest problems is that Dynamic lang Array uses ISO codes
 * as way to identifying current user lang, while newgrf uses bit shift codes
 * not related to ISO.  So equivalence functionnality had to be set.
 */

#include "stdafx.h"
#include "debug.h"
#include "openttd.h"
#include "string.h"
#include "strings.h"
#include "variables.h"
#include "macros.h"
#include "table/strings.h"
#include "newgrf_text.h"
#include "table/control_codes.h"

#define GRFTAB  28
#define TABSIZE 11

/**
 * Explains the newgrf shift bit positionning.
 * the grf base will not be used in order to find the string, but rather for
 * jumping from standard langID scheme to the new one.
 */
typedef enum grf_base_languages {
	GRFLB_AMERICAN    = 0x01,
	GRFLB_ENGLISH     = 0x02,
	GRFLB_GERMAN      = 0x04,
	GRFLB_FRENCH      = 0x08,
	GRFLB_SPANISH     = 0x10,
	GRFLB_GENERIC     = 0x80,
} grf_base_language;

typedef enum grf_extended_languages {
	GRFLX_AMERICAN    = 0x00,
	GRFLX_ENGLISH     = 0x01,
	GRFLX_GERMAN      = 0x02,
	GRFLX_FRENCH      = 0x03,
	GRFLX_SPANISH     = 0x04,
	GRFLX_RUSSIAN     = 0x07,
	GRFLX_CZECH       = 0x15,
	GRFLX_SLOVAK      = 0x16,
	GRFLX_AFRIKAANS   = 0x1B,
	GRFLX_GREEK       = 0x1E,
	GRFLX_DUTCH       = 0x1F,
	GRFLX_CATALAN     = 0x22,
	GRFLX_HUNGARIAN   = 0x24,
	GRFLX_ITALIAN     = 0x27,
	GRFLX_ROMANIAN    = 0x28,
	GRFLX_ICELANDIC   = 0x29,
	GRFLX_LATVIAN     = 0x2A,
	GRFLX_LITHUANIAN  = 0x2B,
	GRFLX_SLOVENIAN   = 0x2C,
	GRFLX_DANISH      = 0x2D,
	GRFLX_SWEDISH     = 0x2E,
	GRFLX_NORWEGIAN   = 0x2F,
	GRFLX_POLISH      = 0x30,
	GRFLX_GALICIAN    = 0x31,
	GRFLX_FRISIAN     = 0x32,
	GRFLX_UKRAINIAN   = 0x33,
	GRFLX_ESTONIAN    = 0x34,
	GRFLX_FINNISH     = 0x35,
	GRFLX_PORTUGUESE  = 0x36,
	GRFLX_BRAZILIAN   = 0x37,
	GRFLX_CROATIAN    = 0x38,
	GRFLX_TURKISH     = 0x3E,
	GRFLX_UNSPECIFIED = 0x7F,
} grf_language;


typedef struct iso_grf {
	char code[6];
	byte grfLangID;
} iso_grf;

/**
 * ISO code VS NewGrf langID conversion array.
 * This array is used in two ways:
 * 1-its ISO part is matching OpenTTD dynamic language id
 *   with newgrf bit positionning language id
 * 2-its shift part is used to know what is the shift to
 *   watch for when inserting new strings, hence analysing newgrf langid
 */
const iso_grf iso_codes[] = {
	{"en_US", GRFLX_AMERICAN},
	{"en_GB", GRFLX_ENGLISH},
	{"de_DE", GRFLX_GERMAN},
	{"fr_FR", GRFLX_FRENCH},
	{"es_ES", GRFLX_SPANISH},
	{"af_ZA", GRFLX_AFRIKAANS},
	{"hr_HR", GRFLX_CROATIAN},
	{"cs_CS", GRFLX_CZECH},
	{"ca_ES", GRFLX_CATALAN},
	{"da_DA", GRFLX_DANISH},
	{"nl_NL", GRFLX_DUTCH},
	{"et_ET", GRFLX_ESTONIAN},
	{"fi_FI", GRFLX_FINNISH},
	{"fy_NL", GRFLX_FRISIAN},
	{"gl_ES", GRFLX_GALICIAN},
	{"el_GR", GRFLX_GREEK},
	{"hu_HU", GRFLX_HUNGARIAN},
	{"is_IS", GRFLX_ICELANDIC},
	{"it_IT", GRFLX_ITALIAN},
	{"lv_LV", GRFLX_LATVIAN},
	{"lt_LT", GRFLX_LITHUANIAN},
	{"nb_NO", GRFLX_NORWEGIAN},
	{"pl_PL", GRFLX_POLISH},
	{"pt_PT", GRFLX_PORTUGUESE},
	{"pt_BR", GRFLX_BRAZILIAN},
	{"ro_RO", GRFLX_ROMANIAN},
	{"ru_RU", GRFLX_RUSSIAN},
	{"sk_SK", GRFLX_SLOVAK},
	{"sl_SL", GRFLX_SLOVENIAN},
	{"sv_SE", GRFLX_SWEDISH},
	{"tr_TR", GRFLX_TURKISH},
	{"uk_UA", GRFLX_UKRAINIAN},
	{"gen",   GRFLB_GENERIC}   //this is not iso code, but there has to be something...
};


/**
 * Element of the linked list.
 * Each of those elements represent the string,
 * but according to a different lang.
 */
typedef struct GRFText {
	struct GRFText *next;
	byte langid;
	char text[VARARRAY_SIZE];
} GRFText;


/**
 * Holder of the above structure.
 * Putting both grfid and stringid together allows us to avoid duplicates,
 * since it is NOT SUPPOSED to happen.
 */
typedef struct GRFTextEntry {
	uint32 grfid;
	uint16 stringid;
	StringID def_string;
	GRFText *textholder;
} GRFTextEntry;


static uint _num_grf_texts = 0;
static GRFTextEntry _grf_text[(1 << TABSIZE) * 3];
static byte _currentLangID = GRFLX_ENGLISH;  //by default, english is used.


char *TranslateTTDPatchCodes(const char *str)
{
	char *tmp = malloc(strlen(str) * 10 + 1); /* Allocate space to allow for expansion */
	char *d = tmp;
	bool unicode = false;
	WChar c;
	size_t len = Utf8Decode(&c, str);

	if (c == 0x00DE) {
		/* The thorn ('þ') indicates a unicode string to TTDPatch */
		unicode = true;
		str += len;
	}

	for (;;) {
		const char *tmp = str; /* Used for UTF-8 decoding */

		c = (byte)*str++;
		if (c == 0) break;

		switch (c) {
			case 0x01:
				d += Utf8Encode(d, SCC_SETX);
				*d++ = *str++;
				break;
			case 0x0A: break;
			case 0x0D: *d++ = 0x0A; break;
			case 0x0E: d += Utf8Encode(d, SCC_TINYFONT); break;
			case 0x0F: d += Utf8Encode(d, SCC_BIGFONT); break;
			case 0x1F:
				d += Utf8Encode(d, SCC_SETXY);
				*d++ = *str++;
				*d++ = *str++;
				break;
			case 0x7B:
			case 0x7C:
			case 0x7D:
			case 0x7E: d += Utf8Encode(d, SCC_NUM); break;
			case 0x7F: d += Utf8Encode(d, SCC_CURRENCY); break;
			case 0x80: d += Utf8Encode(d, SCC_STRING); break;
			case 0x81: {
				StringID string;
				string  = *str++;
				string |= *str++ << 8;
				d += Utf8Encode(d, SCC_STRING_ID);
				d += Utf8Encode(d, string);
				break;
			}
			case 0x82: d += Utf8Encode(d, SCC_DATE_TINY); break;
			case 0x83: d += Utf8Encode(d, SCC_DATE_SHORT); break;
			case 0x84: d += Utf8Encode(d, SCC_VELOCITY); break;
			case 0x85: d += Utf8Encode(d, SCC_SKIP);    break;
			case 0x86: /* "Rotate down top 4 words on stack" */ break;
			case 0x87: d += Utf8Encode(d, SCC_VOLUME);  break;
			case 0x88: d += Utf8Encode(d, SCC_BLUE);    break;
			case 0x89: d += Utf8Encode(d, SCC_SILVER);  break;
			case 0x8A: d += Utf8Encode(d, SCC_GOLD);    break;
			case 0x8B: d += Utf8Encode(d, SCC_RED);     break;
			case 0x8C: d += Utf8Encode(d, SCC_PURPLE);  break;
			case 0x8D: d += Utf8Encode(d, SCC_LTBROWN); break;
			case 0x8E: d += Utf8Encode(d, SCC_ORANGE);  break;
			case 0x8F: d += Utf8Encode(d, SCC_GREEN);   break;
			case 0x90: d += Utf8Encode(d, SCC_YELLOW);  break;
			case 0x91: d += Utf8Encode(d, SCC_DKGREEN); break;
			case 0x92: d += Utf8Encode(d, SCC_CREAM);   break;
			case 0x93: d += Utf8Encode(d, SCC_BROWN);   break;
			case 0x94: d += Utf8Encode(d, SCC_WHITE);   break;
			case 0x95: d += Utf8Encode(d, SCC_LTBLUE);  break;
			case 0x96: d += Utf8Encode(d, SCC_GRAY);    break;
			case 0x97: d += Utf8Encode(d, SCC_DKBLUE);  break;
			case 0x98: d += Utf8Encode(d, SCC_BLACK);   break;
			case 0x9E: d += Utf8Encode(d, 0x20AC); break; // Euro
			case 0x9F: d += Utf8Encode(d, 0x0178); break; // Y with diaeresis
			case 0xA0: d += Utf8Encode(d, SCC_UPARROW); break;
			case 0xAA: d += Utf8Encode(d, SCC_DOWNARROW); break;
			case 0xAC: d += Utf8Encode(d, SCC_CHECKMARK); break;
			case 0xAD: d += Utf8Encode(d, SCC_CROSS); break;
			case 0xAF: d += Utf8Encode(d, SCC_RIGHTARROW); break;
			case 0xB4: d += Utf8Encode(d, SCC_TRAIN); break;
			case 0xB5: d += Utf8Encode(d, SCC_LORRY); break;
			case 0xB6: d += Utf8Encode(d, SCC_BUS); break;
			case 0xB7: d += Utf8Encode(d, SCC_PLANE); break;
			case 0xB8: d += Utf8Encode(d, SCC_SHIP); break;
			default:
				if (unicode) {
					d += Utf8Encode(d, Utf8Consume(&tmp));
					str = tmp;
					break;
				}

				/* Validate any unhandled character */
				if (!IsValidChar(c, CS_ALPHANUMERAL)) c = '?';
				d += Utf8Encode(d, c);
				break;
		}
	}

	*d = '\0';
	return realloc(tmp, strlen(tmp) + 1);
}


/**
 * Add the new read string into our structure.
 */
StringID AddGRFString(uint32 grfid, uint16 stringid, byte langid_to_add, bool new_scheme, const char *text_to_add, StringID def_string)
{
	char *translatedtext;
	GRFText *newtext;
	uint id;

	/* When working with the old language scheme (grf_version is less than 7) and
	 * English or American is among the set bits, simply add it as English in
	 * the new scheme, i.e. as langid = 1.
	 * If English is set, it is pretty safe to assume the translations are not
	 * actually translated.
	 */
	if (!new_scheme) {
		if (HASBITS(langid_to_add, GRFLB_AMERICAN | GRFLB_ENGLISH)) {
			langid_to_add = GRFLX_ENGLISH;
		} else {
			StringID ret = STR_EMPTY;
			if (langid_to_add & GRFLB_GERMAN)  ret = AddGRFString(grfid, stringid, GRFLX_GERMAN,  true, text_to_add, def_string);
			if (langid_to_add & GRFLB_FRENCH)  ret = AddGRFString(grfid, stringid, GRFLX_FRENCH,  true, text_to_add, def_string);
			if (langid_to_add & GRFLB_SPANISH) ret = AddGRFString(grfid, stringid, GRFLX_SPANISH, true, text_to_add, def_string);
			return ret;
		}
	}

	for (id = 0; id < _num_grf_texts; id++) {
		if (_grf_text[id].grfid == grfid && _grf_text[id].stringid == stringid) {
			break;
		}
	}

	/* Too many strings allocated, return empty */
	if (id == lengthof(_grf_text)) return STR_EMPTY;

	translatedtext = TranslateTTDPatchCodes(text_to_add);

	newtext = malloc(sizeof(*newtext) + strlen(translatedtext) + 1);
	newtext->next   = NULL;
	newtext->langid = langid_to_add;
	strcpy(newtext->text, translatedtext);

	free(translatedtext);

	/* If we didn't find our stringid and grfid in the list, allocate a new id */
	if (id == _num_grf_texts) _num_grf_texts++;

	if (_grf_text[id].textholder == NULL) {
		_grf_text[id].grfid      = grfid;
		_grf_text[id].stringid   = stringid;
		_grf_text[id].def_string = def_string;
		_grf_text[id].textholder = newtext;
	} else {
		GRFText **ptext, *text;
		bool replaced = false;

		/* Loop through all languages and see if we can replace a string */
		for (ptext = &_grf_text[id].textholder; (text = *ptext) != NULL; ptext = &text->next) {
			if (text->langid != langid_to_add) continue;
			newtext->next = text->next;
			*ptext = newtext;
			free(text);
			replaced = true;
			break;
		}

		/* If a string wasn't replaced, then we must append the new string */
		if (!replaced) *ptext = newtext;
	}

	DEBUG(grf, 2)("Added 0x%X: grfid 0x%X string 0x%X lang 0x%X string %s", id, grfid, stringid, newtext->langid, newtext->text);

	return (GRFTAB << TABSIZE) + id;
}

/* Used to remember the grfid that the last retrieved string came from */
static uint32 _last_grfid = 0;

/**
 * Returns the index for this stringid associated with its grfID
 */
StringID GetGRFStringID(uint32 grfid, uint16 stringid)
{
	uint id;

	/* grfid is zero when we're being called via an include */
	if (grfid == 0) grfid = _last_grfid;

	for (id = 0; id < _num_grf_texts; id++) {
		if (_grf_text[id].grfid == grfid && _grf_text[id].stringid == stringid) {
			return (GRFTAB << TABSIZE) + id;
		}
	}

	return STR_UNDEFINED;
}


char *GetGRFString(char *buff, uint16 stringid, const char* last)
{
	const GRFText *default_text = NULL;
	const GRFText *search_text;

	assert(_grf_text[stringid].grfid != 0);

	/* Remember this grfid in case the string has included text */
	_last_grfid = _grf_text[stringid].grfid;

	/*Search the list of lang-strings of this stringid for current lang */
	for (search_text = _grf_text[stringid].textholder; search_text != NULL; search_text = search_text->next) {
		if (search_text->langid == _currentLangID) {
			return strecpy(buff, search_text->text, last);
		}

		/* If the current string is English or American, set it as the
		 * fallback language if the specific language isn't available. */
		if (search_text->langid == GRFLX_UNSPECIFIED || (default_text == NULL && (search_text->langid == GRFLX_ENGLISH || search_text->langid == GRFLX_AMERICAN))) {
			default_text = search_text;
		}
	}

	/* If there is a fallback string, return that */
	if (default_text != NULL) return strecpy(buff, default_text->text, last);

	/* Use the default string ID if the fallback string isn't available */
	return GetString(buff, _grf_text[stringid].def_string, last);
}

/**
 * Equivalence Setter function between game and newgrf langID.
 * This function will adjust _currentLangID as to what is the LangID
 * of the current language set by the user.
 * The array iso_codes will be used to find that match.
 * If not found, it will have to be standard english
 * This function is called after the user changed language,
 * from strings.c:ReadLanguagePack
 * @param iso code of current selection
 */
void SetCurrentGrfLangID(const char *iso_name)
{
	/* Use English by default, if we can't match up the iso_code. */
	byte ret = GRFLX_ENGLISH;
	byte i;

	for (i=0; i < lengthof(iso_codes); i++) {
		if (strncmp(iso_codes[i].code, iso_name, strlen(iso_codes[i].code)) == 0) {
			/* We found a match, so let's use it. */
			ret = i;
			break;
		}
	}
	_currentLangID = ret;
}

/**
 * House cleaning.
 * Remove all strings and reset the text counter.
 */
void CleanUpStrings(void)
{
	uint id;

	for (id = 0; id < _num_grf_texts; id++) {
		GRFText *grftext = _grf_text[id].textholder;
		while (grftext != NULL) {
			GRFText *grftext2 = grftext->next;
			free(grftext);
			grftext = grftext2;
		}
		_grf_text[id].grfid      = 0;
		_grf_text[id].stringid   = 0;
		_grf_text[id].textholder = NULL;
	}

	_num_grf_texts = 0;
}
