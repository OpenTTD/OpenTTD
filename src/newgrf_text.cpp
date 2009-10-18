/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_text.cpp
 * Implementation of  Action 04 "universal holder" structure and functions.
 * This file implements a linked-lists of strings,
 * holding everything that the newgrf action 04 will send over to OpenTTD.
 * One of the biggest problems is that Dynamic lang Array uses ISO codes
 * as way to identifying current user lang, while newgrf uses bit shift codes
 * not related to ISO.  So equivalence functionnality had to be set.
 */

#include "stdafx.h"
#include "newgrf.h"
#include "strings_func.h"
#include "newgrf_storage.h"
#include "string_func.h"
#include "date_type.h"
#include "debug.h"

#include "table/strings.h"
#include "table/control_codes.h"

#define GRFTAB  28
#define TABSIZE 11

/**
 * Perform a mapping from TTDPatch's string IDs to OpenTTD's
 * string IDs, but only for the ones we are aware off; the rest
 * like likely unused and will show a warning.
 * @param str the string ID to convert
 * @return the converted string ID
 */
StringID TTDPStringIDToOTTDStringIDMapping(StringID str)
{
	/* StringID table for TextIDs 0x4E->0x6D */
	static const StringID units_volume[] = {
		STR_NOTHING,    STR_PASSENGERS, STR_TONS,       STR_BAGS,
		STR_LITERS,     STR_ITEMS,      STR_CRATES,     STR_TONS,
		STR_TONS,       STR_TONS,       STR_TONS,       STR_BAGS,
		STR_TONS,       STR_TONS,       STR_TONS,       STR_BAGS,
		STR_TONS,       STR_TONS,       STR_BAGS,       STR_LITERS,
		STR_TONS,       STR_LITERS,     STR_TONS,       STR_NOTHING,
		STR_BAGS,       STR_LITERS,     STR_TONS,       STR_NOTHING,
		STR_TONS,       STR_NOTHING,    STR_LITERS,     STR_NOTHING
	};

	/* A string straight from a NewGRF; no need to remap this as it's already mapped. */
	if (IsInsideMM(str, 0xD000, 0xD7FF) || IsInsideMM(str, 0xDC00, 0xDCFF)) return str;

#define TEXTID_TO_STRINGID(begin, end, stringid) if (str >= begin && str <= end) return str + (stringid - begin)
	/* We have some changes in our cargo strings, resulting in some missing. */
	TEXTID_TO_STRINGID(0x000E, 0x002D, STR_CARGO_PLURAL_NOTHING);
	TEXTID_TO_STRINGID(0x002E, 0x004D, STR_CARGO_SINGULAR_NOTHING);
	if (str >= 0x004E && str <= 0x006D) return units_volume[str - 0x004E];
	TEXTID_TO_STRINGID(0x006E, 0x008D, STR_QUANTITY_NOTHING);
	TEXTID_TO_STRINGID(0x008E, 0x00AD, STR_ABBREV_NOTHING);

	/* Map building names according to our lang file changes. There are several
	 * ranges of house ids, all of which need to be remapped to allow newgrfs
	 * to use original house names. */
	TEXTID_TO_STRINGID(0x200F, 0x201F, STR_TOWN_BUILDING_NAME_TALL_OFFICE_BLOCK_1);
	TEXTID_TO_STRINGID(0x2036, 0x2041, STR_TOWN_BUILDING_NAME_COTTAGES_1);
	TEXTID_TO_STRINGID(0x2059, 0x205C, STR_TOWN_BUILDING_NAME_IGLOO_1);

	/* Same thing for industries */
	TEXTID_TO_STRINGID(0x4802, 0x4826, STR_INDUSTRY_NAME_COAL_MINE);
	TEXTID_TO_STRINGID(0x482D, 0x482E, STR_NEWS_INDUSTRY_CONSTRUCTION);
	TEXTID_TO_STRINGID(0x4832, 0x4834, STR_NEWS_INDUSTRY_CLOSURE_GENERAL);
	TEXTID_TO_STRINGID(0x4835, 0x4838, STR_NEWS_INDUSTRY_PRODUCTION_INCREASE_GENERAL);
	TEXTID_TO_STRINGID(0x4839, 0x483A, STR_NEWS_INDUSTRY_PRODUCTION_DECREASE_GENERAL);

	switch (str) {
		case 0x4830: return STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY;
		case 0x4831: return STR_ERROR_FOREST_CAN_ONLY_BE_PLANTED;
		case 0x483B: return STR_ERROR_CAN_ONLY_BE_POSITIONED;
	}
#undef TEXTID_TO_STRINGID

	if (str == STR_NULL) return STR_EMPTY;

	DEBUG(grf, 0, "Unknown StringID 0x%04X remapped to STR_EMPTY. Please open a Feature Request if you need it", str);

	return STR_EMPTY;
}

/**
 * Explains the newgrf shift bit positionning.
 * the grf base will not be used in order to find the string, but rather for
 * jumping from standard langID scheme to the new one.
 */
enum GRFBaseLanguages {
	GRFLB_AMERICAN    = 0x01,
	GRFLB_ENGLISH     = 0x02,
	GRFLB_GERMAN      = 0x04,
	GRFLB_FRENCH      = 0x08,
	GRFLB_SPANISH     = 0x10,
	GRFLB_GENERIC     = 0x80,
};

enum GRFExtendedLanguages {
	GRFLX_AMERICAN    = 0x00,
	GRFLX_ENGLISH     = 0x01,
	GRFLX_GERMAN      = 0x02,
	GRFLX_FRENCH      = 0x03,
	GRFLX_SPANISH     = 0x04,
	GRFLX_UNSPECIFIED = 0x7F,
};

/**
 * Element of the linked list.
 * Each of those elements represent the string,
 * but according to a different lang.
 */
struct GRFText {
	public:
		static GRFText* New(byte langid, const char* text)
		{
			return new(strlen(text) + 1) GRFText(langid, text);
		}

	private:
		GRFText(byte langid_, const char* text_) : next(NULL), langid(langid_)
		{
			strcpy(text, text_);
		}

		void *operator new(size_t size, size_t extra)
		{
			return ::operator new(size + extra);
		}

public:
		/* dummy operator delete to silence VC8:
		 * 'void *GRFText::operator new(size_t,size_t)' : no matching operator delete found;
		 *     memory will not be freed if initialization throws an exception */
		void operator delete(void *p, size_t extra)
		{
			return ::operator delete(p);
		}

	public:
		GRFText *next;
		byte langid;
		char text[];
};


/**
 * Holder of the above structure.
 * Putting both grfid and stringid together allows us to avoid duplicates,
 * since it is NOT SUPPOSED to happen.
 */
struct GRFTextEntry {
	uint32 grfid;
	uint16 stringid;
	StringID def_string;
	GRFText *textholder;
};


static uint _num_grf_texts = 0;
static GRFTextEntry _grf_text[(1 << TABSIZE) * 3];
static byte _currentLangID = GRFLX_ENGLISH;  ///< by default, english is used.


char *TranslateTTDPatchCodes(uint32 grfid, const char *str)
{
	char *tmp = MallocT<char>(strlen(str) * 10 + 1); // Allocate space to allow for expansion
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
		if (unicode && Utf8EncodedCharLen(*str) != 0) {
			c = Utf8Consume(&str);
			/* 'Magic' range of control codes. */
			if (GB(c, 8, 8) == 0xE0) {
				c = GB(c, 0, 8);
			} else if (c >= 0x20) {
				if (!IsValidChar(c, CS_ALPHANUMERAL)) c = '?';
				d += Utf8Encode(d, c);
				continue;
			}
		} else {
			c = (byte)*str++;
		}
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
			case 0x7E:
			case 0x7F:
			case 0x80: d += Utf8Encode(d, SCC_NEWGRF_PRINT_DWORD + c - 0x7B); break;
			case 0x81: {
				StringID string;
				string  = ((uint8)*str++);
				string |= ((uint8)*str++) << 8;
				d += Utf8Encode(d, SCC_STRING_ID);
				d += Utf8Encode(d, MapGRFStringID(grfid, string));
				break;
			}
			case 0x82:
			case 0x83:
			case 0x84: d += Utf8Encode(d, SCC_NEWGRF_PRINT_WORD_SPEED + c - 0x82); break;
			case 0x85: d += Utf8Encode(d, SCC_NEWGRF_DISCARD_WORD);       break;
			case 0x86: d += Utf8Encode(d, SCC_NEWGRF_ROTATE_TOP_4_WORDS); break;
			case 0x87: d += Utf8Encode(d, SCC_NEWGRF_PRINT_WORD_LITRES);  break;
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
			case 0x9A:
				switch (*str++) {
					case 0: // FALL THROUGH
					case 1:
						d += Utf8Encode(d, SCC_NEWGRF_PRINT_QWORD_CURRENCY);
						break;
					case 3: {
						uint16 tmp  = ((uint8)*str++);
						tmp        |= ((uint8)*str++) << 8;
						d += Utf8Encode(d, SCC_NEWGRF_PUSH_WORD);
						d += Utf8Encode(d, tmp);
					} break;
					case 4:
						d += Utf8Encode(d, SCC_NEWGRF_UNPRINT);
						d += Utf8Encode(d, *str++);
						break;
					case 6:
						d += Utf8Encode(d, SCC_NEWGRF_PRINT_HEX_BYTE);
						break;
					case 7:
						d += Utf8Encode(d, SCC_NEWGRF_PRINT_HEX_WORD);
						break;
					case 8:
						d += Utf8Encode(d, SCC_NEWGRF_PRINT_HEX_DWORD);
						break;

					default:
						grfmsg(1, "missing handler for extended format code");
						break;
				}
				break;

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
			case 0xB9: d += Utf8Encode(d, SCC_SUPERSCRIPT_M1); break;
			case 0xBC: d += Utf8Encode(d, SCC_SMALLUPARROW); break;
			case 0xBD: d += Utf8Encode(d, SCC_SMALLDOWNARROW); break;
			default:
				/* Validate any unhandled character */
				if (!IsValidChar(c, CS_ALPHANUMERAL)) c = '?';
				d += Utf8Encode(d, c);
				break;
		}
	}

	*d = '\0';
	tmp = ReallocT(tmp, strlen(tmp) + 1);
	return tmp;
}


/**
 * Add the new read string into our structure.
 */
StringID AddGRFString(uint32 grfid, uint16 stringid, byte langid_to_add, bool new_scheme, const char *text_to_add, StringID def_string)
{
	char *translatedtext;
	uint id;

	/* When working with the old language scheme (grf_version is less than 7) and
	 * English or American is among the set bits, simply add it as English in
	 * the new scheme, i.e. as langid = 1.
	 * If English is set, it is pretty safe to assume the translations are not
	 * actually translated.
	 */
	if (!new_scheme) {
		if (langid_to_add & (GRFLB_AMERICAN | GRFLB_ENGLISH)) {
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

	translatedtext = TranslateTTDPatchCodes(grfid, text_to_add);

	GRFText *newtext = GRFText::New(langid_to_add, translatedtext);

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
			delete text;
			replaced = true;
			break;
		}

		/* If a string wasn't replaced, then we must append the new string */
		if (!replaced) *ptext = newtext;
	}

	grfmsg(3, "Added 0x%X: grfid %08X string 0x%X lang 0x%X string '%s'", id, grfid, stringid, newtext->langid, newtext->text);

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


const char *GetGRFStringPtr(uint16 stringid)
{
	const GRFText *default_text = NULL;
	const GRFText *search_text;

	assert(_grf_text[stringid].grfid != 0);

	/* Remember this grfid in case the string has included text */
	_last_grfid = _grf_text[stringid].grfid;

	/* Search the list of lang-strings of this stringid for current lang */
	for (search_text = _grf_text[stringid].textholder; search_text != NULL; search_text = search_text->next) {
		if (search_text->langid == _currentLangID) {
			return search_text->text;
		}

		/* If the current string is English or American, set it as the
		 * fallback language if the specific language isn't available. */
		if (search_text->langid == GRFLX_UNSPECIFIED || (default_text == NULL && (search_text->langid == GRFLX_ENGLISH || search_text->langid == GRFLX_AMERICAN))) {
			default_text = search_text;
		}
	}

	/* If there is a fallback string, return that */
	if (default_text != NULL) return default_text->text;

	/* Use the default string ID if the fallback string isn't available */
	return GetStringPtr(_grf_text[stringid].def_string);
}

/**
 * Equivalence Setter function between game and newgrf langID.
 * This function will adjust _currentLangID as to what is the LangID
 * of the current language set by the user.
 * This function is called after the user changed language,
 * from strings.cpp:ReadLanguagePack
 * @param language_id iso code of current selection
 */
void SetCurrentGrfLangID(byte language_id)
{
	_currentLangID = language_id;
}

bool CheckGrfLangID(byte lang_id, byte grf_version)
{
	if (grf_version < 7) {
		switch (_currentLangID) {
			case GRFLX_GERMAN:  return (lang_id & GRFLB_GERMAN)  != 0;
			case GRFLX_FRENCH:  return (lang_id & GRFLB_FRENCH)  != 0;
			case GRFLX_SPANISH: return (lang_id & GRFLB_SPANISH) != 0;
			default:            return (lang_id & (GRFLB_ENGLISH | GRFLB_AMERICAN)) != 0;
		}
	}

	return (lang_id == _currentLangID || lang_id == GRFLX_UNSPECIFIED);
}

/**
 * House cleaning.
 * Remove all strings and reset the text counter.
 */
void CleanUpStrings()
{
	uint id;

	for (id = 0; id < _num_grf_texts; id++) {
		GRFText *grftext = _grf_text[id].textholder;
		while (grftext != NULL) {
			GRFText *grftext2 = grftext->next;
			delete grftext;
			grftext = grftext2;
		}
		_grf_text[id].grfid      = 0;
		_grf_text[id].stringid   = 0;
		_grf_text[id].textholder = NULL;
	}

	_num_grf_texts = 0;
}

struct TextRefStack {
	byte stack[0x30];
	byte position;
	bool used;

	TextRefStack() : used(false) {}

	uint8  PopUnsignedByte()  { assert(this->position < lengthof(this->stack)); return this->stack[this->position++]; }
	int8   PopSignedByte()    { return (int8)this->PopUnsignedByte(); }

	uint16 PopUnsignedWord()
	{
		uint16 val = this->PopUnsignedByte();
		return val | (this->PopUnsignedByte() << 8);
	}
	int16  PopSignedWord()    { return (int32)this->PopUnsignedWord(); }

	uint32 PopUnsignedDWord()
	{
		uint32 val = this->PopUnsignedWord();
		return val | (this->PopUnsignedWord() << 16);
	}
	int32  PopSignedDWord()   { return (int32)this->PopUnsignedDWord(); }

	uint64 PopUnsignedQWord()
	{
		uint64 val = this->PopUnsignedDWord();
		return val | (((uint64)this->PopUnsignedDWord()) << 32);
	}
	int64  PopSignedQWord()   { return (int64)this->PopUnsignedQWord(); }

	/** Rotate the top four words down: W1, W2, W3, W4 -> W4, W1, W2, W3 */
	void RotateTop4Words()
	{
		byte tmp[2];
		for (int i = 0; i  < 2; i++) tmp[i] = this->stack[this->position + i + 6];
		for (int i = 5; i >= 0; i--) this->stack[this->position + i + 2] = this->stack[this->position + i];
		for (int i = 0; i  < 2; i++) this->stack[this->position + i] = tmp[i];
	}

	void PushWord(uint16 word)
	{
		if (this->position >= 2) {
			this->position -= 2;
		} else {
			for (int i = lengthof(stack) - 1; i >= this->position + 2; i--) {
				this->stack[i] = this->stack[i - 2];
			}
		}
		this->stack[this->position]     = GB(word, 0, 8);
		this->stack[this->position + 1] = GB(word, 8, 8);
	}

	void ResetStack()  { this->position = 0; this->used = true; }
	void RewindStack() { this->position = 0; }
};

static TextRefStack _newgrf_normal_textrefstack;
static TextRefStack _newgrf_error_textrefstack;

/** The stack that is used for TTDP compatible string code parsing */
static TextRefStack *_newgrf_textrefstack = &_newgrf_normal_textrefstack;

/**
 * Prepare the TTDP compatible string code parsing
 * @param numEntries number of entries to copy from the registers
 */
void PrepareTextRefStackUsage(byte numEntries)
{
	extern TemporaryStorageArray<uint32, 0x110> _temp_store;

	_newgrf_textrefstack->ResetStack();

	byte *p = _newgrf_textrefstack->stack;
	for (uint i = 0; i < numEntries; i++) {
		for (uint j = 0; j < 32; j += 8) {
			*p = GB(_temp_store.Get(0x100 + i), j, 8);
			p++;
		}
	}
}

/** Stop using the TTDP compatible string code parsing */
void StopTextRefStackUsage() { _newgrf_textrefstack->used = false; }

void SwitchToNormalRefStack()
{
	_newgrf_textrefstack = &_newgrf_normal_textrefstack;
}

void SwitchToErrorRefStack()
{
	_newgrf_textrefstack = &_newgrf_error_textrefstack;
}

void RewindTextRefStack()
{
	_newgrf_textrefstack->RewindStack();
}

/**
 * FormatString for NewGRF specific "magic" string control codes
 * @param scc   the string control code that has been read
 * @param buff  the buffer we're writing to
 * @param str   the string that we need to write
 * @param argv  the OpenTTD stack of values
 * @return the string control code to "execute" now
 */
uint RemapNewGRFStringControlCode(uint scc, char **buff, const char **str, int64 *argv)
{
	if (_newgrf_textrefstack->used) {
		switch (scc) {
			default: NOT_REACHED();
			case SCC_NEWGRF_PRINT_SIGNED_BYTE:    *argv = _newgrf_textrefstack->PopSignedByte();    break;
			case SCC_NEWGRF_PRINT_SIGNED_WORD:    *argv = _newgrf_textrefstack->PopSignedWord();    break;
			case SCC_NEWGRF_PRINT_QWORD_CURRENCY: *argv = _newgrf_textrefstack->PopUnsignedQWord(); break;

			case SCC_NEWGRF_PRINT_DWORD_CURRENCY:
			case SCC_NEWGRF_PRINT_DWORD:          *argv = _newgrf_textrefstack->PopSignedDWord();   break;

			case SCC_NEWGRF_PRINT_HEX_BYTE:       *argv = _newgrf_textrefstack->PopUnsignedByte();  break;
			case SCC_NEWGRF_PRINT_HEX_DWORD:      *argv = _newgrf_textrefstack->PopUnsignedDWord(); break;

			case SCC_NEWGRF_PRINT_HEX_WORD:
			case SCC_NEWGRF_PRINT_WORD_SPEED:
			case SCC_NEWGRF_PRINT_WORD_LITRES:
			case SCC_NEWGRF_PRINT_UNSIGNED_WORD:  *argv = _newgrf_textrefstack->PopUnsignedWord();  break;

			case SCC_NEWGRF_PRINT_DATE:
			case SCC_NEWGRF_PRINT_MONTH_YEAR:     *argv = _newgrf_textrefstack->PopSignedWord() + DAYS_TILL_ORIGINAL_BASE_YEAR; break;

			case SCC_NEWGRF_DISCARD_WORD:         _newgrf_textrefstack->PopUnsignedWord(); break;

			case SCC_NEWGRF_ROTATE_TOP_4_WORDS:   _newgrf_textrefstack->RotateTop4Words(); break;
			case SCC_NEWGRF_PUSH_WORD:            _newgrf_textrefstack->PushWord(Utf8Consume(str)); break;
			case SCC_NEWGRF_UNPRINT:              *buff -= Utf8Consume(str); break;

			case SCC_NEWGRF_PRINT_STRING_ID:
				*argv = TTDPStringIDToOTTDStringIDMapping(_newgrf_textrefstack->PopUnsignedWord());
				break;
		}
	}

	switch (scc) {
		default: NOT_REACHED();
		case SCC_NEWGRF_PRINT_DWORD:
		case SCC_NEWGRF_PRINT_SIGNED_WORD:
		case SCC_NEWGRF_PRINT_SIGNED_BYTE:
		case SCC_NEWGRF_PRINT_UNSIGNED_WORD:
			return SCC_COMMA;

		case SCC_NEWGRF_PRINT_HEX_BYTE:
		case SCC_NEWGRF_PRINT_HEX_WORD:
		case SCC_NEWGRF_PRINT_HEX_DWORD:
			return SCC_HEX;

		case SCC_NEWGRF_PRINT_DWORD_CURRENCY:
		case SCC_NEWGRF_PRINT_QWORD_CURRENCY:
			return SCC_CURRENCY;

		case SCC_NEWGRF_PRINT_STRING_ID:
			return SCC_STRING1;

		case SCC_NEWGRF_PRINT_DATE:
			return SCC_DATE_LONG;

		case SCC_NEWGRF_PRINT_MONTH_YEAR:
			return SCC_DATE_TINY;

		case SCC_NEWGRF_PRINT_WORD_SPEED:
			return SCC_VELOCITY;

		case SCC_NEWGRF_PRINT_WORD_LITRES:
			return SCC_VOLUME;

		case SCC_NEWGRF_DISCARD_WORD:
		case SCC_NEWGRF_ROTATE_TOP_4_WORDS:
		case SCC_NEWGRF_PUSH_WORD:
		case SCC_NEWGRF_UNPRINT:
			return 0;
	}
}
