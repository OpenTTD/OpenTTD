/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file truetypefontcache.h Common base definition for font file based font caches. */

#ifndef TRUETYPEFONTCACHE_H
#define TRUETYPEFONTCACHE_H

#include "../fontcache.h"


static const int MAX_FONT_SIZE = 72; ///< Maximum font size.

static const byte FACE_COLOUR = 1;
static const byte SHADOW_COLOUR = 2;

/** Font cache for fonts that are based on a TrueType font. */
class TrueTypeFontCache : public FontCache {
protected:
	static constexpr int MAX_GLYPH_DIM = 256;          ///< Maximum glyph dimensions.
	static constexpr uint MAX_FONT_MIN_REC_SIZE = 20u; ///< Upper limit for the recommended font size in case a font file contains nonsensical values.

	int req_size;  ///< Requested font size.
	int used_size; ///< Used font size.

	using FontTable = std::map<uint32_t, std::pair<size_t, const void *>>; ///< Table with font table cache
	FontTable font_tables; ///< Cached font tables.

	/** Container for information about a glyph. */
	struct GlyphEntry {
		Sprite *sprite; ///< The loaded sprite.
		byte width;     ///< The width of the glyph.
		bool duplicate; ///< Whether this glyph entry is a duplicate, i.e. may this be freed?
	};

	/**
	 * The glyph cache. This is structured to reduce memory consumption.
	 * 1) There is a 'segment' table for each font size.
	 * 2) Each segment table is a discrete block of characters.
	 * 3) Each block contains 256 (aligned) characters sequential characters.
	 *
	 * The cache is accessed in the following way:
	 * For character 0x0041  ('A'): glyph_to_sprite[0x00][0x41]
	 * For character 0x20AC (Euro): glyph_to_sprite[0x20][0xAC]
	 *
	 * Currently only 256 segments are allocated, "limiting" us to 65536 characters.
	 * This can be simply changed in the two functions Get & SetGlyphPtr.
	 */
	GlyphEntry **glyph_to_sprite;

	GlyphEntry *GetGlyphPtr(GlyphID key);
	void SetGlyphPtr(GlyphID key, const GlyphEntry *glyph, bool duplicate = false);

	virtual const void *InternalGetFontTable(uint32_t tag, size_t &length) = 0;
	virtual const Sprite *InternalGetGlyph(GlyphID key, bool aa) = 0;

public:
	TrueTypeFontCache(FontSize fs, int pixels);
	virtual ~TrueTypeFontCache();
	int GetFontSize() const override { return this->used_size; }
	void SetUnicodeGlyph(char32_t key, SpriteID sprite) override { this->parent->SetUnicodeGlyph(key, sprite); }
	void InitializeUnicodeGlyphMap() override { this->parent->InitializeUnicodeGlyphMap(); }
	const Sprite *GetGlyph(GlyphID key) override;
	const void *GetFontTable(uint32_t tag, size_t &length) override;
	void ClearFontCache() override;
	uint GetGlyphWidth(GlyphID key) override;
	bool GetDrawGlyphShadow() override;
	bool IsBuiltInFont() override { return false; }
};

#endif /* TRUETYPEFONTCACHE_H */
