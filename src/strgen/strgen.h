/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file strgen.h Language pack header for strgen (needs to match). */

#ifndef STRGEN_H
#define STRGEN_H

/** Header of a language file. */
struct LanguagePackHeader {
	static const uint32 IDENT = 0x474E414C; ///< Identifier for OpenTTD language files, big endian for "LANG"

	uint32 ident;       ///< 32-bits identifier
	uint32 version;     ///< 32-bits of auto generated version info which is basically a hash of strings.h
	char name[32];      ///< the international name of this language
	char own_name[32];  ///< the localized name of this language
	char isocode[16];   ///< the ISO code for the language (not country code)
	uint16 offsets[32]; ///< the offsets

	/** Thousand separator used for anything not currencies */
	char digit_group_separator[8];
	/** Thousand separator used for currencies */
	char digit_group_separator_currency[8];
	/** Decimal separator */
	char digit_decimal_separator[8];
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
	uint16 winlangid;   ///< windows language id
	uint8 newgrflangid; ///< newgrf language id
	byte pad[3];        ///< pad header to be a multiple of 4

	/**
	 * Check whether the header is a valid header for OpenTTD.
	 * @return true iff the header is deemed valid.
	 */
	bool IsValid() const;
};

assert_compile(sizeof(LanguagePackHeader) % 4 == 0);

#endif /* STRGEN_H */
