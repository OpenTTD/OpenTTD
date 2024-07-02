/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file spritefontcache.cpp Sprite fontcache implementation. */

#include "../stdafx.h"

#include "../fontcache.h"
#include "../gfx_layout.h"
#include "../zoom_func.h"
#include "spritefontcache.h"

#include "../table/sprites.h"
#include "../table/control_codes.h"
#include "../table/unicode.h"

#include "../safeguards.h"

static std::array<std::unordered_map<char32_t, SpriteID>, FS_END> _glyph_maps{}; ///< Glyph map for each font size.

/**
 * Scale traditional pixel dimensions to font zoom level, for drawing sprite fonts.
 * @param value Pixel amount at #ZOOM_BASE (traditional "normal" interface size).
 * @return Pixel amount at _font_zoom (current interface size).
 */
static int ScaleFontTrad(int value)
{
	return UnScaleByZoom(value * ZOOM_BASE, _font_zoom);
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

/**
 * Get the SpriteID associated with a unicode character.
 * @param key Character to find.
 * @return SpriteID of character, or 0 if not present.
 */
SpriteID SpriteFontCache::GetSpriteIDForChar(char32_t key)
{
	const auto &glyph_map = _glyph_maps[this->fs];

	auto it = glyph_map.find(key);
	if (it != std::end(glyph_map)) return it->second;

	return 0;
}

void SpriteFontCache::ClearFontCache()
{
	Layouter::ResetFontCache(this->fs);
	this->height = ScaleGUITrad(FontCache::GetDefaultFontHeight(this->fs));
	this->ascender = (this->height - ScaleFontTrad(FontCache::GetDefaultFontHeight(this->fs))) / 2;
}

const Sprite *SpriteFontCache::GetGlyph(GlyphID key)
{
	SpriteID sprite = this->GetSpriteIDForChar(static_cast<char32_t>(key & ~SPRITE_GLYPH));
	if (sprite == 0) sprite = this->GetSpriteIDForChar('?');
	return GetSprite(sprite, SpriteType::Font);
}

uint SpriteFontCache::GetGlyphWidth(GlyphID key)
{
	SpriteID sprite = this->GetSpriteIDForChar(static_cast<char32_t>(key & ~SPRITE_GLYPH));
	if (sprite == 0) sprite = this->GetSpriteIDForChar('?');
	return SpriteExists(sprite) ? GetSprite(sprite, SpriteType::Font)->width + ScaleFontTrad(this->fs != FS_NORMAL ? 1 : 0) : 0;
}

GlyphID SpriteFontCache::MapCharToGlyph(char32_t key, [[maybe_unused]] bool allow_fallback)
{
	assert(IsPrintable(key));
	SpriteID sprite = this->GetSpriteIDForChar(key);
	if (sprite == 0) return 0;
	return SPRITE_GLYPH | key;
}

bool SpriteFontCache::GetDrawGlyphShadow()
{
	return false;
}

/**
 * Set the SpriteID for a unicode character.
 * @param fs Font size to set.
 * @param key Unicode character to set.
 * @param sprite SpriteID of character.
 */
void SetUnicodeGlyph(FontSize fs, char32_t key, SpriteID sprite)
{
	_glyph_maps[fs][key] = sprite;
}

/**
 * Get the sprite font base sprite index.
 * @param fs Font size to get base sprite for.
 * @return Base sprite for font.
 */
static SpriteID GetSpriteFontBase(FontSize fs)
{
	switch (fs) {
		default: NOT_REACHED();
		case FS_MONO: // Use normal as default for mono spaced font
		case FS_NORMAL: return SPR_ASCII_SPACE;
		case FS_SMALL: return SPR_ASCII_SPACE_SMALL;
		case FS_LARGE: return SPR_ASCII_SPACE_BIG;
	}
}

/**
 * Initialize the glyph map for a font size.
 * This populates the glyph map with the baseset font sprites.
 * @param fs Font size to initialize.
 */
void InitializeUnicodeGlyphMap(FontSize fs)
{
	static constexpr int ASCII_LETTERSTART = 32; ///< First printable ASCII letter.

	/* Clear existing glyph map. */
	_glyph_maps[fs].clear();

	SpriteID base = GetSpriteFontBase(fs);
	for (uint i = ASCII_LETTERSTART; i < 256; i++) {
		SpriteID sprite = base + i - ASCII_LETTERSTART;
		if (!SpriteExists(sprite)) continue;

		/* Map the glyph for normal use. */
		SetUnicodeGlyph(fs, i, sprite);

		/* Glyphs are also mapped to the first private use area. */
		SetUnicodeGlyph(fs, i + SCC_SPRITE_START, sprite);
	}

	/* Modify map to move non-standard glyphs to a better unicode codepoint. */
	for (const auto &unicode_map : _default_unicode_map) {
		uint8_t key = unicode_map.key;
		if (key == CLRA) {
			/* Clear the glyph. This happens if the glyph at this code point
			 * is non-standard and should be accessed by an SCC_xxx enum
			 * entry only. */
			_glyph_maps[fs].erase(unicode_map.code);
		} else {
			SpriteID sprite = base + key - ASCII_LETTERSTART;
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
