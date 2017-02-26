/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file newgrf_text.cpp
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
#include "newgrf_text.h"
#include "newgrf_cargo.h"
#include "string_func.h"
#include "date_type.h"
#include "debug.h"
#include "core/alloc_type.hpp"
#include "core/smallmap_type.hpp"
#include "language.h"

#include "table/strings.h"
#include "table/control_codes.h"

#include "safeguards.h"

/**
 * Explains the newgrf shift bit positioning.
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
	/**
	 * Allocate, and assign a new GRFText with the given text.
	 * As these strings can have string terminations in them, e.g.
	 * due to "choice lists" we (sometimes) cannot rely on detecting
	 * the length by means of strlen. Also, if the length of already
	 * known not scanning the whole string is more efficient.
	 * @param langid The language of the text.
	 * @param text   The text to store in the new GRFText.
	 * @param len    The length of the text.
	 */
	static GRFText *New(byte langid, const char *text, size_t len)
	{
		return new (len) GRFText(langid, text, len);
	}

	/**
	 * Create a copy of this GRFText.
	 * @param orig the grftext to copy.
	 * @return an exact copy of the given text.
	 */
	static GRFText *Copy(GRFText *orig)
	{
		return GRFText::New(orig->langid, orig->text, orig->len);
	}

	/**
	 * Helper allocation function to disallow something.
	 * Don't allow simple 'news'; they wouldn't have enough memory.
	 * @param size the amount of space not to allocate.
	 */
	void *operator new(size_t size)
	{
		NOT_REACHED();
	}

	/**
	 * Free the memory we allocated.
	 * @param p memory to free.
	 */
	void operator delete(void *p)
	{
		free(p);
	}
private:
	/**
	 * Actually construct the GRFText.
	 * @param langid_ The language of the text.
	 * @param text_   The text to store in this GRFText.
	 * @param len_    The length of the text to store.
	 */
	GRFText(byte langid_, const char *text_, size_t len_) : next(NULL), len(len_), langid(langid_)
	{
		/* We need to use memcpy instead of strcpy due to
		 * the possibility of "choice lists" and therefore
		 * intermediate string terminators. */
		memcpy(this->text, text_, len);
	}

	/**
	 * Allocate memory for this class.
	 * @param size the size of the instance
	 * @param extra the extra memory for the text
	 * @return the requested amount of memory for both the instance and the text
	 */
	void *operator new(size_t size, size_t extra)
	{
		return MallocT<byte>(size + extra);
	}

public:
	GRFText *next; ///< The next GRFText in this chain.
	size_t len;    ///< The length of the stored string, used for copying.
	byte langid;   ///< The language associated with this GRFText.
	char text[];   ///< The actual (translated) text.
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
static GRFTextEntry _grf_text[TAB_SIZE_NEWGRF];
static byte _currentLangID = GRFLX_ENGLISH;  ///< by default, english is used.

/**
 * Get the mapping from the NewGRF supplied ID to OpenTTD's internal ID.
 * @param newgrf_id The NewGRF ID to map.
 * @param gender    Whether to map genders or cases.
 * @return The, to OpenTTD's internal ID, mapped index, or -1 if there is no mapping.
 */
int LanguageMap::GetMapping(int newgrf_id, bool gender) const
{
	const SmallVector<Mapping, 1> &map = gender ? this->gender_map : this->case_map;
	for (const Mapping *m = map.Begin(); m != map.End(); m++) {
		if (m->newgrf_id == newgrf_id) return m->openttd_id;
	}
	return -1;
}

/**
 * Get the mapping from OpenTTD's internal ID to the NewGRF supplied ID.
 * @param openttd_id The OpenTTD ID to map.
 * @param gender     Whether to map genders or cases.
 * @return The, to the NewGRF supplied ID, mapped index, or -1 if there is no mapping.
 */
int LanguageMap::GetReverseMapping(int openttd_id, bool gender) const
{
	const SmallVector<Mapping, 1> &map = gender ? this->gender_map : this->case_map;
	for (const Mapping *m = map.Begin(); m != map.End(); m++) {
		if (m->openttd_id == openttd_id) return m->newgrf_id;
	}
	return -1;
}

/** Helper structure for mapping choice lists. */
struct UnmappedChoiceList : ZeroedMemoryAllocator {
	/** Clean everything up. */
	~UnmappedChoiceList()
	{
		for (SmallPair<byte, char *> *p = this->strings.Begin(); p < this->strings.End(); p++) {
			free(p->second);
		}
	}

	/**
	 * Initialise the mapping.
	 * @param type   The type of mapping.
	 * @param old_d  The old begin of the string, i.e. from where to start writing again.
	 * @param offset The offset to get the plural/gender from.
	 */
	UnmappedChoiceList(StringControlCode type, char *old_d, int offset) :
		type(type), old_d(old_d), offset(offset)
	{
	}

	StringControlCode type; ///< The type of choice list.
	char *old_d;            ///< The old/original location of the "d" local variable.
	int offset;             ///< The offset for the plural/gender form.

	/** Mapping of NewGRF supplied ID to the different strings in the choice list. */
	SmallMap<byte, char *> strings;

	/**
	 * Flush this choice list into the old d variable.
	 * @param lm  The current language mapping.
	 * @return The new location of the output string.
	 */
	char *Flush(const LanguageMap *lm)
	{
		if (!this->strings.Contains(0)) {
			/* In case of a (broken) NewGRF without a default,
			 * assume an empty string. */
			grfmsg(1, "choice list misses default value");
			this->strings[0] = stredup("");
		}

		char *d = old_d;
		if (lm == NULL) {
			/* In case there is no mapping, just ignore everything but the default.
			 * A probable cause for this happening is when the language file has
			 * been removed by the user and as such no mapping could be made. */
			size_t len = strlen(this->strings[0]);
			memcpy(d, this->strings[0], len);
			return d + len;
		}

		d += Utf8Encode(d, this->type);

		if (this->type == SCC_SWITCH_CASE) {
			/*
			 * Format for case switch:
			 * <NUM CASES> <CASE1> <LEN1> <STRING1> <CASE2> <LEN2> <STRING2> <CASE3> <LEN3> <STRING3> <STRINGDEFAULT>
			 * Each LEN is printed using 2 bytes in big endian order.
			 */

			/* "<NUM CASES>" */
			int count = 0;
			for (uint8 i = 0; i < _current_language->num_cases; i++) {
				/* Count the ones we have a mapped string for. */
				if (this->strings.Contains(lm->GetReverseMapping(i, false))) count++;
			}
			*d++ = count;

			for (uint8 i = 0; i < _current_language->num_cases; i++) {
				/* Resolve the string we're looking for. */
				int idx = lm->GetReverseMapping(i, false);
				if (!this->strings.Contains(idx)) continue;
				char *str = this->strings[idx];

				/* "<CASEn>" */
				*d++ = i + 1;

				/* "<LENn>" */
				size_t len = strlen(str) + 1;
				*d++ = GB(len, 8, 8);
				*d++ = GB(len, 0, 8);

				/* "<STRINGn>" */
				memcpy(d, str, len);
				d += len;
			}

			/* "<STRINGDEFAULT>" */
			size_t len = strlen(this->strings[0]) + 1;
			memcpy(d, this->strings[0], len);
			d += len;
		} else {
			if (this->type == SCC_PLURAL_LIST) {
				*d++ = lm->plural_form;
			}

			/*
			 * Format for choice list:
			 * <OFFSET> <NUM CHOICES> <LENs> <STRINGs>
			 */

			/* "<OFFSET>" */
			*d++ = this->offset - 0x80;

			/* "<NUM CHOICES>" */
			int count = (this->type == SCC_GENDER_LIST ? _current_language->num_genders : LANGUAGE_MAX_PLURAL_FORMS);
			*d++ = count;

			/* "<LENs>" */
			for (int i = 0; i < count; i++) {
				int idx = (this->type == SCC_GENDER_LIST ? lm->GetReverseMapping(i, true) : i + 1);
				const char *str = this->strings[this->strings.Contains(idx) ? idx : 0];
				size_t len = strlen(str) + 1;
				if (len > 0xFF) grfmsg(1, "choice list string is too long");
				*d++ = GB(len, 0, 8);
			}

			/* "<STRINGs>" */
			for (int i = 0; i < count; i++) {
				int idx = (this->type == SCC_GENDER_LIST ? lm->GetReverseMapping(i, true) : i + 1);
				const char *str = this->strings[this->strings.Contains(idx) ? idx : 0];
				/* Limit the length of the string we copy to 0xFE. The length is written above
				 * as a byte and we need room for the final '\0'. */
				size_t len = min<size_t>(0xFE, strlen(str));
				memcpy(d, str, len);
				d += len;
				*d++ = '\0';
			}
		}
		return d;
	}
};

/**
 * Translate TTDPatch string codes into something OpenTTD can handle (better).
 * @param grfid          The (NewGRF) ID associated with this string
 * @param language_id    The (NewGRF) language ID associated with this string.
 * @param allow_newlines Whether newlines are allowed in the string or not.
 * @param str            The string to translate.
 * @param [out] olen     The length of the final string.
 * @param byte80         The control code to use as replacement for the 0x80-value.
 * @return The translated string.
 */
char *TranslateTTDPatchCodes(uint32 grfid, uint8 language_id, bool allow_newlines, const char *str, int *olen, StringControlCode byte80)
{
	char *tmp = MallocT<char>(strlen(str) * 10 + 1); // Allocate space to allow for expansion
	char *d = tmp;
	bool unicode = false;
	WChar c;
	size_t len = Utf8Decode(&c, str);

	/* Helper variable for a possible (string) mapping. */
	UnmappedChoiceList *mapping = NULL;

	if (c == NFO_UTF8_IDENTIFIER) {
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
		if (c == '\0') break;

		switch (c) {
			case 0x01:
				if (str[0] == '\0') goto string_end;
				d += Utf8Encode(d, ' ');
				str++;
				break;
			case 0x0A: break;
			case 0x0D:
				if (allow_newlines) {
					*d++ = 0x0A;
				} else {
					grfmsg(1, "Detected newline in string that does not allow one");
				}
				break;
			case 0x0E: d += Utf8Encode(d, SCC_TINYFONT); break;
			case 0x0F: d += Utf8Encode(d, SCC_BIGFONT); break;
			case 0x1F:
				if (str[0] == '\0' || str[1] == '\0') goto string_end;
				d += Utf8Encode(d, ' ');
				str += 2;
				break;
			case 0x7B:
			case 0x7C:
			case 0x7D:
			case 0x7E:
			case 0x7F: d += Utf8Encode(d, SCC_NEWGRF_PRINT_DWORD_SIGNED + c - 0x7B); break;
			case 0x80: d += Utf8Encode(d, byte80); break;
			case 0x81: {
				if (str[0] == '\0' || str[1] == '\0') goto string_end;
				StringID string;
				string  = ((uint8)*str++);
				string |= ((uint8)*str++) << 8;
				d += Utf8Encode(d, SCC_NEWGRF_STRINL);
				d += Utf8Encode(d, MapGRFStringID(grfid, string));
				break;
			}
			case 0x82:
			case 0x83:
			case 0x84: d += Utf8Encode(d, SCC_NEWGRF_PRINT_WORD_DATE_LONG + c - 0x82); break;
			case 0x85: d += Utf8Encode(d, SCC_NEWGRF_DISCARD_WORD);       break;
			case 0x86: d += Utf8Encode(d, SCC_NEWGRF_ROTATE_TOP_4_WORDS); break;
			case 0x87: d += Utf8Encode(d, SCC_NEWGRF_PRINT_WORD_VOLUME_LONG);  break;
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
			case 0x9A: {
				int code = *str++;
				switch (code) {
					case 0x00: goto string_end;
					case 0x01: d += Utf8Encode(d, SCC_NEWGRF_PRINT_QWORD_CURRENCY); break;
					/* 0x02: ignore next colour byte is not supported. It works on the final
					 * string and as such hooks into the string drawing routine. At that
					 * point many things already happened, such as splitting up of strings
					 * when drawn over multiple lines or right-to-left translations, which
					 * make the behaviour peculiar, e.g. only happening at specific width
					 * of windows. Or we need to add another pass over the string to just
					 * support this. As such it is not implemented in OpenTTD. */
					case 0x03: {
						if (str[0] == '\0' || str[1] == '\0') goto string_end;
						uint16 tmp  = ((uint8)*str++);
						tmp        |= ((uint8)*str++) << 8;
						d += Utf8Encode(d, SCC_NEWGRF_PUSH_WORD);
						d += Utf8Encode(d, tmp);
						break;
					}
					case 0x04:
						if (str[0] == '\0') goto string_end;
						d += Utf8Encode(d, SCC_NEWGRF_UNPRINT);
						d += Utf8Encode(d, *str++);
						break;
					case 0x06: d += Utf8Encode(d, SCC_NEWGRF_PRINT_BYTE_HEX);          break;
					case 0x07: d += Utf8Encode(d, SCC_NEWGRF_PRINT_WORD_HEX);          break;
					case 0x08: d += Utf8Encode(d, SCC_NEWGRF_PRINT_DWORD_HEX);         break;
					/* 0x09, 0x0A are TTDPatch internal use only string codes. */
					case 0x0B: d += Utf8Encode(d, SCC_NEWGRF_PRINT_QWORD_HEX);         break;
					case 0x0C: d += Utf8Encode(d, SCC_NEWGRF_PRINT_WORD_STATION_NAME); break;
					case 0x0D: d += Utf8Encode(d, SCC_NEWGRF_PRINT_WORD_WEIGHT_LONG);  break;
					case 0x0E:
					case 0x0F: {
						if (str[0] == '\0') goto string_end;
						const LanguageMap *lm = LanguageMap::GetLanguageMap(grfid, language_id);
						int index = *str++;
						int mapped = lm != NULL ? lm->GetMapping(index, code == 0x0E) : -1;
						if (mapped >= 0) {
							d += Utf8Encode(d, code == 0x0E ? SCC_GENDER_INDEX : SCC_SET_CASE);
							d += Utf8Encode(d, code == 0x0E ? mapped : mapped + 1);
						}
						break;
					}

					case 0x10:
					case 0x11:
						if (str[0] == '\0') goto string_end;
						if (mapping == NULL) {
							if (code == 0x10) str++; // Skip the index
							grfmsg(1, "choice list %s marker found when not expected", code == 0x10 ? "next" : "default");
							break;
						} else {
							/* Terminate the previous string. */
							*d = '\0';
							int index = (code == 0x10 ? *str++ : 0);
							if (mapping->strings.Contains(index)) {
								grfmsg(1, "duplicate choice list string, ignoring");
								d++;
							} else {
								d = mapping->strings[index] = MallocT<char>(strlen(str) * 10 + 1);
							}
						}
						break;

					case 0x12:
						if (mapping == NULL) {
							grfmsg(1, "choice list end marker found when not expected");
						} else {
							/* Terminate the previous string. */
							*d = '\0';

							/* Now we can start flushing everything and clean everything up. */
							d = mapping->Flush(LanguageMap::GetLanguageMap(grfid, language_id));
							delete mapping;
							mapping = NULL;
						}
						break;

					case 0x13:
					case 0x14:
					case 0x15:
						if (str[0] == '\0') goto string_end;
						if (mapping != NULL) {
							grfmsg(1, "choice lists can't be stacked, it's going to get messy now...");
							if (code != 0x14) str++;
						} else {
							static const StringControlCode mp[] = { SCC_GENDER_LIST, SCC_SWITCH_CASE, SCC_PLURAL_LIST };
							mapping = new UnmappedChoiceList(mp[code - 0x13], d, code == 0x14 ? 0 : *str++);
						}
						break;

					case 0x16:
					case 0x17:
					case 0x18:
					case 0x19:
					case 0x1A:
					case 0x1B:
					case 0x1C:
					case 0x1D:
					case 0x1E:
						d += Utf8Encode(d, SCC_NEWGRF_PRINT_DWORD_DATE_LONG + code - 0x16);
						break;

					default:
						grfmsg(1, "missing handler for extended format code");
						break;
				}
				break;
			}

			case 0x9E: d += Utf8Encode(d, 0x20AC);               break; // Euro
			case 0x9F: d += Utf8Encode(d, 0x0178);               break; // Y with diaeresis
			case 0xA0: d += Utf8Encode(d, SCC_UP_ARROW);         break;
			case 0xAA: d += Utf8Encode(d, SCC_DOWN_ARROW);       break;
			case 0xAC: d += Utf8Encode(d, SCC_CHECKMARK);        break;
			case 0xAD: d += Utf8Encode(d, SCC_CROSS);            break;
			case 0xAF: d += Utf8Encode(d, SCC_RIGHT_ARROW);      break;
			case 0xB4: d += Utf8Encode(d, SCC_TRAIN);            break;
			case 0xB5: d += Utf8Encode(d, SCC_LORRY);            break;
			case 0xB6: d += Utf8Encode(d, SCC_BUS);              break;
			case 0xB7: d += Utf8Encode(d, SCC_PLANE);            break;
			case 0xB8: d += Utf8Encode(d, SCC_SHIP);             break;
			case 0xB9: d += Utf8Encode(d, SCC_SUPERSCRIPT_M1);   break;
			case 0xBC: d += Utf8Encode(d, SCC_SMALL_UP_ARROW);   break;
			case 0xBD: d += Utf8Encode(d, SCC_SMALL_DOWN_ARROW); break;
			default:
				/* Validate any unhandled character */
				if (!IsValidChar(c, CS_ALPHANUMERAL)) c = '?';
				d += Utf8Encode(d, c);
				break;
		}
	}

string_end:
	if (mapping != NULL) {
		grfmsg(1, "choice list was incomplete, the whole list is ignored");
		delete mapping;
	}

	*d = '\0';
	if (olen != NULL) *olen = d - tmp + 1;
	tmp = ReallocT(tmp, d - tmp + 1);
	return tmp;
}

/**
 * Add a GRFText to a GRFText list.
 * @param list The list where the text should be added to.
 * @param text_to_add The GRFText to add to the list.
 */
void AddGRFTextToList(GRFText **list, GRFText *text_to_add)
{
	GRFText **ptext, *text;

	/* Loop through all languages and see if we can replace a string */
	for (ptext = list; (text = *ptext) != NULL; ptext = &text->next) {
		if (text->langid == text_to_add->langid) {
			text_to_add->next = text->next;
			*ptext = text_to_add;
			delete text;
			return;
		}
	}

	/* If a string wasn't replaced, then we must append the new string */
	*ptext = text_to_add;
}

/**
 * Add a string to a GRFText list.
 * @param list The list where the text should be added to.
 * @param langid The language of the new text.
 * @param grfid The grfid where this string is defined.
 * @param allow_newlines Whether newlines are allowed in this string.
 * @param text_to_add The text to add to the list.
 * @note All text-codes will be translated.
 */
void AddGRFTextToList(struct GRFText **list, byte langid, uint32 grfid, bool allow_newlines, const char *text_to_add)
{
	int len;
	char *translatedtext = TranslateTTDPatchCodes(grfid, langid, allow_newlines, text_to_add, &len);
	GRFText *newtext = GRFText::New(langid, translatedtext, len);
	free(translatedtext);

	AddGRFTextToList(list, newtext);
}

/**
 * Add a GRFText to a GRFText list. The text should  not contain any text-codes.
 * The text will be added as a 'default language'-text.
 * @param list The list where the text should be added to.
 * @param text_to_add The text to add to the list.
 */
void AddGRFTextToList(struct GRFText **list, const char *text_to_add)
{
	AddGRFTextToList(list, GRFText::New(0x7F, text_to_add, strlen(text_to_add) + 1));
}

/**
 * Create a copy of this GRFText list.
 * @param orig The GRFText list to copy.
 * @return A duplicate of the given GRFText.
 */
GRFText *DuplicateGRFText(GRFText *orig)
{
	GRFText *newtext = NULL;
	GRFText **ptext = &newtext;
	for (; orig != NULL; orig = orig->next) {
		*ptext = GRFText::Copy(orig);
		ptext = &(*ptext)->next;
	}
	return newtext;
}

/**
 * Add the new read string into our structure.
 */
StringID AddGRFString(uint32 grfid, uint16 stringid, byte langid_to_add, bool new_scheme, bool allow_newlines, const char *text_to_add, StringID def_string)
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
			if (langid_to_add & GRFLB_GERMAN)  ret = AddGRFString(grfid, stringid, GRFLX_GERMAN,  true, allow_newlines, text_to_add, def_string);
			if (langid_to_add & GRFLB_FRENCH)  ret = AddGRFString(grfid, stringid, GRFLX_FRENCH,  true, allow_newlines, text_to_add, def_string);
			if (langid_to_add & GRFLB_SPANISH) ret = AddGRFString(grfid, stringid, GRFLX_SPANISH, true, allow_newlines, text_to_add, def_string);
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

	int len;
	translatedtext = TranslateTTDPatchCodes(grfid, langid_to_add, allow_newlines, text_to_add, &len);

	GRFText *newtext = GRFText::New(langid_to_add, translatedtext, len);

	free(translatedtext);

	/* If we didn't find our stringid and grfid in the list, allocate a new id */
	if (id == _num_grf_texts) _num_grf_texts++;

	if (_grf_text[id].textholder == NULL) {
		_grf_text[id].grfid      = grfid;
		_grf_text[id].stringid   = stringid;
		_grf_text[id].def_string = def_string;
	}
	AddGRFTextToList(&_grf_text[id].textholder, newtext);

	grfmsg(3, "Added 0x%X: grfid %08X string 0x%X lang 0x%X string '%s'", id, grfid, stringid, newtext->langid, newtext->text);

	return MakeStringID(TEXT_TAB_NEWGRF_START, id);
}

/**
 * Returns the index for this stringid associated with its grfID
 */
StringID GetGRFStringID(uint32 grfid, uint16 stringid)
{
	for (uint id = 0; id < _num_grf_texts; id++) {
		if (_grf_text[id].grfid == grfid && _grf_text[id].stringid == stringid) {
			return MakeStringID(TEXT_TAB_NEWGRF_START, id);
		}
	}

	return STR_UNDEFINED;
}


/**
 * Get a C-string from a GRFText-list. If there is a translation for the
 * current language it is returned, otherwise the default translation
 * is returned. If there is neither a default nor a translation for the
 * current language NULL is returned.
 * @param text The GRFText to get the string from.
 */
const char *GetGRFStringFromGRFText(const GRFText *text)
{
	const char *default_text = NULL;

	/* Search the list of lang-strings of this stringid for current lang */
	for (; text != NULL; text = text->next) {
		if (text->langid == _currentLangID) return text->text;

		/* If the current string is English or American, set it as the
		 * fallback language if the specific language isn't available. */
		if (text->langid == GRFLX_UNSPECIFIED || (default_text == NULL && (text->langid == GRFLX_ENGLISH || text->langid == GRFLX_AMERICAN))) {
			default_text = text->text;
		}
	}

	return default_text;
}

/**
 * Get a C-string from a stringid set by a newgrf.
 */
const char *GetGRFStringPtr(uint16 stringid)
{
	assert(_grf_text[stringid].grfid != 0);

	const char *str = GetGRFStringFromGRFText(_grf_text[stringid].textholder);
	if (str != NULL) return str;

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
 * Delete all items of a linked GRFText list.
 * @param grftext the head of the list to delete
 */
void CleanUpGRFText(GRFText *grftext)
{
	while (grftext != NULL) {
		GRFText *grftext2 = grftext->next;
		delete grftext;
		grftext = grftext2;
	}
}

/**
 * House cleaning.
 * Remove all strings and reset the text counter.
 */
void CleanUpStrings()
{
	uint id;

	for (id = 0; id < _num_grf_texts; id++) {
		CleanUpGRFText(_grf_text[id].textholder);
		_grf_text[id].grfid      = 0;
		_grf_text[id].stringid   = 0;
		_grf_text[id].textholder = NULL;
	}

	_num_grf_texts = 0;
}

struct TextRefStack {
	byte stack[0x30];
	byte position;
	const GRFFile *grffile;
	bool used;

	TextRefStack() : position(0), grffile(NULL), used(false) {}

	TextRefStack(const TextRefStack &stack) :
		position(stack.position),
		grffile(stack.grffile),
		used(stack.used)
	{
		memcpy(this->stack, stack.stack, sizeof(this->stack));
	}

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

	void ResetStack(const GRFFile *grffile)
	{
		assert(grffile != NULL);
		this->position = 0;
		this->grffile = grffile;
		this->used = true;
	}

	void RewindStack() { this->position = 0; }
};

/** The stack that is used for TTDP compatible string code parsing */
static TextRefStack _newgrf_textrefstack;

/**
 * Check whether the NewGRF text stack is in use.
 * @return True iff the NewGRF text stack is used.
 */
bool UsingNewGRFTextStack()
{
	return _newgrf_textrefstack.used;
}

/**
 * Create a backup of the current NewGRF text stack.
 * @return A copy of the current text stack.
 */
struct TextRefStack *CreateTextRefStackBackup()
{
	return new TextRefStack(_newgrf_textrefstack);
}

/**
 * Restore a copy of the text stack to the used stack.
 * @param backup The copy to restore.
 */
void RestoreTextRefStackBackup(struct TextRefStack *backup)
{
	_newgrf_textrefstack = *backup;
	delete backup;
}

/**
 * Start using the TTDP compatible string code parsing.
 *
 * On start a number of values is copied on the #TextRefStack.
 * You can then use #GetString() and the normal string drawing functions,
 * and they will use the #TextRefStack for NewGRF string codes.
 *
 * However, when you want to draw a string multiple times using the same stack,
 * you have to call #RewindTextRefStack() between draws.
 *
 * After you are done with drawing, you must disable usage of the #TextRefStack
 * by calling #StopTextRefStackUsage(), so NewGRF string codes operate on the
 * normal string parameters again.
 *
 * @param grffile the NewGRF providing the stack data
 * @param numEntries number of entries to copy from the registers
 * @param values values to copy onto the stack; if NULL the temporary NewGRF registers will be used instead
 */
void StartTextRefStackUsage(const GRFFile *grffile, byte numEntries, const uint32 *values)
{
	extern TemporaryStorageArray<int32, 0x110> _temp_store;

	_newgrf_textrefstack.ResetStack(grffile);

	byte *p = _newgrf_textrefstack.stack;
	for (uint i = 0; i < numEntries; i++) {
		uint32 value = values != NULL ? values[i] : _temp_store.GetValue(0x100 + i);
		for (uint j = 0; j < 32; j += 8) {
			*p = GB(value, j, 8);
			p++;
		}
	}
}

/** Stop using the TTDP compatible string code parsing */
void StopTextRefStackUsage()
{
	_newgrf_textrefstack.used = false;
}

void RewindTextRefStack()
{
	_newgrf_textrefstack.RewindStack();
}

/**
 * FormatString for NewGRF specific "magic" string control codes
 * @param scc   the string control code that has been read
 * @param buff  the buffer we're writing to
 * @param str   the string that we need to write
 * @param argv  the OpenTTD stack of values
 * @param argv_size space on the stack \a argv
 * @param modify_argv When true, modify the OpenTTD stack.
 * @return the string control code to "execute" now
 */
uint RemapNewGRFStringControlCode(uint scc, char *buf_start, char **buff, const char **str, int64 *argv, uint argv_size, bool modify_argv)
{
	switch (scc) {
		default: break;

		case SCC_NEWGRF_PRINT_DWORD_SIGNED:
		case SCC_NEWGRF_PRINT_WORD_SIGNED:
		case SCC_NEWGRF_PRINT_BYTE_SIGNED:
		case SCC_NEWGRF_PRINT_WORD_UNSIGNED:
		case SCC_NEWGRF_PRINT_BYTE_HEX:
		case SCC_NEWGRF_PRINT_WORD_HEX:
		case SCC_NEWGRF_PRINT_DWORD_HEX:
		case SCC_NEWGRF_PRINT_QWORD_HEX:
		case SCC_NEWGRF_PRINT_DWORD_CURRENCY:
		case SCC_NEWGRF_PRINT_QWORD_CURRENCY:
		case SCC_NEWGRF_PRINT_WORD_STRING_ID:
		case SCC_NEWGRF_PRINT_WORD_DATE_LONG:
		case SCC_NEWGRF_PRINT_DWORD_DATE_LONG:
		case SCC_NEWGRF_PRINT_WORD_DATE_SHORT:
		case SCC_NEWGRF_PRINT_DWORD_DATE_SHORT:
		case SCC_NEWGRF_PRINT_WORD_SPEED:
		case SCC_NEWGRF_PRINT_WORD_VOLUME_LONG:
		case SCC_NEWGRF_PRINT_WORD_VOLUME_SHORT:
		case SCC_NEWGRF_PRINT_WORD_WEIGHT_LONG:
		case SCC_NEWGRF_PRINT_WORD_WEIGHT_SHORT:
		case SCC_NEWGRF_PRINT_WORD_POWER:
		case SCC_NEWGRF_PRINT_WORD_STATION_NAME:
		case SCC_NEWGRF_PRINT_WORD_CARGO_NAME:
			if (argv_size < 1) {
				DEBUG(misc, 0, "Too many NewGRF string parameters.");
				return 0;
			}
			break;

		case SCC_NEWGRF_PRINT_WORD_CARGO_LONG:
		case SCC_NEWGRF_PRINT_WORD_CARGO_SHORT:
		case SCC_NEWGRF_PRINT_WORD_CARGO_TINY:
			if (argv_size < 2) {
				DEBUG(misc, 0, "Too many NewGRF string parameters.");
				return 0;
			}
			break;
	}

	if (_newgrf_textrefstack.used && modify_argv) {
		switch (scc) {
			default: NOT_REACHED();
			case SCC_NEWGRF_PRINT_BYTE_SIGNED:      *argv = _newgrf_textrefstack.PopSignedByte();    break;
			case SCC_NEWGRF_PRINT_QWORD_CURRENCY:   *argv = _newgrf_textrefstack.PopSignedQWord();   break;

			case SCC_NEWGRF_PRINT_DWORD_CURRENCY:
			case SCC_NEWGRF_PRINT_DWORD_SIGNED:     *argv = _newgrf_textrefstack.PopSignedDWord();   break;

			case SCC_NEWGRF_PRINT_BYTE_HEX:         *argv = _newgrf_textrefstack.PopUnsignedByte();  break;
			case SCC_NEWGRF_PRINT_QWORD_HEX:        *argv = _newgrf_textrefstack.PopUnsignedQWord(); break;

			case SCC_NEWGRF_PRINT_WORD_SPEED:
			case SCC_NEWGRF_PRINT_WORD_VOLUME_LONG:
			case SCC_NEWGRF_PRINT_WORD_VOLUME_SHORT:
			case SCC_NEWGRF_PRINT_WORD_SIGNED:      *argv = _newgrf_textrefstack.PopSignedWord();    break;

			case SCC_NEWGRF_PRINT_WORD_HEX:
			case SCC_NEWGRF_PRINT_WORD_WEIGHT_LONG:
			case SCC_NEWGRF_PRINT_WORD_WEIGHT_SHORT:
			case SCC_NEWGRF_PRINT_WORD_POWER:
			case SCC_NEWGRF_PRINT_WORD_STATION_NAME:
			case SCC_NEWGRF_PRINT_WORD_UNSIGNED:    *argv = _newgrf_textrefstack.PopUnsignedWord();  break;

			case SCC_NEWGRF_PRINT_DWORD_DATE_LONG:
			case SCC_NEWGRF_PRINT_DWORD_DATE_SHORT:
			case SCC_NEWGRF_PRINT_DWORD_HEX:        *argv = _newgrf_textrefstack.PopUnsignedDWord(); break;

			case SCC_NEWGRF_PRINT_WORD_DATE_LONG:
			case SCC_NEWGRF_PRINT_WORD_DATE_SHORT:  *argv = _newgrf_textrefstack.PopUnsignedWord() + DAYS_TILL_ORIGINAL_BASE_YEAR; break;

			case SCC_NEWGRF_DISCARD_WORD:           _newgrf_textrefstack.PopUnsignedWord(); break;

			case SCC_NEWGRF_ROTATE_TOP_4_WORDS:     _newgrf_textrefstack.RotateTop4Words(); break;
			case SCC_NEWGRF_PUSH_WORD:              _newgrf_textrefstack.PushWord(Utf8Consume(str)); break;
			case SCC_NEWGRF_UNPRINT:                *buff = max(*buff - Utf8Consume(str), buf_start); break;

			case SCC_NEWGRF_PRINT_WORD_CARGO_LONG:
			case SCC_NEWGRF_PRINT_WORD_CARGO_SHORT:
			case SCC_NEWGRF_PRINT_WORD_CARGO_TINY:
				argv[0] = GetCargoTranslation(_newgrf_textrefstack.PopUnsignedWord(), _newgrf_textrefstack.grffile);
				argv[1] = _newgrf_textrefstack.PopUnsignedWord();
				break;

			case SCC_NEWGRF_PRINT_WORD_STRING_ID:
				*argv = MapGRFStringID(_newgrf_textrefstack.grffile->grfid, _newgrf_textrefstack.PopUnsignedWord());
				break;

			case SCC_NEWGRF_PRINT_WORD_CARGO_NAME: {
				CargoID cargo = GetCargoTranslation(_newgrf_textrefstack.PopUnsignedWord(), _newgrf_textrefstack.grffile);
				*argv = cargo < NUM_CARGO ? 1 << cargo : 0;
				break;
			}
		}
	} else {
		/* Consume additional parameter characters */
		switch (scc) {
			default: break;

			case SCC_NEWGRF_PUSH_WORD:
			case SCC_NEWGRF_UNPRINT:
				Utf8Consume(str);
				break;
		}
	}

	switch (scc) {
		default: NOT_REACHED();
		case SCC_NEWGRF_PRINT_DWORD_SIGNED:
		case SCC_NEWGRF_PRINT_WORD_SIGNED:
		case SCC_NEWGRF_PRINT_BYTE_SIGNED:
		case SCC_NEWGRF_PRINT_WORD_UNSIGNED:
			return SCC_COMMA;

		case SCC_NEWGRF_PRINT_BYTE_HEX:
		case SCC_NEWGRF_PRINT_WORD_HEX:
		case SCC_NEWGRF_PRINT_DWORD_HEX:
		case SCC_NEWGRF_PRINT_QWORD_HEX:
			return SCC_HEX;

		case SCC_NEWGRF_PRINT_DWORD_CURRENCY:
		case SCC_NEWGRF_PRINT_QWORD_CURRENCY:
			return SCC_CURRENCY_LONG;

		case SCC_NEWGRF_PRINT_WORD_STRING_ID:
			return SCC_NEWGRF_PRINT_WORD_STRING_ID;

		case SCC_NEWGRF_PRINT_WORD_DATE_LONG:
		case SCC_NEWGRF_PRINT_DWORD_DATE_LONG:
			return SCC_DATE_LONG;

		case SCC_NEWGRF_PRINT_WORD_DATE_SHORT:
		case SCC_NEWGRF_PRINT_DWORD_DATE_SHORT:
			return SCC_DATE_SHORT;

		case SCC_NEWGRF_PRINT_WORD_SPEED:
			return SCC_VELOCITY;

		case SCC_NEWGRF_PRINT_WORD_VOLUME_LONG:
			return SCC_VOLUME_LONG;

		case SCC_NEWGRF_PRINT_WORD_VOLUME_SHORT:
			return SCC_VOLUME_SHORT;

		case SCC_NEWGRF_PRINT_WORD_WEIGHT_LONG:
			return SCC_WEIGHT_LONG;

		case SCC_NEWGRF_PRINT_WORD_WEIGHT_SHORT:
			return SCC_WEIGHT_SHORT;

		case SCC_NEWGRF_PRINT_WORD_POWER:
			return SCC_POWER;

		case SCC_NEWGRF_PRINT_WORD_CARGO_LONG:
			return SCC_CARGO_LONG;

		case SCC_NEWGRF_PRINT_WORD_CARGO_SHORT:
			return SCC_CARGO_SHORT;

		case SCC_NEWGRF_PRINT_WORD_CARGO_TINY:
			return SCC_CARGO_TINY;

		case SCC_NEWGRF_PRINT_WORD_CARGO_NAME:
			return SCC_CARGO_LIST;

		case SCC_NEWGRF_PRINT_WORD_STATION_NAME:
			return SCC_STATION_NAME;

		case SCC_NEWGRF_DISCARD_WORD:
		case SCC_NEWGRF_ROTATE_TOP_4_WORDS:
		case SCC_NEWGRF_PUSH_WORD:
		case SCC_NEWGRF_UNPRINT:
			return 0;
	}
}
