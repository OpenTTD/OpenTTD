/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file mock_fontcache.h Mock font cache implementation definition. */

#ifndef MOCK_FONTCACHE_H
#define MOCK_FONTCACHE_H

#include "../stdafx.h"

#include "../fontcache.h"
#include "../string_func.h"

/** Font cache for mocking basic use of fonts. */
class MockFontCache : public FontCache {
public:
	MockFontCache(FontSize fs) : FontCache(fs)
	{
		this->height = FontCache::GetDefaultFontHeight(this->fs);
	}

	void SetUnicodeGlyph(char32_t, SpriteID) override {}
	void InitializeUnicodeGlyphMap() override {}
	void ClearFontCache() override {}
	const Sprite *GetGlyph(GlyphID) override { return nullptr; }
	uint GetGlyphWidth(GlyphID) override { return this->height / 2; }
	bool GetDrawGlyphShadow() override { return false; }
	GlyphID MapCharToGlyph(char32_t key) override { return key; }
	const void *GetFontTable(uint32_t, size_t &length) override { length = 0; return nullptr; }
	std::string GetFontName() override { return "mock"; }
	bool IsBuiltInFont() override { return true; }

	static void InitializeFontCaches()
	{
		for (FontSize fs = FS_BEGIN; fs != FS_END; fs++) {
			if (FontCache::caches[fs] == nullptr) new MockFontCache(fs); /* FontCache inserts itself into to the cache. */
		}
	}
};

#endif /* MOCK_FONTCACHE_H */
