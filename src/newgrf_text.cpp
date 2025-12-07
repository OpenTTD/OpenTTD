/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file newgrf_text.cpp
 * Implementation of  Action 04 "universal holder" structure and functions.
 * This file implements a linked-lists of strings,
 * holding everything that the newgrf action 04 will send over to OpenTTD.
 * One of the biggest problems is that Dynamic lang Array uses ISO codes
 * as way to identifying current user lang, while newgrf uses bit shift codes
 * not related to ISO.  So equivalence functionality had to be set.
 */

#include "stdafx.h"

#include "debug.h"
#include "newgrf.h"
#include "strings_internal.h"
#include "newgrf_storage.h"
#include "newgrf_text.h"
#include "newgrf_cargo.h"
#include "string_func.h"
#include "timer/timer_game_calendar.h"
#include "debug.h"
#include "core/string_builder.hpp"
#include "core/string_consumer.hpp"
#include "language.h"
#include <ranges>

#include "table/strings.h"
#include "table/control_codes.h"

#include "safeguards.h"

/**
 * Explains the newgrf shift bit positioning.
 * the grf base will not be used in order to find the string, but rather for
 * jumping from standard langID scheme to the new one.
 */
enum GRFBaseLanguages : uint8_t {
	GRFLB_AMERICAN    = 0x01,
	GRFLB_ENGLISH     = 0x02,
	GRFLB_GERMAN      = 0x04,
	GRFLB_FRENCH      = 0x08,
	GRFLB_SPANISH     = 0x10,
	GRFLB_GENERIC     = 0x80,
};

enum GRFExtendedLanguages : uint8_t {
	GRFLX_AMERICAN    = 0x00,
	GRFLX_ENGLISH     = 0x01,
	GRFLX_GERMAN      = 0x02,
	GRFLX_FRENCH      = 0x03,
	GRFLX_SPANISH     = 0x04,
	GRFLX_UNSPECIFIED = 0x7F,
};


/**
 * Holder of the above structure.
 * Putting both grfid and stringid together allows us to avoid duplicates,
 * since it is NOT SUPPOSED to happen.
 */
struct GRFTextEntry {
	GRFTextList textholder;
	StringID def_string;
	uint32_t grfid;
	GRFStringID stringid;
};


static TypedIndexContainer<std::vector<GRFTextEntry>, StringIndexInTab> _grf_text;
static uint8_t _current_lang_id = GRFLX_ENGLISH;  ///< by default, english is used.

/**
 * Get the mapping from the NewGRF supplied ID to OpenTTD's internal ID.
 * @param newgrf_id The NewGRF ID to map.
 * @param gender    Whether to map genders or cases.
 * @return The, to OpenTTD's internal ID, mapped index, or -1 if there is no mapping.
 */
int LanguageMap::GetMapping(int newgrf_id, bool gender) const
{
	const std::vector<Mapping> &map = gender ? this->gender_map : this->case_map;
	for (const Mapping &m : map) {
		if (m.newgrf_id == newgrf_id) return m.openttd_id;
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
	const std::vector<Mapping> &map = gender ? this->gender_map : this->case_map;
	for (const Mapping &m : map) {
		if (m.openttd_id == openttd_id) return m.newgrf_id;
	}
	return -1;
}

/** Helper structure for mapping choice lists. */
struct UnmappedChoiceList {
	/**
	 * Initialise the mapping.
	 * @param type   The type of mapping.
	 * @param offset The offset to get the plural/gender from.
	 */
	UnmappedChoiceList(StringControlCode type, int offset) :
		type(type), offset(offset)
	{
	}

	StringControlCode type; ///< The type of choice list.
	int offset;             ///< The offset for the plural/gender form.

	/** Mapping of NewGRF supplied ID to the different strings in the choice list. */
	std::map<int, std::string> strings;

	/**
	 * Flush this choice list into the destination string.
	 * @param lm The current language mapping.
	 * @param dest Target to write to.
	 */
	void Flush(const LanguageMap *lm, std::string &dest)
	{
		if (this->strings.find(0) == this->strings.end()) {
			/* In case of a (broken) NewGRF without a default,
			 * assume an empty string. */
			GrfMsg(1, "choice list misses default value");
			this->strings[0] = std::string();
		}

		StringBuilder builder(dest);

		if (lm == nullptr) {
			/* In case there is no mapping, just ignore everything but the default.
			 * A probable cause for this happening is when the language file has
			 * been removed by the user and as such no mapping could be made. */
			builder += this->strings[0];
			return;
		}

		builder.PutUtf8(this->type);

		if (this->type == SCC_SWITCH_CASE) {
			/*
			 * Format for case switch:
			 * <NUM CASES> <CASE1> <LEN1> <STRING1> <CASE2> <LEN2> <STRING2> <CASE3> <LEN3> <STRING3> <LENDEFAULT> <STRINGDEFAULT>
			 * Each LEN is printed using 2 bytes in big endian order.
			 */

			/* "<NUM CASES>" */
			int count = 0;
			for (uint8_t i = 0; i < _current_language->num_cases; i++) {
				/* Count the ones we have a mapped string for. */
				if (this->strings.find(lm->GetReverseMapping(i, false)) != this->strings.end()) count++;
			}
			builder.PutUint8(count);

			auto add_case = [&](std::string_view str) {
				/* "<LENn>" */
				uint16_t len = ClampTo<uint16_t>(str.size());
				builder.PutUint16LE(len);

				/* "<STRINGn>" */
				builder += str.substr(0, len);
			};

			for (uint8_t i = 0; i < _current_language->num_cases; i++) {
				/* Resolve the string we're looking for. */
				int idx = lm->GetReverseMapping(i, false);
				if (this->strings.find(idx) == this->strings.end()) continue;
				auto &str = this->strings[idx];

				/* "<CASEn>" */
				builder.PutUint8(i + 1);

				add_case(str);
			}

			/* "<STRINGDEFAULT>" */
			add_case(this->strings[0]);
		} else {
			if (this->type == SCC_PLURAL_LIST) {
				builder.PutUint8(lm->plural_form);
			}

			/*
			 * Format for choice list:
			 * <OFFSET> <NUM CHOICES> <LENs> <STRINGs>
			 */

			/* "<OFFSET>" */
			builder.PutUint8(this->offset - 0x80);

			/* "<NUM CHOICES>" */
			int count = (this->type == SCC_GENDER_LIST ? _current_language->num_genders : LANGUAGE_MAX_PLURAL_FORMS);
			builder.PutUint8(count);

			/* "<LENs>" */
			for (int i = 0; i < count; i++) {
				int idx = (this->type == SCC_GENDER_LIST ? lm->GetReverseMapping(i, true) : i + 1);
				const auto &str = this->strings[this->strings.find(idx) != this->strings.end() ? idx : 0];
				size_t len = str.size();
				if (len > UINT8_MAX) GrfMsg(1, "choice list string is too long");
				builder.PutUint8(ClampTo<uint8_t>(len));
			}

			/* "<STRINGs>" */
			for (int i = 0; i < count; i++) {
				int idx = (this->type == SCC_GENDER_LIST ? lm->GetReverseMapping(i, true) : i + 1);
				const auto &str = this->strings[this->strings.find(idx) != this->strings.end() ? idx : 0];
				uint8_t len = ClampTo<uint8_t>(str.size());
				builder += str.substr(0, len);
			}
		}
	}
};

/**
 * Translate TTDPatch string codes into something OpenTTD can handle (better).
 * @param grfid          The (NewGRF) ID associated with this string
 * @param language_id    The (NewGRF) language ID associated with this string.
 * @param allow_newlines Whether newlines are allowed in the string or not.
 * @param str            The string to translate.
 * @param byte80         The control code to use as replacement for the 0x80-value.
 * @return The translated string.
 */
std::string TranslateTTDPatchCodes(uint32_t grfid, uint8_t language_id, bool allow_newlines, std::string_view str, StringControlCode byte80)
{
	/* Empty input string? Nothing to do here. */
	if (str.empty()) return {};

	StringConsumer consumer(str);

	/* Is this an unicode string? */
	bool unicode = consumer.ReadUtf8If(NFO_UTF8_IDENTIFIER);

	/* Helper variable for a possible (string) mapping of plural/gender and cases. */
	std::optional<UnmappedChoiceList> mapping_pg, mapping_c;
	std::optional<std::reference_wrapper<std::string>> dest_c;

	std::string dest;
	StringBuilder builder(dest);
	while (consumer.AnyBytesLeft()) {
		char32_t c;
		if (auto u = unicode ? consumer.TryReadUtf8() : std::nullopt; u.has_value()) {
			c = *u;
			/* 'Magic' range of control codes. */
			if (0xE000 <= c && c <= 0xE0FF) {
				c -= 0xE000;
			} else if (c >= 0x20) {
				if (!IsValidChar(c, CS_ALPHANUMERAL)) c = '?';
				builder.PutUtf8(c);
				continue;
			}
		} else {
			c = consumer.ReadUint8(); // read as unsigned, otherwise integer promotion breaks it
		}
		assert(c <= 0xFF);
		if (c == '\0') break;

		switch (c) {
			case 0x01:
				consumer.SkipUint8();
				builder.PutChar(' ');
				break;
			case 0x0A: break;
			case 0x0D:
				if (allow_newlines) {
					builder.PutChar(0x0A);
				} else {
					GrfMsg(1, "Detected newline in string that does not allow one");
				}
				break;
			case 0x0E: builder.PutUtf8(SCC_TINYFONT); break;
			case 0x0F: builder.PutUtf8(SCC_BIGFONT); break;
			case 0x1F:
				consumer.SkipUint8();
				consumer.SkipUint8();
				builder.PutChar(' ');
				break;
			case 0x7B:
			case 0x7C:
			case 0x7D:
			case 0x7E:
			case 0x7F: builder.PutUtf8(SCC_NEWGRF_PRINT_DWORD_SIGNED + c - 0x7B); break;
			case 0x80: builder.PutUtf8(byte80); break;
			case 0x81:
			{
				uint16_t string = consumer.ReadUint16LE();
				builder.PutUtf8(SCC_NEWGRF_STRINL);
				builder.PutUtf8(MapGRFStringID(grfid, GRFStringID{string}));
				break;
			}
			case 0x82:
			case 0x83:
			case 0x84: builder.PutUtf8(SCC_NEWGRF_PRINT_WORD_DATE_LONG + c - 0x82); break;
			case 0x85: builder.PutUtf8(SCC_NEWGRF_DISCARD_WORD);       break;
			case 0x86: builder.PutUtf8(SCC_NEWGRF_ROTATE_TOP_4_WORDS); break;
			case 0x87: builder.PutUtf8(SCC_NEWGRF_PRINT_WORD_VOLUME_LONG);  break;
			case 0x88: builder.PutUtf8(SCC_BLUE);    break;
			case 0x89: builder.PutUtf8(SCC_SILVER);  break;
			case 0x8A: builder.PutUtf8(SCC_GOLD);    break;
			case 0x8B: builder.PutUtf8(SCC_RED);     break;
			case 0x8C: builder.PutUtf8(SCC_PURPLE);  break;
			case 0x8D: builder.PutUtf8(SCC_LTBROWN); break;
			case 0x8E: builder.PutUtf8(SCC_ORANGE);  break;
			case 0x8F: builder.PutUtf8(SCC_GREEN);   break;
			case 0x90: builder.PutUtf8(SCC_YELLOW);  break;
			case 0x91: builder.PutUtf8(SCC_DKGREEN); break;
			case 0x92: builder.PutUtf8(SCC_CREAM);   break;
			case 0x93: builder.PutUtf8(SCC_BROWN);   break;
			case 0x94: builder.PutUtf8(SCC_WHITE);   break;
			case 0x95: builder.PutUtf8(SCC_LTBLUE);  break;
			case 0x96: builder.PutUtf8(SCC_GRAY);    break;
			case 0x97: builder.PutUtf8(SCC_DKBLUE);  break;
			case 0x98: builder.PutUtf8(SCC_BLACK);   break;
			case 0x9A:
			{
				uint8_t code = consumer.ReadUint8();
				switch (code) {
					case 0x00: goto string_end;
					case 0x01: builder.PutUtf8(SCC_NEWGRF_PRINT_QWORD_CURRENCY); break;
						/* 0x02: ignore next colour byte is not supported. It works on the final
						 * string and as such hooks into the string drawing routine. At that
						 * point many things already happened, such as splitting up of strings
						 * when drawn over multiple lines or right-to-left translations, which
						 * make the behaviour peculiar, e.g. only happening at specific width
						 * of windows. Or we need to add another pass over the string to just
						 * support this. As such it is not implemented in OpenTTD. */
					case 0x03:
					{
						uint16_t tmp = consumer.ReadUint16LE();
						builder.PutUtf8(SCC_NEWGRF_PUSH_WORD);
						builder.PutUtf8(tmp);
						break;
					}
					case 0x06: builder.PutUtf8(SCC_NEWGRF_PRINT_BYTE_HEX);          break;
					case 0x07: builder.PutUtf8(SCC_NEWGRF_PRINT_WORD_HEX);          break;
					case 0x08: builder.PutUtf8(SCC_NEWGRF_PRINT_DWORD_HEX);         break;
					/* 0x09, 0x0A are TTDPatch internal use only string codes. */
					case 0x0B: builder.PutUtf8(SCC_NEWGRF_PRINT_QWORD_HEX);         break;
					case 0x0C: builder.PutUtf8(SCC_NEWGRF_PRINT_WORD_STATION_NAME); break;
					case 0x0D: builder.PutUtf8(SCC_NEWGRF_PRINT_WORD_WEIGHT_LONG);  break;
					case 0x0E:
					case 0x0F:
					{
						const LanguageMap *lm = LanguageMap::GetLanguageMap(grfid, language_id);
						int index = consumer.ReadUint8();
						int mapped = lm != nullptr ? lm->GetMapping(index, code == 0x0E) : -1;
						if (mapped >= 0) {
							builder.PutUtf8(code == 0x0E ? SCC_GENDER_INDEX : SCC_SET_CASE);
							builder.PutUtf8(code == 0x0E ? mapped : mapped + 1);
						}
						break;
					}

					case 0x10:
					case 0x11:
						if (!mapping_pg.has_value() && !mapping_c.has_value()) {
							if (code == 0x10) consumer.SkipUint8(); // Skip the index
							GrfMsg(1, "choice list {} marker found when not expected", code == 0x10 ? "next" : "default");
							break;
						} else {
							auto &mapping = mapping_pg ? mapping_pg : mapping_c;
							int index = (code == 0x10 ? consumer.ReadUint8() : 0);
							if (mapping->strings.find(index) != mapping->strings.end()) {
								GrfMsg(1, "duplicate choice list string, ignoring");
							} else {
								builder = StringBuilder(mapping->strings[index]);
								if (!mapping_pg) dest_c = mapping->strings[index];
							}
						}
						break;

					case 0x12:
						if (!mapping_pg.has_value() && !mapping_c.has_value()) {
							GrfMsg(1, "choice list end marker found when not expected");
						} else {
							auto &mapping = mapping_pg ? mapping_pg : mapping_c;
							auto &new_dest = mapping_pg && dest_c ? dest_c->get() : dest;
							/* Now we can start flushing everything and clean everything up. */
							mapping->Flush(LanguageMap::GetLanguageMap(grfid, language_id), new_dest);
							if (!mapping_pg) dest_c.reset();
							mapping.reset();

							builder = StringBuilder(new_dest);
						}
						break;

					case 0x13:
					case 0x14:
					case 0x15: {
						auto &mapping = code == 0x14 ? mapping_c : mapping_pg;
						/* Case mapping can have nested plural/gender mapping. Otherwise nesting is invalid. */
						if (mapping.has_value() || mapping_pg.has_value()) {
							GrfMsg(1, "choice lists can't be stacked, it's going to get messy now...");
							if (code != 0x14) consumer.SkipUint8();
						} else {
							static const StringControlCode mp[] = { SCC_GENDER_LIST, SCC_SWITCH_CASE, SCC_PLURAL_LIST };
							mapping.emplace(mp[code - 0x13], code == 0x14 ? 0 : consumer.ReadUint8());
						}
						break;
					}

					case 0x16:
					case 0x17:
					case 0x18:
					case 0x19:
					case 0x1A:
					case 0x1B:
					case 0x1C:
					case 0x1D:
					case 0x1E:
						builder.PutUtf8(SCC_NEWGRF_PRINT_DWORD_DATE_LONG + code - 0x16);
						break;

					case 0x1F: builder.PutUtf8(SCC_PUSH_COLOUR); break;
					case 0x20: builder.PutUtf8(SCC_POP_COLOUR);  break;

					case 0x21: builder.PutUtf8(SCC_NEWGRF_PRINT_DWORD_FORCE); break;

					default:
						GrfMsg(1, "missing handler for extended format code");
						break;
				}
				break;
			}

			case 0x9B: builder.PutUtf8(SCC_TOWN);             break;
			case 0x9C: builder.PutUtf8(SCC_CITY);             break;
			case 0x9E: builder.PutUtf8(0x20AC);               break; // Euro
			case 0x9F: builder.PutUtf8(0x0178);               break; // Y with diaeresis
			case 0xA0: builder.PutUtf8(SCC_UP_ARROW);         break;
			case 0xAA: builder.PutUtf8(SCC_DOWN_ARROW);       break;
			case 0xAC: builder.PutUtf8(SCC_CHECKMARK);        break;
			case 0xAD: builder.PutUtf8(SCC_CROSS);            break;
			case 0xAF: builder.PutUtf8(SCC_RIGHT_ARROW);      break;
			case 0xB4: builder.PutUtf8(SCC_TRAIN);            break;
			case 0xB5: builder.PutUtf8(SCC_LORRY);            break;
			case 0xB6: builder.PutUtf8(SCC_BUS);              break;
			case 0xB7: builder.PutUtf8(SCC_PLANE);            break;
			case 0xB8: builder.PutUtf8(SCC_SHIP);             break;
			case 0xB9: builder.PutUtf8(SCC_SUPERSCRIPT_M1);   break;
			case 0xBC: builder.PutUtf8(SCC_SMALL_UP_ARROW);   break;
			case 0xBD: builder.PutUtf8(SCC_SMALL_DOWN_ARROW); break;
			default:
				/* Validate any unhandled character */
				if (!IsValidChar(c, CS_ALPHANUMERAL)) c = '?';
				builder.PutUtf8(c);
				break;
		}
	}

string_end:
	if (mapping_pg.has_value() || mapping_c.has_value()) {
		GrfMsg(1, "choice list was incomplete, the whole list is ignored");
	}

	return dest;
}

/**
 * Add a new text to a GRFText list.
 * @param list The list where the text should be added to.
 * @param langid The The language of the new text.
 * @param text_to_add The text to add to the list.
 */
static void AddGRFTextToList(GRFTextList &list, uint8_t langid, std::string_view text_to_add)
{
	/* Loop through all languages and see if we can replace a string */
	for (auto &text : list) {
		if (text.langid == langid) {
			text.text = text_to_add;
			return;
		}
	}

	/* If a string wasn't replaced, then we must append the new string */
	list.emplace_back(langid, std::string{text_to_add});
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
void AddGRFTextToList(GRFTextList &list, uint8_t langid, uint32_t grfid, bool allow_newlines, std::string_view text_to_add)
{
	AddGRFTextToList(list, langid, TranslateTTDPatchCodes(grfid, langid, allow_newlines, text_to_add));
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
void AddGRFTextToList(GRFTextWrapper &list, uint8_t langid, uint32_t grfid, bool allow_newlines, std::string_view text_to_add)
{
	if (list == nullptr) list = std::make_shared<GRFTextList>();
	AddGRFTextToList(*list, langid, grfid, allow_newlines, text_to_add);
}

/**
 * Add a GRFText to a GRFText list. The text should  not contain any text-codes.
 * The text will be added as a 'default language'-text.
 * @param list The list where the text should be added to.
 * @param text_to_add The text to add to the list.
 */
void AddGRFTextToList(GRFTextWrapper &list, std::string_view text_to_add)
{
	if (list == nullptr) list = std::make_shared<GRFTextList>();
	AddGRFTextToList(*list, GRFLX_UNSPECIFIED, text_to_add);
}

/**
 * Add the new read string into our structure.
 */
StringID AddGRFString(uint32_t grfid, GRFStringID stringid, uint8_t langid_to_add, bool new_scheme, bool allow_newlines, std::string_view text_to_add, StringID def_string)
{
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

	auto it = std::ranges::find_if(_grf_text, [&grfid, &stringid](const GRFTextEntry &grf_text) { return grf_text.grfid == grfid && grf_text.stringid == stringid; });
	if (it == std::end(_grf_text)) {
		/* Too many strings allocated, return empty. */
		if (_grf_text.size() == TAB_SIZE_NEWGRF) return STR_EMPTY;

		/* We didn't find our stringid and grfid in the list, allocate a new id. */
		it = _grf_text.emplace(std::end(_grf_text));
		it->grfid      = grfid;
		it->stringid   = stringid;
		it->def_string = def_string;
	}
	StringIndexInTab id(it - std::begin(_grf_text));

	std::string newtext = TranslateTTDPatchCodes(grfid, langid_to_add, allow_newlines, text_to_add);
	AddGRFTextToList(it->textholder, langid_to_add, newtext);

	GrfMsg(3, "Added 0x{:X} grfid {:08X} string 0x{:X} lang 0x{:X} string '{}' ({:X})", id, grfid, stringid, langid_to_add, newtext, MakeStringID(TEXT_TAB_NEWGRF_START, id));

	return MakeStringID(TEXT_TAB_NEWGRF_START, id);
}

/**
 * Returns the index for this stringid associated with its grfID
 */
StringID GetGRFStringID(uint32_t grfid, GRFStringID stringid)
{
	auto it = std::ranges::find_if(_grf_text, [&grfid, &stringid](const GRFTextEntry &grf_text) { return grf_text.grfid == grfid && grf_text.stringid == stringid; });
	if (it != std::end(_grf_text)) {
		StringIndexInTab id(it - std::begin(_grf_text));
		return MakeStringID(TEXT_TAB_NEWGRF_START, id);
	}

	return STR_UNDEFINED;
}


/**
 * Get a C-string from a GRFText-list. If there is a translation for the
 * current language it is returned, otherwise the default translation
 * is returned. If there is neither a default nor a translation for the
 * current language nullptr is returned.
 * @param text_list The GRFTextList to get the string from.
 */
std::optional<std::string_view> GetGRFStringFromGRFText(const GRFTextList &text_list)
{
	std::optional<std::string_view> default_text;

	/* Search the list of lang-strings of this stringid for current lang */
	for (const auto &text : text_list) {
		if (text.langid == _current_lang_id) return text.text;

		/* If the current string is English or American, set it as the
		 * fallback language if the specific language isn't available. */
		if (text.langid == GRFLX_UNSPECIFIED || (!default_text.has_value() && (text.langid == GRFLX_ENGLISH || text.langid == GRFLX_AMERICAN))) {
			default_text = text.text;
		}
	}

	return default_text;
}

/**
 * Get a C-string from a GRFText-list. If there is a translation for the
 * current language it is returned, otherwise the default translation
 * is returned. If there is neither a default nor a translation for the
 * current language nullptr is returned.
 * @param text The GRFTextList to get the string from.
 */
std::optional<std::string_view> GetGRFStringFromGRFText(const GRFTextWrapper &text)
{
	return text ? GetGRFStringFromGRFText(*text) : std::nullopt;
}

/**
 * Get a C-string from a stringid set by a newgrf.
 */
std::string_view GetGRFStringPtr(StringIndexInTab stringid)
{
	assert(stringid.base() < _grf_text.size());
	assert(_grf_text[stringid].grfid != 0);

	auto str = GetGRFStringFromGRFText(_grf_text[stringid].textholder);
	if (str.has_value()) return *str;

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
void SetCurrentGrfLangID(uint8_t language_id)
{
	_current_lang_id = language_id;
}

bool CheckGrfLangID(uint8_t lang_id, uint8_t grf_version)
{
	if (grf_version < 7) {
		switch (_current_lang_id) {
			case GRFLX_GERMAN:  return (lang_id & GRFLB_GERMAN)  != 0;
			case GRFLX_FRENCH:  return (lang_id & GRFLB_FRENCH)  != 0;
			case GRFLX_SPANISH: return (lang_id & GRFLB_SPANISH) != 0;
			default:            return (lang_id & (GRFLB_ENGLISH | GRFLB_AMERICAN)) != 0;
		}
	}

	return (lang_id == _current_lang_id || lang_id == GRFLX_UNSPECIFIED);
}

/**
 * House cleaning.
 * Remove all strings.
 */
void CleanUpStrings()
{
	_grf_text.clear();
}

struct TextRefStack {
private:
	std::vector<uint8_t> stack;
public:
	const GRFFile *grffile = nullptr;

	TextRefStack(const GRFFile *grffile, std::span<const int32_t> textstack) : grffile(grffile)
	{
		this->stack.reserve(textstack.size() * 4 + 6); // for translations it is reasonable to push 3 word and rotate them
		for (int32_t value : std::ranges::reverse_view(textstack)) {
			this->PushDWord(value);
		}
	}

	uint8_t PopUnsignedByte()
	{
		if (this->stack.empty()) return 0;
		auto res = this->stack.back();
		this->stack.pop_back();
		return res;
	}
	int8_t   PopSignedByte()    { return (int8_t)this->PopUnsignedByte(); }

	uint16_t PopUnsignedWord()
	{
		uint16_t val = this->PopUnsignedByte();
		return val | (this->PopUnsignedByte() << 8);
	}
	int16_t  PopSignedWord()    { return (int32_t)this->PopUnsignedWord(); }

	uint32_t PopUnsignedDWord()
	{
		uint32_t val = this->PopUnsignedWord();
		return val | (this->PopUnsignedWord() << 16);
	}
	int32_t  PopSignedDWord()   { return (int32_t)this->PopUnsignedDWord(); }

	uint64_t PopUnsignedQWord()
	{
		uint64_t val = this->PopUnsignedDWord();
		return val | (((uint64_t)this->PopUnsignedDWord()) << 32);
	}
	int64_t  PopSignedQWord()   { return (int64_t)this->PopUnsignedQWord(); }

	/** Rotate the top four words down: W1, W2, W3, W4 -> W4, W1, W2, W3 */
	void RotateTop4Words()
	{
		auto w1 = this->PopUnsignedWord();
		auto w2 = this->PopUnsignedWord();
		auto w3 = this->PopUnsignedWord();
		auto w4 = this->PopUnsignedWord();
		this->PushWord(w3);
		this->PushWord(w2);
		this->PushWord(w1);
		this->PushWord(w4);
	}

	void PushByte(uint8_t b)
	{
		this->stack.push_back(b);
	}

	void PushWord(uint16_t w)
	{
		this->PushByte(GB(w, 8, 8));
		this->PushByte(GB(w, 0, 8));
	}

	void PushDWord(uint32_t dw)
	{
		this->PushWord(GB(dw, 16, 16));
		this->PushWord(GB(dw, 0, 16));
	}
};

static void HandleNewGRFStringControlCodes(std::string_view str, TextRefStack &stack, std::vector<StringParameter> &params);

/**
 * Process NewGRF string control code instructions.
 * @param scc The string control code that has been read.
 * @param consumer The string that we are reading from.
 * @param stack The TextRefStack.
 * @param[out] params Output parameters
 */
static void ProcessNewGRFStringControlCode(char32_t scc, StringConsumer &consumer, TextRefStack &stack, std::vector<StringParameter> &params)
{
	/* There is data on the NewGRF text stack, and we want to move them to OpenTTD's string stack.
	 * After this call, a new call is made with `modify_parameters` set to false when the string is finally formatted. */
	switch (scc) {
		default: return;

		case SCC_PLURAL_LIST:
			consumer.SkipUint8(); // plural form
			[[fallthrough]];
		case SCC_GENDER_LIST: {
			consumer.SkipUint8(); // offset
			/* plural and gender choices cannot contain any string commands, so just skip the whole thing */
			uint num = consumer.ReadUint8();
			uint total_len = 0;
			for (uint i = 0; i != num; i++) {
				total_len += consumer.ReadUint8();
			}
			consumer.Skip(total_len);
			break;
		}

		case SCC_SWITCH_CASE: {
			/* skip all cases and continue with default case */
			uint num = consumer.ReadUint8();
			for (uint i = 0; i != num; i++) {
				consumer.SkipUint8();
				auto len = consumer.ReadUint16LE();
				consumer.Skip(len);
			}
			consumer.SkipUint16LE(); // length of default
			break;
		}

		case SCC_GENDER_INDEX:
		case SCC_SET_CASE:
			consumer.SkipUint8();
			break;

		case SCC_ARG_INDEX:
			NOT_REACHED();
			break;

		case SCC_NEWGRF_PRINT_BYTE_SIGNED:      params.emplace_back(stack.PopSignedByte());    break;
		case SCC_NEWGRF_PRINT_QWORD_CURRENCY:   params.emplace_back(stack.PopSignedQWord());   break;

		case SCC_NEWGRF_PRINT_DWORD_CURRENCY:
		case SCC_NEWGRF_PRINT_DWORD_SIGNED:     params.emplace_back(stack.PopSignedDWord());   break;

		case SCC_NEWGRF_PRINT_BYTE_HEX:         params.emplace_back(stack.PopUnsignedByte());  break;
		case SCC_NEWGRF_PRINT_QWORD_HEX:        params.emplace_back(stack.PopUnsignedQWord()); break;

		case SCC_NEWGRF_PRINT_WORD_SPEED:
		case SCC_NEWGRF_PRINT_WORD_VOLUME_LONG:
		case SCC_NEWGRF_PRINT_WORD_VOLUME_SHORT:
		case SCC_NEWGRF_PRINT_WORD_SIGNED:      params.emplace_back(stack.PopSignedWord());    break;

		case SCC_NEWGRF_PRINT_WORD_HEX:
		case SCC_NEWGRF_PRINT_WORD_WEIGHT_LONG:
		case SCC_NEWGRF_PRINT_WORD_WEIGHT_SHORT:
		case SCC_NEWGRF_PRINT_WORD_POWER:
		case SCC_NEWGRF_PRINT_WORD_STATION_NAME:
		case SCC_NEWGRF_PRINT_WORD_UNSIGNED:    params.emplace_back(stack.PopUnsignedWord());  break;

		case SCC_NEWGRF_PRINT_DWORD_FORCE:
		case SCC_NEWGRF_PRINT_DWORD_DATE_LONG:
		case SCC_NEWGRF_PRINT_DWORD_DATE_SHORT:
		case SCC_NEWGRF_PRINT_DWORD_HEX:        params.emplace_back(stack.PopUnsignedDWord()); break;

		/* Dates from NewGRFs have 1920-01-01 as their zero point, convert it to OpenTTD's epoch. */
		case SCC_NEWGRF_PRINT_WORD_DATE_LONG:
		case SCC_NEWGRF_PRINT_WORD_DATE_SHORT:  params.emplace_back(CalendarTime::DAYS_TILL_ORIGINAL_BASE_YEAR + stack.PopUnsignedWord()); break;

		case SCC_NEWGRF_DISCARD_WORD:           stack.PopUnsignedWord(); break;

		case SCC_NEWGRF_ROTATE_TOP_4_WORDS:     stack.RotateTop4Words(); break;
		case SCC_NEWGRF_PUSH_WORD:              stack.PushWord(consumer.ReadUtf8(0)); break;

		case SCC_NEWGRF_PRINT_WORD_CARGO_LONG:
		case SCC_NEWGRF_PRINT_WORD_CARGO_SHORT:
		case SCC_NEWGRF_PRINT_WORD_CARGO_TINY:
			params.emplace_back(GetCargoTranslation(stack.PopUnsignedWord(), stack.grffile));
			params.emplace_back(stack.PopUnsignedWord());
			break;

		case SCC_NEWGRF_STRINL: {
			StringID stringid = consumer.ReadUtf8(STR_NULL);
			/* We also need to handle the substring's stack usage. */
			HandleNewGRFStringControlCodes(GetStringPtr(stringid), stack, params);
			break;
		}

		case SCC_NEWGRF_PRINT_WORD_STRING_ID: {
			StringID stringid = MapGRFStringID(stack.grffile->grfid, GRFStringID{stack.PopUnsignedWord()});
			params.emplace_back(stringid);
			/* We also need to handle the substring's stack usage. */
			HandleNewGRFStringControlCodes(GetStringPtr(stringid), stack, params);
			break;
		}

		case SCC_NEWGRF_PRINT_WORD_CARGO_NAME: {
			CargoType cargo = GetCargoTranslation(stack.PopUnsignedWord(), stack.grffile);
			params.emplace_back(cargo < NUM_CARGO ? 1ULL << cargo : 0);
			break;
		}
	}
}

/**
 * Emit OpenTTD's internal string code for the different NewGRF string codes.
 * @param scc NewGRF string code.
 * @param consumer String consumer, moved forward if SCC_NEWGRF_PUSH_WORD is found.
 * @returns String code to use.
 */
char32_t RemapNewGRFStringControlCode(char32_t scc, StringConsumer &consumer)
{
	switch (scc) {
		default:
			return scc;

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

		case SCC_NEWGRF_PRINT_DWORD_FORCE:
			return SCC_FORCE;

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

		/* These NewGRF string codes modify the NewGRF stack or otherwise do not map to OpenTTD string codes. */
		case SCC_NEWGRF_PUSH_WORD:
			consumer.SkipUtf8();
			return 0;

		case SCC_NEWGRF_DISCARD_WORD:
		case SCC_NEWGRF_ROTATE_TOP_4_WORDS:
			return 0;
	}
}

/**
 * Handle control codes in a NewGRF string, processing the stack and filling parameters.
 * @param str String to process.
 * @param[in,out] stack Stack to use.
 * @param[out] params Parameters to fill.
 */
static void HandleNewGRFStringControlCodes(std::string_view str, TextRefStack &stack, std::vector<StringParameter> &params)
{
	StringConsumer consumer(str);
	while (consumer.AnyBytesLeft()) {
		char32_t scc = consumer.ReadUtf8();
		ProcessNewGRFStringControlCode(scc, consumer, stack, params);
	}
}

/**
 * Process the text ref stack for a GRF String and return its parameters.
 * @param grffile GRFFile of string.
 * @param stringid StringID of string.
 * @param textstack Text parameter stack.
 * @returns Parameters for GRF string.
 */
std::vector<StringParameter> GetGRFStringTextStackParameters(const GRFFile *grffile, StringID stringid, std::span<const int32_t> textstack)
{
	if (stringid == INVALID_STRING_ID) return {};

	auto str = GetStringPtr(stringid);

	std::vector<StringParameter> params;
	params.reserve(20);

	TextRefStack stack{grffile, textstack};
	HandleNewGRFStringControlCodes(str, stack, params);

	return params;
}

/**
 * Format a GRF string using the text ref stack for parameters.
 * @param grffile GRFFile of string.
 * @param grfstringid GRFStringID of string.
 * @param textstack Text parameter stack.
 * @returns Formatted string.
 */
std::string GetGRFStringWithTextStack(const struct GRFFile *grffile, GRFStringID grfstringid, std::span<const int32_t> textstack)
{
	StringID stringid = GetGRFStringID(grffile->grfid, grfstringid);
	auto params = GetGRFStringTextStackParameters(grffile, stringid, textstack);
	return GetStringWithArgs(stringid, params);
}
