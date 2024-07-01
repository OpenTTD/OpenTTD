/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file truetypefontcache.cpp Common base implementation for font file based font caches. */

#include "../stdafx.h"
#include "../debug.h"
#include "../fontcache.h"
#include "../core/bitmath_func.hpp"
#include "../gfx_layout.h"
#include "truetypefontcache.h"

#include "../safeguards.h"

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
	if (found == std::end(this->glyph_to_sprite_map)) return nullptr;
	return &found->second;
}

TrueTypeFontCache::GlyphEntry &TrueTypeFontCache::SetGlyphPtr(GlyphID key, GlyphEntry &&glyph)
{
	this->glyph_to_sprite_map[key] = std::move(glyph);
	return this->glyph_to_sprite_map[key];
}

bool TrueTypeFontCache::GetDrawGlyphShadow()
{
	return this->fs == FS_NORMAL && GetFontAAState();
}

uint TrueTypeFontCache::GetGlyphWidth(GlyphID key)
{
	if ((key & SPRITE_GLYPH) != 0) return this->parent->GetGlyphWidth(key);

	GlyphEntry *glyph = this->GetGlyphPtr(key);
	if (glyph == nullptr || glyph->data == nullptr) {
		this->GetGlyph(key);
		glyph = this->GetGlyphPtr(key);
	}

	return glyph->width;
}

const Sprite *TrueTypeFontCache::GetGlyph(GlyphID key)
{
	if ((key & SPRITE_GLYPH) != 0) return this->parent->GetGlyph(key);

	/* Check for the glyph in our cache */
	GlyphEntry *glyph = this->GetGlyphPtr(key);
	if (glyph != nullptr && glyph->data != nullptr) return glyph->GetSprite();

	return this->InternalGetGlyph(key, GetFontAAState());
}
