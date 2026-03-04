/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file spritefontcache.h Sprite font cache implementation definition. */

#ifndef SPRITEFONTCACHE_H
#define SPRITEFONTCACHE_H

#include "../fontcache.h"

/** Font cache for fonts that are based on a freetype font. */
class SpriteFontCache : public FontCache {
public:
	SpriteFontCache(FontSize fs);
	void ClearFontCache() override;
	const Sprite *GetGlyph(GlyphID key) override;
	uint GetGlyphWidth(GlyphID key) override;
	bool GetDrawGlyphShadow() override;
	GlyphID MapCharToGlyph(char32_t key, bool allow_fallback = true) override;
	std::string GetFontName() override { return "sprite"; }
	bool IsBuiltInFont() override { return true; }
};

#endif /* SPRITEFONTCACHE_H */
