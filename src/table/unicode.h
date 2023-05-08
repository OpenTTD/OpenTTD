/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file unicode.h Character mapping for using Unicode characters in OTTD. */

struct DefaultUnicodeMapping {
	char32_t code; ///< Unicode value
	byte key;   ///< Character index of sprite
};

static const byte CLRA = 0; ///< Identifier to clear all glyphs at this codepoint

/* Default unicode mapping table for sprite based glyphs.
 * This table allows us use unicode characters even though the glyphs don't
 * exist, or are in the wrong place, in the standard sprite fonts.
 * This is not used for TrueType rendering */

static const DefaultUnicodeMapping _default_unicode_map[] = {
	{ 0x00A0, 0x20 }, // Non-breaking space / Up arrow
	{ 0x00AA, CLRA }, // Feminine ordinal indicator / Down arrow
	{ 0x00AC, CLRA }, // Not sign / Tick mark
	{ 0x00AD, 0x20 }, // Soft hyphen / X mark
	{ 0x00AF, CLRA }, // Macron / Right arrow
	{ 0x00B4, CLRA }, // Acute accent / Train symbol
	{ 0x00B5, CLRA }, // Micro sign / Truck symbol
	{ 0x00B6, CLRA }, // Pilcrow sign / Bus symbol
	{ 0x00B7, CLRA }, // Middle dot / Aircraft symbol
	{ 0x00B8, CLRA }, // Cedilla / Ship symbol
	{ 0x00B9, CLRA }, // Superscript 1 / Superscript -1
	{ 0x00BC, CLRA }, // One quarter / Small up arrow
	{ 0x00BD, CLRA }, // One half / Small down arrow
	{ 0x0178, 0x9F }, // Capital letter Y with diaeresis
	{ 0x010D, 0x63 }, // Small letter c with caron
};
