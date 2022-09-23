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
	SpriteID GetUnicodeGlyph(WChar key);

	void ClearGlyphToSpriteMap();
public:
	SpriteFontCache(FontSize fs);
	~SpriteFontCache();
	virtual void SetUnicodeGlyph(WChar key, SpriteID sprite);
	virtual void InitializeUnicodeGlyphMap();
	virtual void ClearFontCache();
	virtual const Sprite *GetGlyph(GlyphID key);
	virtual uint GetGlyphWidth(GlyphID key);
	virtual bool GetDrawGlyphShadow();
	virtual GlyphID MapCharToGlyph(WChar key) { assert(IsPrintable(key)); return SPRITE_GLYPH | key; }
	virtual const void *GetFontTable(uint32 tag, size_t &length) { length = 0; return nullptr; }
	virtual const char *GetFontName() { return "sprite"; }
	virtual bool IsBuiltInFont() { return true; }
};

#endif /* SPRITEFONTCACHE_H */
