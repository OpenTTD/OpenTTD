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
#include "../blitter/factory.hpp"
#include "../core/bitmath_func.hpp"
#include "../gfx_layout.h"
#include "truetypefontcache.h"

#include "../safeguards.h"

/**
 * Create a new TrueTypeFontCache.
 * @param fs     The font size that is going to be cached.
 * @param pixels The number of pixels this font should be high.
 */
TrueTypeFontCache::TrueTypeFontCache(FontSize fs, int pixels) : FontCache(fs), req_size(pixels), glyph_to_sprite(nullptr)
{
}

/**
 * Free everything that was allocated for this font cache.
 */
TrueTypeFontCache::~TrueTypeFontCache()
{
	/* Virtual functions get called statically in destructors, so make it explicit to remove any confusion. */
	this->TrueTypeFontCache::ClearFontCache();

	for (auto &iter : this->font_tables) {
		free(iter.second.second);
	}
}

/**
 * Reset cached glyphs.
 */
void TrueTypeFontCache::ClearFontCache()
{
	if (this->glyph_to_sprite == nullptr) return;

	for (int i = 0; i < 256; i++) {
		if (this->glyph_to_sprite[i] == nullptr) continue;

		for (int j = 0; j < 256; j++) {
			if (this->glyph_to_sprite[i][j].duplicate) continue;
			free(this->glyph_to_sprite[i][j].sprite);
		}

		free(this->glyph_to_sprite[i]);
	}

	free(this->glyph_to_sprite);
	this->glyph_to_sprite = nullptr;

	Layouter::ResetFontCache(this->fs);
}


TrueTypeFontCache::GlyphEntry *TrueTypeFontCache::GetGlyphPtr(GlyphID key)
{
	if (this->glyph_to_sprite == nullptr) return nullptr;
	if (this->glyph_to_sprite[GB(key, 8, 8)] == nullptr) return nullptr;
	return &this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)];
}

void TrueTypeFontCache::SetGlyphPtr(GlyphID key, const GlyphEntry *glyph, bool duplicate)
{
	if (this->glyph_to_sprite == nullptr) {
		Debug(fontcache, 3, "Allocating root glyph cache for size {}", this->fs);
		this->glyph_to_sprite = CallocT<GlyphEntry*>(256);
	}

	if (this->glyph_to_sprite[GB(key, 8, 8)] == nullptr) {
		Debug(fontcache, 3, "Allocating glyph cache for range 0x{:02X}00, size {}", GB(key, 8, 8), this->fs);
		this->glyph_to_sprite[GB(key, 8, 8)] = CallocT<GlyphEntry>(256);
	}

	Debug(fontcache, 4, "Set glyph for unicode character 0x{:04X}, size {}", key, this->fs);
	this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)].sprite = glyph->sprite;
	this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)].width = glyph->width;
	this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)].duplicate = duplicate;
}

bool TrueTypeFontCache::GetDrawGlyphShadow()
{
	return this->fs == FS_NORMAL && GetFontAAState(FS_NORMAL);
}

uint TrueTypeFontCache::GetGlyphWidth(GlyphID key)
{
	if ((key & SPRITE_GLYPH) != 0) return this->parent->GetGlyphWidth(key);

	GlyphEntry *glyph = this->GetGlyphPtr(key);
	if (glyph == nullptr || glyph->sprite == nullptr) {
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
	if (glyph != nullptr && glyph->sprite != nullptr) return glyph->sprite;

	if (key == 0) {
		GlyphID question_glyph = this->MapCharToGlyph('?');
		if (question_glyph == 0) {
			/* The font misses the '?' character. Use built-in sprite.
			 * Note: We cannot use the baseset as this also has to work in the bootstrap GUI. */
#define CPSET { 0, 0, 0, 0, 1 }
#define CP___ { 0, 0, 0, 0, 0 }
			static SpriteLoader::CommonPixel builtin_questionmark_data[10 * 8] = {
				CP___, CP___, CPSET, CPSET, CPSET, CPSET, CP___, CP___,
				CP___, CPSET, CPSET, CP___, CP___, CPSET, CPSET, CP___,
				CP___, CP___, CP___, CP___, CP___, CPSET, CPSET, CP___,
				CP___, CP___, CP___, CP___, CPSET, CPSET, CP___, CP___,
				CP___, CP___, CP___, CPSET, CPSET, CP___, CP___, CP___,
				CP___, CP___, CP___, CPSET, CPSET, CP___, CP___, CP___,
				CP___, CP___, CP___, CPSET, CPSET, CP___, CP___, CP___,
				CP___, CP___, CP___, CP___, CP___, CP___, CP___, CP___,
				CP___, CP___, CP___, CPSET, CPSET, CP___, CP___, CP___,
				CP___, CP___, CP___, CPSET, CPSET, CP___, CP___, CP___,
			};
#undef CPSET
#undef CP___
			static const SpriteLoader::SpriteCollection builtin_questionmark = {{ {
				10, // height
				8,  // width
				0,  // x_offs
				0,  // y_offs
				SpriteType::Font,
				SCC_PAL,
				builtin_questionmark_data
			} }};

			Sprite *spr = BlitterFactory::GetCurrentBlitter()->Encode(builtin_questionmark, SimpleSpriteAlloc);
			assert(spr != nullptr);
			GlyphEntry new_glyph;
			new_glyph.sprite = spr;
			new_glyph.width  = spr->width + (this->fs != FS_NORMAL);
			this->SetGlyphPtr(key, &new_glyph, false);
			return new_glyph.sprite;
		} else {
			/* Use '?' for missing characters. */
			this->GetGlyph(question_glyph);
			glyph = this->GetGlyphPtr(question_glyph);
			this->SetGlyphPtr(key, glyph, true);
			return glyph->sprite;
		}
	}

	return this->InternalGetGlyph(key, GetFontAAState(this->fs));
}

const void *TrueTypeFontCache::GetFontTable(uint32_t tag, size_t &length)
{
	const auto iter = this->font_tables.find(tag);
	if (iter != this->font_tables.end()) {
		length = iter->second.first;
		return iter->second.second;
	}

	const void *result = this->InternalGetFontTable(tag, length);

	this->font_tables[tag] = std::pair<size_t, const void *>(length, result);
	return result;
}
