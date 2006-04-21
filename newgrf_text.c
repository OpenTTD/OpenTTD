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
#include "variables.h"
#include "macros.h"
#include "table/strings.h"
#include "newgrf_text.h"

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
	GRFLX_DEUTCH      = 0x1F,
	GRFLX_CATALAN     = 0x22,
	GRFLX_HUNGARIAN   = 0x24,
	GRFLX_ITALIAN     = 0x27,
	GRFLX_ROMANIAN    = 0x28,
	GRFLX_ICELANDIC   = 0x29,
	GRFLX_LATVIAN     = 0x2A,
	GRFLX_LITHUANIAN  = 0x2B,
	GRFLX_SLOVENIAN   = 0x2C,
	GRFLX_DANISH      = 0x2D,
	GRFLX_SEWDISH     = 0x2E,
	GRFLX_NORWEGIAN   = 0x2F,
	GRFLX_POLISH      = 0x30,
	GRFLX_GALICIAN    = 0x31,
	GRFLX_FRISIAN     = 0x32,
	GRFLX_ESTONIAN    = 0x34,
	GRFLX_FINNISH     = 0x35,
	GRFLX_PORTUGUESE  = 0x36,
	GRFLX_BRAZIZILAN  = 0x37,
	GRFLX_TURKISH     = 0x3E,
} grf_language;


typedef struct iso_grf{
	char code[6];
	byte grfLangID;
}iso_grf;

/**
 * ISO code VS NewGrf langID conversion array.
 * This array is used in two ways:
 * 1-its ISO part is matching OpenTTD dynamic language id
 *   with newgrf bit positionning language id
 * 2-its shift part is used to know what is the shift to
 *   watch for when inserting new strings, hence analysing newgrf langid
 */
const iso_grf iso_codes[MAX_LANG] = {
	{"en_US", GRFLX_AMERICAN},
	{"en_GB", GRFLX_ENGLISH},
	{"de",    GRFLX_GERMAN},
	{"fr",    GRFLX_FRENCH},
	{"es",    GRFLX_SPANISH},
	{"cs",    GRFLX_CZECH},
	{"ca",    GRFLX_CATALAN},
	{"da",    GRFLX_DANISH},
	{"nl",    GRFLX_DEUTCH},
	{"et",    GRFLX_ESTONIAN},
	{"fi",    GRFLX_FINNISH},
	{"fy",    GRFLX_FRISIAN},
	{"gl",    GRFLX_GALICIAN},
	{"hu",    GRFLX_HUNGARIAN},
	{"is",    GRFLX_ICELANDIC},
	{"it",    GRFLX_ITALIAN},
	{"lv",    GRFLX_LATVIAN},
	{"lt",    GRFLX_LITHUANIAN},
	{"nb",    GRFLX_NORWEGIAN},
	{"pl",    GRFLX_POLISH},
	{"pt",    GRFLX_PORTUGUESE},
	{"ro",    GRFLX_ROMANIAN},
	{"ru",    GRFLX_RUSSIAN},
	{"sk",    GRFLX_SLOVAK},
	{"sl",    GRFLX_SLOVENIAN},
	{"sv",    GRFLX_SEWDISH},
	{"tr",    GRFLX_TURKISH},
	{"gen",   GRFLB_GENERIC}   //this is not iso code, but there has to be something...
};


static uint _num_grf_texts = 0;
static GRFTextEntry _grf_text[(1 << TABSIZE) * 3];
static byte _currentLangID = GRFLX_ENGLISH;  //by default, english is used.


/**
 * Add the new read stirng into our structure.
 * TODO : ajust the old scheme to the new one for german,french and spanish
 */
StringID AddGRFString(uint32 grfid, uint16 stringid, byte langid_to_add, const char *text_to_add)
{
	uint id;

	GRFText *newtext = calloc(1, sizeof(*newtext));

	newtext->langid = langid_to_add;
	newtext->text   = strdup(text_to_add);
	newtext->next   = NULL;

	str_validate(newtext->text);

	for (id = 0; id < _num_grf_texts; id++) {
		if (_grf_text[id].grfid == grfid && _grf_text[id].stringid == stringid) {
			break;
		}
	}

	/* Too many strings allocated, return empty */
	if (id == lengthof(_grf_text)) return STR_EMPTY;

	/* Raise the number of strings added*/
	if (id == _num_grf_texts) _num_grf_texts++;

	/* When working with old scheme (BIT 6 of langid is clear) and
	 * English or american is among the set bits, simply add it as
	 * english on new scheme, as langid = 1.
	 * If there are more langid, we simply don't need them
	 */
	if (!HASBIT(6,langid_to_add)) {
		if (HASBITS( GRFLB_AMERICAN | GRFLB_ENGLISH, langid_to_add)) {
			newtext->langid = langid_to_add = GRFLX_ENGLISH;
		} else {
			/* If old scheme and not english nor american, scanning will
			 * have to be done. At this stage, only 3 are remaining:
			 * german,french and spanish : 0x04=2,0x08=3,0x10=4
			 */
		}
	}

	if (_grf_text[id].textholder == NULL) {
		_grf_text[id].grfid      = grfid;
		_grf_text[id].stringid   = stringid;
		_grf_text[id].textholder = newtext;
	} else {
		GRFText *textptr = _grf_text[id].textholder;
		while (textptr->next != NULL) textptr = textptr->next;
		textptr->next = newtext;
	}

	debug("Added %x: grfid %x string %x lang %x string %s", id, grfid, stringid, langid_to_add, newtext->text);

	return (GRFTAB << TABSIZE) + id;
}


/**
 * Returns the index for this stringid associated with its grfID
 */
StringID GetGRFStringID(uint32 grfid, uint16 stringid)
{
	uint id;
	for (id = 0; id < _num_grf_texts; id++) {
		if (_grf_text[id].grfid == grfid && _grf_text[id].stringid == stringid) {
			return (GRFTAB << TABSIZE) + id;
		}
	}

	return STR_UNDEFINED;
}


char *GetGRFString(char *buff, uint16 stringid)
{
	GRFText *search_text;

	assert(_grf_text[stringid].grfid != 0);
	/*Search the list of lang-strings of this stringid for current lang */
	for (search_text = _grf_text[stringid].textholder; search_text != NULL; search_text = search_text->next) {
		if (search_text->langid == _currentLangID){
			return strecpy(buff, search_text->text, NULL);
		}
	}

	/* Use the first text if the specific language isn't available */
	return strecpy(buff, _grf_text[stringid].textholder->text, NULL);
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
void SetCurrentGrfLangID( const char *iso_name )
{
	byte ret,i;

	ret = GRFLX_ENGLISH;  //by default, english (not american) will be used

	for (i=0; i < MAX_LANG+1; i++) {
		if (strcmp(iso_codes[i].code, iso_name)==0){  //we have a match?
			ret = i;                                  //we'll use this one then.
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
			free(grftext->text);
			free(grftext);
			grftext = grftext2;
		}
		_grf_text[id].grfid      = 0;
		_grf_text[id].stringid   = 0;
		_grf_text[id].textholder = NULL;
	}

	_num_grf_texts = 0;
}
