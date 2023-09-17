/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file spritefontcache.h Sprite font cache implementation definition. */

#ifndef SPRITEFONTCACHE_H
#define SPRITEFONTCACHE_H

#include "../string_func.h"
#include "../fontcache.h"

/** Font cache for fonts that are based on a freetype font. */
class SpriteFontCache : public FontCache {
private:
	SpriteID **glyph_to_spriteid_map; ///< Mapping of glyphs to sprite IDs.
	SpriteID GetUnicodeGlyph(char32_t key);

	void ClearGlyphToSpriteMap();
public:
	SpriteFontCache(FontSize fs);
	~SpriteFontCache();
	void SetUnicodeGlyph(char32_t key, SpriteID sprite) override;
	void InitializeUnicodeGlyphMap() override;
	void ClearFontCache() override;
	const Sprite *GetGlyph(GlyphID key) override;
	uint GetGlyphWidth(GlyphID key) override;
	bool GetDrawGlyphShadow() override;
	GlyphID MapCharToGlyph(char32_t key) override { assert(IsPrintable(key)); return SPRITE_GLYPH | key; }
	const void *GetFontTable(uint32_t, size_t &length) override { length = 0; return nullptr; }
	std::string GetFontName() override { return "sprite"; }
	bool IsBuiltInFont() override { return true; }
};

#endif /* SPRITEFONTCACHE_H */
