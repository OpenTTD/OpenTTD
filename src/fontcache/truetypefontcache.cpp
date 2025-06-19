/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file truetypefontcache.cpp Common base implementation for font file based font caches. */

#include "../stdafx.h"
#include "../fontcache.h"
#include "../gfx_layout.h"
#include "../table/sprites.h"
#include "../blitter/base.hpp"
#include "truetypefontcache.h"

#include "../safeguards.h"

extern void GfxMainBlitter(const Sprite *sprite, int x, int y, BlitterMode mode, const SubSprite *sub = nullptr, SpriteID sprite_id = SPR_CURSOR_MOUSE, ZoomLevel zoom = ZoomLevel::Min);

/**
 * Create a new TrueTypeFontCache.
 * @param fs     The font size that is going to be cached.
 * @param pixels The number of pixels this font should be high.
 */
TrueTypeFontCache::TrueTypeFontCache(FontSize fs, int pixels) : FontCache(fs), req_size(pixels)
{
}

/**
 * Free everything that was allocated for this font cache.
 */
TrueTypeFontCache::~TrueTypeFontCache()
{
	/* Virtual functions get called statically in destructors, so make it explicit to remove any confusion. */
	this->TrueTypeFontCache::ClearFontCache();
}

/**
 * Reset cached glyphs.
 */
void TrueTypeFontCache::ClearFontCache()
{
	this->glyph_to_sprite_map.clear();
	Layouter::ResetFontCache(this->fs);
}


TrueTypeFontCache::GlyphEntry *TrueTypeFontCache::GetGlyphPtr(GlyphID key)
{
	auto found = this->glyph_to_sprite_map.find(key);
	if (found != std::end(this->glyph_to_sprite_map)) return &found->second;
	return this->InternalGetGlyph(key, GetFontAAState());
}

TrueTypeFontCache::GlyphEntry &TrueTypeFontCache::SetGlyphPtr(GlyphID key, GlyphEntry &&glyph)
{
	this->glyph_to_sprite_map[key] = std::move(glyph);
	return this->glyph_to_sprite_map[key];
}

void TrueTypeFontCache::DrawGlyph(GlyphID key, const Rect &r)
{
	extern void GfxMainBlitter(const Sprite *sprite, int x, int y, BlitterMode mode, const SubSprite *sub = nullptr, SpriteID sprite_id = SPR_CURSOR_MOUSE, ZoomLevel zoom = ZoomLevel::Min);
	GlyphEntry *glyph = this->GetGlyphPtr(key);
	const Sprite *sprite = glyph->GetSprite();
	GfxMainBlitter(sprite, r.left, r.top, BlitterMode::ColourRemap);
}

void TrueTypeFontCache::DrawGlyphShadow(GlyphID key, const Rect &r)
{
	if (this->GetDrawGlyphShadow()) DrawGlyph(key, r);
}

bool TrueTypeFontCache::GetDrawGlyphShadow()
{
	return this->fs == FS_NORMAL && GetFontAAState();
}

uint TrueTypeFontCache::GetGlyphWidth(GlyphID key)
{
	GlyphEntry *glyph = this->GetGlyphPtr(key);
	if (glyph == nullptr || glyph->data == nullptr) return 0;
	return glyph->width;
}
