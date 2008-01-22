/* $Id$ */


struct DefaultUnicodeMapping {
	WChar code; ///< Unicode value
	byte key;   ///< Character index of sprite
};


/* Default unicode mapping table for sprite based glyphs.
 * This table allows us use unicode characters even though the glyphs don't
 * exist, or are in the wrong place, in the standard sprite fonts.
 * This is not used for FreeType rendering */

static DefaultUnicodeMapping _default_unicode_map[] = {
	{ 0x010D, 0x63 }, /* Small letter c with caron */
};
