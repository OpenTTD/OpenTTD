/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_text.h Header of Action 04 "universal holder" structure and functions */

#ifndef NEWGRF_TEXT_H
#define NEWGRF_TEXT_H

#include "string_type.h"
#include "strings_type.h"
#include "table/control_codes.h"
#include <utility>

/** This character, the thorn ('þ'), indicates a unicode string to NFO. */
static const char32_t NFO_UTF8_IDENTIFIER = 0x00DE;

/** A GRF text with associated language ID. */
struct GRFText {
	byte langid;      ///< The language associated with this GRFText.
	std::string text; ///< The actual (translated) text.
};

/** A GRF text with a list of translations. */
typedef std::vector<GRFText> GRFTextList;
/** Reference counted wrapper around a GRFText pointer. */
typedef std::shared_ptr<GRFTextList> GRFTextWrapper;

StringID AddGRFString(uint32_t grfid, uint16_t stringid, byte langid, bool new_scheme, bool allow_newlines, const char *text_to_add, StringID def_string);
StringID GetGRFStringID(uint32_t grfid, StringID stringid);
const char *GetGRFStringFromGRFText(const GRFTextList &text_list);
const char *GetGRFStringFromGRFText(const GRFTextWrapper &text);
const char *GetGRFStringPtr(uint32_t stringid);
void CleanUpStrings();
void SetCurrentGrfLangID(byte language_id);
std::string TranslateTTDPatchCodes(uint32_t grfid, uint8_t language_id, bool allow_newlines, const std::string &str, StringControlCode byte80 = SCC_NEWGRF_PRINT_WORD_STRING_ID);
void AddGRFTextToList(GRFTextList &list, byte langid, uint32_t grfid, bool allow_newlines, const char *text_to_add);
void AddGRFTextToList(GRFTextWrapper &list, byte langid, uint32_t grfid, bool allow_newlines, const char *text_to_add);
void AddGRFTextToList(GRFTextWrapper &list, const std::string &text_to_add);

bool CheckGrfLangID(byte lang_id, byte grf_version);

void StartTextRefStackUsage(const struct GRFFile *grffile, byte numEntries, const uint32_t *values = nullptr);
void StopTextRefStackUsage();
bool UsingNewGRFTextStack();
struct TextRefStack *CreateTextRefStackBackup();
void RestoreTextRefStackBackup(struct TextRefStack *backup);

/** Mapping of language data between a NewGRF and OpenTTD. */
struct LanguageMap {
	/** Mapping between NewGRF and OpenTTD IDs. */
	struct Mapping {
		byte newgrf_id;  ///< NewGRF's internal ID for a case/gender.
		byte openttd_id; ///< OpenTTD's internal ID for a case/gender.
	};

	/* We need a vector and can't use SmallMap due to the fact that for "setting" a
	 * gender of a string or requesting a case for a substring we want to map from
	 * the NewGRF's internal ID to OpenTTD's ID whereas for the choice lists we map
	 * the genders/cases/plural OpenTTD IDs to the NewGRF's internal IDs. In this
	 * case a NewGRF developer/translator might want a different translation for
	 * both cases. Thus we are basically implementing a multi-map. */
	std::vector<Mapping> gender_map; ///< Mapping of NewGRF and OpenTTD IDs for genders.
	std::vector<Mapping> case_map;   ///< Mapping of NewGRF and OpenTTD IDs for cases.
	int plural_form;                 ///< The plural form used for this language.

	int GetMapping(int newgrf_id, bool gender) const;
	int GetReverseMapping(int openttd_id, bool gender) const;
	static const LanguageMap *GetLanguageMap(uint32_t grfid, uint8_t language_id);
};

#endif /* NEWGRF_TEXT_H */
