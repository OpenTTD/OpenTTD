/* $Id$ */

/** @file unicode.h Character mapping for using Unicode characters in OTTD. */

struct DefaultUnicodeMapping {
	WChar code; ///< Unicode value
	byte key;   ///< Character index of sprite
};

enum {
	CLRA = 0, ///< Identifier to clear all glyphs at this codepoint
	CLRL = 1, ///< Identifier to clear glyphs for large font at this codepoint
};

/* Default unicode mapping table for sprite based glyphs.
 * This table allows us use unicode characters even though the glyphs don't
 * exist, or are in the wrong place, in the standard sprite fonts.
 * This is not used for FreeType rendering */

static DefaultUnicodeMapping _default_unicode_map[] = {
	{ 0x00A0, 0x20 }, /* Non-breaking space / Up arrow */
	{ 0x00A4, CLRL }, /* Currency sign */
	{ 0x00A6, CLRL }, /* Broken bar */
	{ 0x00A7, CLRL }, /* Section sign */
	{ 0x00A8, CLRL }, /* Diaeresis */
	{ 0x00A9, CLRL }, /* Copyright sign */
	{ 0x00AA, CLRA }, /* Feminine ordinal indicator / Down arrow */
	{ 0x00AC, CLRA }, /* Not sign / Tick mark */
	{ 0x00AD, 0x20 }, /* Soft hyphen / X mark */
	{ 0x00AF, CLRA }, /* Macron / Right arrow */
	{ 0x00B0, CLRL }, /* Degree sign */
	{ 0x00B1, CLRL }, /* Plus-Minus sign */
	{ 0x00B2, CLRL }, /* Superscript 2 */
	{ 0x00B3, CLRL }, /* Superscript 3 */
	{ 0x00B4, CLRA }, /* Acute accent / Train symbol */
	{ 0x00B5, CLRA }, /* Micro sign / Truck symbol */
	{ 0x00B6, CLRA }, /* Pilcrow sign / Bus symbol */
	{ 0x00B7, CLRA }, /* Middle dot / Aircraft symbol */
	{ 0x00B8, CLRA }, /* Cedilla / Ship symbol */
	{ 0x00B9, CLRA }, /* Superscript 1 / Superscript -1 */
	{ 0x00BA, CLRL }, /* Masculine ordinal indicator */
	{ 0x00BC, CLRA }, /* One quarter / Small up arrow */
	{ 0x00BD, CLRA }, /* One half / Small down arrow */
	{ 0x00BE, CLRL }, /* Three quarters */
	{ 0x00D0, CLRL }, /* Capital letter eth */
	{ 0x00D7, CLRL }, /* Multiplication sign */
	{ 0x00D8, CLRL }, /* Capital letter O with stroke */
	{ 0x00D9, CLRL }, /* Capital letter U with grave */
	{ 0x00DE, CLRL }, /* Capital letter thorn */
	{ 0x00E6, CLRL }, /* Small letter ae */
	{ 0x00F0, CLRL }, /* Small letter eth */
	{ 0x00F7, CLRL }, /* Divison sign */
	{ 0x00F8, CLRL }, /* Small letter o with stroke */
	{ 0x00FE, CLRL }, /* Small letter thorn */
	{ 0x0178, 0x9F }, /* Capital letter Y with diaeresis */
	{ 0x010D, 0x63 }, /* Small letter c with caron */
};
