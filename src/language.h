/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file language.h Information about languages and their files. */

#ifndef LANGUAGE_H
#define LANGUAGE_H

#ifdef WITH_ICU_I18N
#include <unicode/coll.h>
#endif /* WITH_ICU_I18N */
#include "strings_type.h"
#include <filesystem>

static const uint8_t CASE_GENDER_LEN = 16; ///< The (maximum) length of a case/gender string.
static const uint8_t MAX_NUM_GENDERS =  8; ///< Maximum number of supported genders.
static const uint8_t MAX_NUM_CASES   = 16; ///< Maximum number of supported cases.

/** Header of a language file. */
struct LanguagePackHeader {
	static const uint32_t IDENT = 0x474E414C; ///< Identifier for OpenTTD language files, big endian for "LANG"

	uint32_t ident;       ///< 32-bits identifier
	uint32_t version;     ///< 32-bits of auto generated version info which is basically a hash of strings.h
	char name[32];      ///< the international name of this language
	char own_name[32];  ///< the localized name of this language
	char isocode[16];   ///< the ISO code for the language (not country code)
	uint16_t offsets[TEXT_TAB_END]; ///< the offsets

	/** Thousand separator used for anything not currencies */
	char digit_group_separator[8];
	/** Thousand separator used for currencies */
	char digit_group_separator_currency[8];
	/** Decimal separator */
	char digit_decimal_separator[8];
	uint16_t missing;     ///< number of missing strings.
	byte plural_form;   ///< plural form index
	byte text_dir;      ///< default direction of the text
	/**
	 * Windows language ID:
	 * Windows cannot and will not convert isocodes to something it can use to
	 * determine whether a font can be used for the language or not. As a result
	 * of that we need to pass the language id via strgen to OpenTTD to tell
	 * what language it is in "Windows". The ID is the 'locale identifier' on:
	 *   http://msdn.microsoft.com/en-us/library/ms776294.aspx
	 */
	uint16_t winlangid;   ///< windows language id
	uint8_t newgrflangid; ///< newgrf language id
	uint8_t num_genders;  ///< the number of genders of this language
	uint8_t num_cases;    ///< the number of cases of this language
	byte pad[3];        ///< pad header to be a multiple of 4

	char genders[MAX_NUM_GENDERS][CASE_GENDER_LEN]; ///< the genders used by this translation
	char cases[MAX_NUM_CASES][CASE_GENDER_LEN];     ///< the cases used by this translation

	bool IsValid() const;
	bool IsReasonablyFinished() const;

	/**
	 * Get the index for the given gender.
	 * @param gender_str The string representation of the gender.
	 * @return The index of the gender, or MAX_NUM_GENDERS when the gender is unknown.
	 */
	uint8_t GetGenderIndex(const char *gender_str) const
	{
		for (uint8_t i = 0; i < MAX_NUM_GENDERS; i++) {
			if (strcmp(gender_str, this->genders[i]) == 0) return i;
		}
		return MAX_NUM_GENDERS;
	}

	/**
	 * Get the index for the given case.
	 * @param case_str The string representation of the case.
	 * @return The index of the case, or MAX_NUM_CASES when the case is unknown.
	 */
	uint8_t GetCaseIndex(const char *case_str) const
	{
		for (uint8_t i = 0; i < MAX_NUM_CASES; i++) {
			if (strcmp(case_str, this->cases[i]) == 0) return i;
		}
		return MAX_NUM_CASES;
	}
};
/** Make sure the size is right. */
static_assert(sizeof(LanguagePackHeader) % 4 == 0);

/** Metadata about a single language. */
struct LanguageMetadata : public LanguagePackHeader {
	std::filesystem::path file; ///< Name of the file we read this data from.
};

/** Type for the list of language meta data. */
typedef std::vector<LanguageMetadata> LanguageList;

/** The actual list of language meta data. */
extern LanguageList _languages;

/** The currently loaded language. */
extern const LanguageMetadata *_current_language;

#ifdef WITH_ICU_I18N
extern std::unique_ptr<icu::Collator> _current_collator;
#endif /* WITH_ICU_I18N */

bool ReadLanguagePack(const LanguageMetadata *lang);
const LanguageMetadata *GetLanguage(byte newgrflangid);

#endif /* LANGUAGE_H */
