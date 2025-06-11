/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file iconfontcache.h Icon font cache implementation definition. */

#ifndef ICONFONTCACHE_H
#define ICONFONTCACHE_H

#include "../fontcache.h"

/** Font cache for fonts that are based on a freetype font. */
class IconFontCache : public FontCache {
public:
	IconFontCache(FontSize fs);
	void ClearFontCache() override;
	void DrawGlyph(GlyphID key, const Rect &r) override;
	void DrawGlyphShadow(GlyphID, const Rect &) override {}
	uint GetGlyphWidth(GlyphID key) override;
	bool GetDrawGlyphShadow() override { return false; }
	GlyphID MapCharToGlyph(char32_t key) override;
	std::string GetFontName() override { return "icon"; }
	bool IsBuiltInFont() override { return true; }

private:
	void UpdateMetrics();
};

#endif /* ICONFONTCACHE_H */
