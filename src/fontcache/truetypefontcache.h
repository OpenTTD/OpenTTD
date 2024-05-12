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

static const uint8_t FACE_COLOUR = 1;
static const uint8_t SHADOW_COLOUR = 2;

/** Font cache for fonts that are based on a TrueType font. */
class TrueTypeFontCache : public FontCache {
protected:
	static constexpr int MAX_GLYPH_DIM = 256;          ///< Maximum glyph dimensions.
	static constexpr uint MAX_FONT_MIN_REC_SIZE = 20u; ///< Upper limit for the recommended font size in case a font file contains nonsensical values.

	int req_size;  ///< Requested font size.
	int used_size; ///< Used font size.

	/** Container for information about a glyph. */
	struct GlyphEntry {
		std::unique_ptr<uint8_t[]> data; ///< The loaded sprite.
		uint8_t width = 0; ///< The width of the glyph.

		Sprite *GetSprite() { return reinterpret_cast<Sprite *>(data.get()); }
	};

	std::unordered_map<GlyphID, GlyphEntry> glyph_to_sprite_map{};

	GlyphEntry *GetGlyphPtr(GlyphID key);
	GlyphEntry &SetGlyphPtr(GlyphID key, GlyphEntry &&glyph);

	virtual const Sprite *InternalGetGlyph(GlyphID key, bool aa) = 0;

public:
	TrueTypeFontCache(FontSize fs, int pixels);
	virtual ~TrueTypeFontCache();
	int GetFontSize() const override { return this->used_size; }
	void SetUnicodeGlyph(char32_t key, SpriteID sprite) override { this->parent->SetUnicodeGlyph(key, sprite); }
	void InitializeUnicodeGlyphMap() override { this->parent->InitializeUnicodeGlyphMap(); }
	const Sprite *GetGlyph(GlyphID key) override;
	void ClearFontCache() override;
	uint GetGlyphWidth(GlyphID key) override;
	bool GetDrawGlyphShadow() override;
	bool IsBuiltInFont() override { return false; }
};

#endif /* TRUETYPEFONTCACHE_H */
