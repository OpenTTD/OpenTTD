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

/**
 * Create a new sprite font cache.
 * @param fs The font size to create the cache for.
 */
SpriteFontCache::SpriteFontCache(FontSize fs) : FontCache(fs)
{
	this->InitializeUnicodeGlyphMap();
	this->height = ScaleGUITrad(FontCache::GetDefaultFontHeight(this->fs));
	this->ascender = (this->height - ScaleFontTrad(FontCache::GetDefaultFontHeight(this->fs))) / 2;
}

/**
 * Get SpriteID associated with a GlyphID.
 * @param key Glyph to find.
 * @return SpriteID of glyph, or 0 if not present.
 */
SpriteID SpriteFontCache::GetUnicodeGlyph(GlyphID key)
{
	const auto found = this->glyph_to_spriteid_map.find(key & ~SPRITE_GLYPH);
	if (found == std::end(this->glyph_to_spriteid_map)) return 0;
	return found->second;
}

void SpriteFontCache::SetUnicodeGlyph(char32_t key, SpriteID sprite)
{
	this->glyph_to_spriteid_map[key] = sprite;
}

void SpriteFontCache::InitializeUnicodeGlyphMap()
{
	/* Clear out existing glyph map if it exists */
	this->glyph_to_spriteid_map.clear();

	SpriteID base;
	switch (this->fs) {
		default: NOT_REACHED();
		case FS_MONO:   // Use normal as default for mono spaced font
		case FS_NORMAL: base = SPR_ASCII_SPACE;       break;
		case FS_SMALL:  base = SPR_ASCII_SPACE_SMALL; break;
		case FS_LARGE:  base = SPR_ASCII_SPACE_BIG;   break;
	}

	for (uint i = ASCII_LETTERSTART; i < 256; i++) {
		SpriteID sprite = base + i - ASCII_LETTERSTART;
		if (!SpriteExists(sprite)) continue;
		this->SetUnicodeGlyph(i, sprite);
		this->SetUnicodeGlyph(i + SCC_SPRITE_START, sprite);
	}

	for (const auto &unicode_map : _default_unicode_map) {
		uint8_t key = unicode_map.key;
		if (key == CLRA) {
			/* Clear the glyph. This happens if the glyph at this code point
			 * is non-standard and should be accessed by an SCC_xxx enum
			 * entry only. */
			this->SetUnicodeGlyph(unicode_map.code, 0);
		} else {
			SpriteID sprite = base + key - ASCII_LETTERSTART;
			this->SetUnicodeGlyph(unicode_map.code, sprite);
		}
	}
}

void SpriteFontCache::ClearFontCache()
{
	Layouter::ResetFontCache(this->fs);
	this->height = ScaleGUITrad(FontCache::GetDefaultFontHeight(this->fs));
	this->ascender = (this->height - ScaleFontTrad(FontCache::GetDefaultFontHeight(this->fs))) / 2;
}

const Sprite *SpriteFontCache::GetGlyph(GlyphID key)
{
	SpriteID sprite = this->GetUnicodeGlyph(static_cast<char32_t>(key & ~SPRITE_GLYPH));
	if (sprite == 0) sprite = this->GetUnicodeGlyph('?');
	return GetSprite(sprite, SpriteType::Font);
}

uint SpriteFontCache::GetGlyphWidth(GlyphID key)
{
	SpriteID sprite = this->GetUnicodeGlyph(static_cast<char32_t>(key & ~SPRITE_GLYPH));
	if (sprite == 0) sprite = this->GetUnicodeGlyph('?');
	return SpriteExists(sprite) ? GetSprite(sprite, SpriteType::Font)->width + ScaleFontTrad(this->fs != FS_NORMAL ? 1 : 0) : 0;
}

bool SpriteFontCache::GetDrawGlyphShadow()
{
	return false;
}
