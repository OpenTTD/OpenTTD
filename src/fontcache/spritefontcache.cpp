/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file spritefontcache.cpp Sprite fontcache implementation. */

#include "../stdafx.h"
#include "../fontcache.h"
#include "../gfx_layout.h"
#include "../spritecache.h"
#include "../string_func.h"
#include "../zoom_func.h"
#include "spritefontcache.h"

#include "../table/sprites.h"
#include "../table/control_codes.h"
#include "../table/unicode.h"

#include "../safeguards.h"

static const int ASCII_LETTERSTART = 32; ///< First printable ASCII letter.

/**
 * Scale traditional pixel dimensions to font zoom level, for drawing sprite fonts.
 * @param value Pixel amount at #ZOOM_BASE (traditional "normal" interface size).
 * @return Pixel amount at _font_zoom (current interface size).
 */
static int ScaleFontTrad(int value)
{
	return UnScaleByZoom(value * ZOOM_BASE, _font_zoom);
}

static std::array<std::unordered_map<char32_t, SpriteID>, FS_END> _char_maps{}; ///< Glyph map for each font size.

/**
 * Get SpriteID associated with a character.
 * @param key Character to find.
 * @return SpriteID for character, or 0 if not present.
 */
static SpriteID GetUnicodeGlyph(FontSize fs, char32_t key)
{
	auto found = _char_maps[fs].find(key);
	if (found != std::end(_char_maps[fs])) return found->second;
	return 0;
}

/**
 * Set the SpriteID for a unicode character.
 * @param fs Font size to set.
 * @param key Unicode character to set.
 * @param sprite SpriteID of character.
 */
void SetUnicodeGlyph(FontSize fs, char32_t key, SpriteID sprite)
{
	_char_maps[fs][key] = sprite;
}

/**
 * Initialize the glyph map for a font size.
 * This populates the glyph map with the baseset font sprites.
 * @param fs Font size to initialize.
 */
void InitializeUnicodeGlyphMap(FontSize fs)
{
	/* Clear out existing glyph map if it exists */
	_char_maps[fs].clear();

	SpriteID base;
	switch (fs) {
		default: NOT_REACHED();
		case FS_MONO:   // Use normal as default for mono spaced font
		case FS_NORMAL: base = SPR_ASCII_SPACE;       break;
		case FS_SMALL:  base = SPR_ASCII_SPACE_SMALL; break;
		case FS_LARGE:  base = SPR_ASCII_SPACE_BIG;   break;
	}

	for (uint i = ASCII_LETTERSTART; i < 256; i++) {
		SpriteID sprite = base + i - ASCII_LETTERSTART;
		if (!SpriteExists(sprite)) continue;
		SetUnicodeGlyph(fs, i, sprite);
		SetUnicodeGlyph(fs, i + SCC_SPRITE_START, sprite);
	}

	/* Modify map to move non-standard glyphs to a better unicode codepoint. */
	for (const auto &unicode_map : _default_unicode_map) {
		uint8_t key = unicode_map.key;
		if (key == CLRA) {
			/* Clear the glyph. This happens if the glyph at this code point
			 * is non-standard and should be accessed by an SCC_xxx enum
			 * entry only. */
			SetUnicodeGlyph(fs, unicode_map.code, 0);
		} else {
			SpriteID sprite = base + key - ASCII_LETTERSTART;
			if (!SpriteExists(sprite)) continue;
			SetUnicodeGlyph(fs, unicode_map.code, sprite);
		}
	}
}

/**
 * Initialize the glyph map.
 */
void InitializeUnicodeGlyphMap()
{
	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		InitializeUnicodeGlyphMap(fs);
	}
}

/**
 * Create a new sprite font cache.
 * @param fs The font size to create the cache for.
 */
SpriteFontCache::SpriteFontCache(FontSize fs) : FontCache(fs)
{
	this->height = ScaleGUITrad(FontCache::GetDefaultFontHeight(this->fs));
	this->ascender = (this->height - ScaleFontTrad(FontCache::GetDefaultFontHeight(this->fs))) / 2;
}

void SpriteFontCache::ClearFontCache()
{
	Layouter::ResetFontCache(this->fs);
	this->height = ScaleGUITrad(FontCache::GetDefaultFontHeight(this->fs));
	this->ascender = (this->height - ScaleFontTrad(FontCache::GetDefaultFontHeight(this->fs))) / 2;
}

const Sprite *SpriteFontCache::GetGlyph(GlyphID key)
{
	SpriteID sprite = static_cast<SpriteID>(key & ~SPRITE_GLYPH);
	if (sprite == 0) sprite = GetUnicodeGlyph(this->fs, '?');
	return GetSprite(sprite, SpriteType::Font);
}

uint SpriteFontCache::GetGlyphWidth(GlyphID key)
{
	SpriteID sprite = static_cast<SpriteID>(key & ~SPRITE_GLYPH);
	if (sprite == 0) sprite = GetUnicodeGlyph(this->fs, '?');
	return SpriteExists(sprite) ? GetSprite(sprite, SpriteType::Font)->width + ScaleFontTrad(this->fs != FS_NORMAL ? 1 : 0) : 0;
}

GlyphID SpriteFontCache::MapCharToGlyph(char32_t key, [[maybe_unused]] bool allow_fallback)
{
	assert(IsPrintable(key));
	SpriteID sprite = GetUnicodeGlyph(this->fs, key);
	if (sprite == 0) return 0;
	return SPRITE_GLYPH | sprite;
}

bool SpriteFontCache::GetDrawGlyphShadow()
{
	return false;
}

class SpriteFontCacheFactory : public FontCacheFactory {
public:
	SpriteFontCacheFactory() : FontCacheFactory("sprite", "Sprite font provider") {}

	std::unique_ptr<FontCache> LoadFont(FontSize fs, FontType fonttype) const override
	{
		if (fonttype != FontType::Sprite) return nullptr;

		return std::make_unique<SpriteFontCache>(fs);
	}

	bool FindFallbackFont(struct FontCacheSettings *, const std::string &, class MissingGlyphSearcher *) const override
	{
		return false;
	}

private:
	static SpriteFontCacheFactory instance;
};

/* static */ SpriteFontCacheFactory SpriteFontCacheFactory::instance;
