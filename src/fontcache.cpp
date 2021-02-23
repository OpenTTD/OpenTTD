/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fontcache.cpp Cache for characters from fonts. */

#include "stdafx.h"
#include "fontcache.h"
#include "fontcache_internal.h"
#include "fontdetection.h"
#include "blitter/factory.hpp"
#include "core/math_func.hpp"
#include "core/smallmap_type.hpp"
#include "strings_func.h"
#include "zoom_type.h"
#include "gfx_layout.h"
#include "zoom_func.h"
#include "fileio_func.h"

#include "table/sprites.h"
#include "table/control_codes.h"
#include "table/unicode.h"

#include "safeguards.h"

static const int ASCII_LETTERSTART = 32; ///< First printable ASCII letter.

/** Default heights for the different sizes of fonts. */
static const int _default_font_height[FS_END]   = {10, 6, 18, 10};
static const int _default_font_ascender[FS_END] = { 8, 5, 15,  8};

FreeTypeSettings _freetype;

/**
 * Create a new font cache.
 * @param fs The size of the font.
 */
FontCache::FontCache(FontSize fs) : parent(FontCache::Get(fs)), fs(fs), height(_default_font_height[fs]),
		ascender(_default_font_ascender[fs]), descender(_default_font_ascender[fs] - _default_font_height[fs]),
		units_per_em(1)
{
	assert(this->parent == nullptr || this->fs == this->parent->fs);
	FontCache::caches[this->fs] = this;
	Layouter::ResetFontCache(this->fs);
}

/** Clean everything up. */
FontCache::~FontCache()
{
	assert(this->fs == this->parent->fs);
	FontCache::caches[this->fs] = this->parent;
	Layouter::ResetFontCache(this->fs);
}

int FontCache::GetDefaultFontHeight(FontSize fs)
{
	return _default_font_height[fs];
}


/**
 * Get height of a character for a given font size.
 * @param size Font size to get height of
 * @return     Height of characters in the given font (pixels)
 */
int GetCharacterHeight(FontSize size)
{
	return FontCache::Get(size)->GetHeight();
}


/** Font cache for fonts that are based on a freetype font. */
class SpriteFontCache : public FontCache {
private:
	SpriteID **glyph_to_spriteid_map; ///< Mapping of glyphs to sprite IDs.

	void ClearGlyphToSpriteMap();
public:
	SpriteFontCache(FontSize fs);
	~SpriteFontCache();
	virtual SpriteID GetUnicodeGlyph(WChar key);
	virtual void SetUnicodeGlyph(WChar key, SpriteID sprite);
	virtual void InitializeUnicodeGlyphMap();
	virtual void ClearFontCache();
	virtual const Sprite *GetGlyph(GlyphID key);
	virtual uint GetGlyphWidth(GlyphID key);
	virtual int GetHeight() const;
	virtual bool GetDrawGlyphShadow();
	virtual GlyphID MapCharToGlyph(WChar key) { assert(IsPrintable(key)); return SPRITE_GLYPH | key; }
	virtual const void *GetFontTable(uint32 tag, size_t &length) { length = 0; return nullptr; }
	virtual const char *GetFontName() { return "sprite"; }
	virtual bool IsBuiltInFont() { return true; }
};

/**
 * Create a new sprite font cache.
 * @param fs The font size to create the cache for.
 */
SpriteFontCache::SpriteFontCache(FontSize fs) : FontCache(fs), glyph_to_spriteid_map(nullptr)
{
	this->InitializeUnicodeGlyphMap();
}

/**
 * Free everything we allocated.
 */
SpriteFontCache::~SpriteFontCache()
{
	this->ClearGlyphToSpriteMap();
}

SpriteID SpriteFontCache::GetUnicodeGlyph(WChar key)
{
	if (this->glyph_to_spriteid_map[GB(key, 8, 8)] == nullptr) return 0;
	return this->glyph_to_spriteid_map[GB(key, 8, 8)][GB(key, 0, 8)];
}

void SpriteFontCache::SetUnicodeGlyph(WChar key, SpriteID sprite)
{
	if (this->glyph_to_spriteid_map == nullptr) this->glyph_to_spriteid_map = CallocT<SpriteID*>(256);
	if (this->glyph_to_spriteid_map[GB(key, 8, 8)] == nullptr) this->glyph_to_spriteid_map[GB(key, 8, 8)] = CallocT<SpriteID>(256);
	this->glyph_to_spriteid_map[GB(key, 8, 8)][GB(key, 0, 8)] = sprite;
}

void SpriteFontCache::InitializeUnicodeGlyphMap()
{
	/* Clear out existing glyph map if it exists */
	this->ClearGlyphToSpriteMap();

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

	for (uint i = 0; i < lengthof(_default_unicode_map); i++) {
		byte key = _default_unicode_map[i].key;
		if (key == CLRA) {
			/* Clear the glyph. This happens if the glyph at this code point
			 * is non-standard and should be accessed by an SCC_xxx enum
			 * entry only. */
			this->SetUnicodeGlyph(_default_unicode_map[i].code, 0);
		} else {
			SpriteID sprite = base + key - ASCII_LETTERSTART;
			this->SetUnicodeGlyph(_default_unicode_map[i].code, sprite);
		}
	}
}

/**
 * Clear the glyph to sprite mapping.
 */
void SpriteFontCache::ClearGlyphToSpriteMap()
{
	if (this->glyph_to_spriteid_map == nullptr) return;

	for (uint i = 0; i < 256; i++) {
		free(this->glyph_to_spriteid_map[i]);
	}
	free(this->glyph_to_spriteid_map);
	this->glyph_to_spriteid_map = nullptr;
}

void SpriteFontCache::ClearFontCache()
{
	Layouter::ResetFontCache(this->fs);
}

const Sprite *SpriteFontCache::GetGlyph(GlyphID key)
{
	SpriteID sprite = this->GetUnicodeGlyph(key);
	if (sprite == 0) sprite = this->GetUnicodeGlyph('?');
	return GetSprite(sprite, ST_FONT);
}

uint SpriteFontCache::GetGlyphWidth(GlyphID key)
{
	SpriteID sprite = this->GetUnicodeGlyph(key);
	if (sprite == 0) sprite = this->GetUnicodeGlyph('?');
	return SpriteExists(sprite) ? GetSprite(sprite, ST_FONT)->width + ScaleFontTrad(this->fs != FS_NORMAL ? 1 : 0) : 0;
}

int SpriteFontCache::GetHeight() const
{
	return ScaleFontTrad(this->height);
}

bool SpriteFontCache::GetDrawGlyphShadow()
{
	return false;
}

/* static */ FontCache *FontCache::caches[FS_END] = { new SpriteFontCache(FS_NORMAL), new SpriteFontCache(FS_SMALL), new SpriteFontCache(FS_LARGE), new SpriteFontCache(FS_MONO) };


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
	this->ClearFontCache();

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
		DEBUG(freetype, 3, "Allocating root glyph cache for size %u", this->fs);
		this->glyph_to_sprite = CallocT<GlyphEntry*>(256);
	}

	if (this->glyph_to_sprite[GB(key, 8, 8)] == nullptr) {
		DEBUG(freetype, 3, "Allocating glyph cache for range 0x%02X00, size %u", GB(key, 8, 8), this->fs);
		this->glyph_to_sprite[GB(key, 8, 8)] = CallocT<GlyphEntry>(256);
	}

	DEBUG(freetype, 4, "Set glyph for unicode character 0x%04X, size %u", key, this->fs);
	this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)].sprite = glyph->sprite;
	this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)].width = glyph->width;
	this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)].duplicate = duplicate;
}


/* Check if a glyph should be rendered with anti-aliasing. */
static bool GetFontAAState(FontSize size, bool check_blitter = true)
{
	/* AA is only supported for 32 bpp */
	if (check_blitter && BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 32) return false;

	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return _freetype.medium.aa;
		case FS_SMALL:  return _freetype.small.aa;
		case FS_LARGE:  return _freetype.large.aa;
		case FS_MONO:   return _freetype.mono.aa;
	}
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
			static const SpriteLoader::Sprite builtin_questionmark = {
				10, // height
				8,  // width
				0,  // x_offs
				0,  // y_offs
				ST_FONT,
				SCC_PAL,
				builtin_questionmark_data
			};

			Sprite *spr = BlitterFactory::GetCurrentBlitter()->Encode(&builtin_questionmark, SimpleSpriteAlloc);
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

const void *TrueTypeFontCache::GetFontTable(uint32 tag, size_t &length)
{
	const FontTable::iterator iter = this->font_tables.Find(tag);
	if (iter != this->font_tables.data() + this->font_tables.size()) {
		length = iter->second.first;
		return iter->second.second;
	}

	const void *result = this->InternalGetFontTable(tag, length);

	this->font_tables.Insert(tag, std::pair<size_t, const void *>(length, result));
	return result;
}


#ifdef WITH_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H

/** Font cache for fonts that are based on a freetype font. */
class FreeTypeFontCache : public TrueTypeFontCache {
private:
	FT_Face face;  ///< The font face associated with this font.

	void SetFontSize(FontSize fs, FT_Face face, int pixels);
	virtual const void *InternalGetFontTable(uint32 tag, size_t &length);
	virtual const Sprite *InternalGetGlyph(GlyphID key, bool aa);

public:
	FreeTypeFontCache(FontSize fs, FT_Face face, int pixels);
	~FreeTypeFontCache();
	virtual void ClearFontCache();
	virtual GlyphID MapCharToGlyph(WChar key);
	virtual const char *GetFontName() { return face->family_name; }
	virtual bool IsBuiltInFont() { return false; }
};

FT_Library _library = nullptr;


/**
 * Create a new FreeTypeFontCache.
 * @param fs     The font size that is going to be cached.
 * @param face   The font that has to be loaded.
 * @param pixels The number of pixels this font should be high.
 */
FreeTypeFontCache::FreeTypeFontCache(FontSize fs, FT_Face face, int pixels) : TrueTypeFontCache(fs, pixels), face(face)
{
	assert(face != nullptr);

	this->SetFontSize(fs, face, pixels);
}

void FreeTypeFontCache::SetFontSize(FontSize fs, FT_Face face, int pixels)
{
	if (pixels == 0) {
		/* Try to determine a good height based on the minimal height recommended by the font. */
		int scaled_height = ScaleFontTrad(_default_font_height[this->fs]);
		pixels = scaled_height;

		TT_Header *head = (TT_Header *)FT_Get_Sfnt_Table(this->face, ft_sfnt_head);
		if (head != nullptr) {
			/* Font height is minimum height plus the difference between the default
			 * height for this font size and the small size. */
			int diff = scaled_height - ScaleFontTrad(_default_font_height[FS_SMALL]);
			pixels = Clamp(std::min<uint>(head->Lowest_Rec_PPEM, MAX_FONT_MIN_REC_SIZE) + diff, scaled_height, MAX_FONT_SIZE);
		}
	} else {
		pixels = ScaleFontTrad(pixels);
	}
	this->used_size = pixels;

	FT_Error err = FT_Set_Pixel_Sizes(this->face, 0, pixels);
	if (err != FT_Err_Ok) {

		/* Find nearest size to that requested */
		FT_Bitmap_Size *bs = this->face->available_sizes;
		int i = this->face->num_fixed_sizes;
		if (i > 0) { // In pathetic cases one might get no fixed sizes at all.
			int n = bs->height;
			FT_Int chosen = 0;
			for (; --i; bs++) {
				if (abs(pixels - bs->height) >= abs(pixels - n)) continue;
				n = bs->height;
				chosen = this->face->num_fixed_sizes - i;
			}

			/* Don't use FT_Set_Pixel_Sizes here - it might give us another
			 * error, even though the size is available (FS#5885). */
			err = FT_Select_Size(this->face, chosen);
		}
	}

	if (err == FT_Err_Ok) {
		this->units_per_em = this->face->units_per_EM;
		this->ascender     = this->face->size->metrics.ascender >> 6;
		this->descender    = this->face->size->metrics.descender >> 6;
		this->height       = this->ascender - this->descender;
	} else {
		/* Both FT_Set_Pixel_Sizes and FT_Select_Size failed. */
		DEBUG(freetype, 0, "Font size selection failed. Using FontCache defaults.");
	}
}

/**
 * Loads the freetype font.
 * First type to load the fontname as if it were a path. If that fails,
 * try to resolve the filename of the font using fontconfig, where the
 * format is 'font family name' or 'font family name, font style'.
 * @param fs The font size to load.
 */
static void LoadFreeTypeFont(FontSize fs)
{
	FreeTypeSubSetting *settings = nullptr;
	switch (fs) {
		default: NOT_REACHED();
		case FS_SMALL:  settings = &_freetype.small;  break;
		case FS_NORMAL: settings = &_freetype.medium; break;
		case FS_LARGE:  settings = &_freetype.large;  break;
		case FS_MONO:   settings = &_freetype.mono;   break;
	}

	if (StrEmpty(settings->font)) return;

	if (_library == nullptr) {
		if (FT_Init_FreeType(&_library) != FT_Err_Ok) {
			ShowInfoF("Unable to initialize FreeType, using sprite fonts instead");
			return;
		}

		DEBUG(freetype, 2, "Initialized");
	}

	FT_Face face = nullptr;

	/* If font is an absolute path to a ttf, try loading that first. */
	FT_Error error = FT_New_Face(_library, settings->font, 0, &face);

#if defined(WITH_COCOA)
	extern void MacOSRegisterExternalFont(const char *file_path);
	if (error == FT_Err_Ok) MacOSRegisterExternalFont(settings->font);
#endif

	if (error != FT_Err_Ok) {
		/* Check if font is a relative filename in one of our search-paths. */
		std::string full_font = FioFindFullPath(BASE_DIR, settings->font);
		if (!full_font.empty()) {
			error = FT_New_Face(_library, full_font.c_str(), 0, &face);
#if defined(WITH_COCOA)
			if (error == FT_Err_Ok) MacOSRegisterExternalFont(full_font.c_str());
#endif
		}
	}

	/* Try loading based on font face name (OS-wide fonts). */
	if (error != FT_Err_Ok) error = GetFontByFaceName(settings->font, &face);

	if (error == FT_Err_Ok) {
		DEBUG(freetype, 2, "Requested '%s', using '%s %s'", settings->font, face->family_name, face->style_name);

		/* Attempt to select the unicode character map */
		error = FT_Select_Charmap(face, ft_encoding_unicode);
		if (error == FT_Err_Ok) goto found_face; // Success

		if (error == FT_Err_Invalid_CharMap_Handle) {
			/* Try to pick a different character map instead. We default to
			 * the first map, but platform_id 0 encoding_id 0 should also
			 * be unicode (strange system...) */
			FT_CharMap found = face->charmaps[0];
			int i;

			for (i = 0; i < face->num_charmaps; i++) {
				FT_CharMap charmap = face->charmaps[i];
				if (charmap->platform_id == 0 && charmap->encoding_id == 0) {
					found = charmap;
				}
			}

			if (found != nullptr) {
				error = FT_Set_Charmap(face, found);
				if (error == FT_Err_Ok) goto found_face;
			}
		}
	}

	FT_Done_Face(face);

	static const char *SIZE_TO_NAME[] = { "medium", "small", "large", "mono" };
	ShowInfoF("Unable to use '%s' for %s font, FreeType reported error 0x%X, using sprite font instead", settings->font, SIZE_TO_NAME[fs], error);
	return;

found_face:
	new FreeTypeFontCache(fs, face, settings->size);
}


/**
 * Free everything that was allocated for this font cache.
 */
FreeTypeFontCache::~FreeTypeFontCache()
{
	FT_Done_Face(this->face);
	this->face = nullptr;
	this->ClearFontCache();
}

/**
 * Reset cached glyphs.
 */
void FreeTypeFontCache::ClearFontCache()
{
	/* Font scaling might have changed, determine font size anew if it was automatically selected. */
	if (this->face != nullptr) this->SetFontSize(this->fs, this->face, this->req_size);

	this->TrueTypeFontCache::ClearFontCache();
}


const Sprite *FreeTypeFontCache::InternalGetGlyph(GlyphID key, bool aa)
{
	FT_GlyphSlot slot = this->face->glyph;

	FT_Load_Glyph(this->face, key, aa ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO);
	FT_Render_Glyph(this->face->glyph, aa ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);

	/* Despite requesting a normal glyph, FreeType may have returned a bitmap */
	aa = (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);

	/* Add 1 pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel */
	uint width  = std::max(1U, (uint)slot->bitmap.width + (this->fs == FS_NORMAL));
	uint height = std::max(1U, (uint)slot->bitmap.rows  + (this->fs == FS_NORMAL));

	/* Limit glyph size to prevent overflows later on. */
	if (width > MAX_GLYPH_DIM || height > MAX_GLYPH_DIM) usererror("Font glyph is too large");

	/* FreeType has rendered the glyph, now we allocate a sprite and copy the image into it */
	SpriteLoader::Sprite sprite;
	sprite.AllocateData(ZOOM_LVL_NORMAL, width * height);
	sprite.type = ST_FONT;
	sprite.colours = (aa ? SCC_PAL | SCC_ALPHA : SCC_PAL);
	sprite.width = width;
	sprite.height = height;
	sprite.x_offs = slot->bitmap_left;
	sprite.y_offs = this->ascender - slot->bitmap_top;

	/* Draw shadow for medium size */
	if (this->fs == FS_NORMAL && !aa) {
		for (uint y = 0; y < (uint)slot->bitmap.rows; y++) {
			for (uint x = 0; x < (uint)slot->bitmap.width; x++) {
				if (aa ? (slot->bitmap.buffer[x + y * slot->bitmap.pitch] > 0) : HasBit(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
					sprite.data[1 + x + (1 + y) * sprite.width].m = SHADOW_COLOUR;
					sprite.data[1 + x + (1 + y) * sprite.width].a = aa ? slot->bitmap.buffer[x + y * slot->bitmap.pitch] : 0xFF;
				}
			}
		}
	}

	for (uint y = 0; y < (uint)slot->bitmap.rows; y++) {
		for (uint x = 0; x < (uint)slot->bitmap.width; x++) {
			if (aa ? (slot->bitmap.buffer[x + y * slot->bitmap.pitch] > 0) : HasBit(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
				sprite.data[x + y * sprite.width].m = FACE_COLOUR;
				sprite.data[x + y * sprite.width].a = aa ? slot->bitmap.buffer[x + y * slot->bitmap.pitch] : 0xFF;
			}
		}
	}

	GlyphEntry new_glyph;
	new_glyph.sprite = BlitterFactory::GetCurrentBlitter()->Encode(&sprite, SimpleSpriteAlloc);
	new_glyph.width  = slot->advance.x >> 6;

	this->SetGlyphPtr(key, &new_glyph);

	return new_glyph.sprite;
}


GlyphID FreeTypeFontCache::MapCharToGlyph(WChar key)
{
	assert(IsPrintable(key));

	if (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END) {
		return this->parent->MapCharToGlyph(key);
	}

	return FT_Get_Char_Index(this->face, key);
}

const void *FreeTypeFontCache::InternalGetFontTable(uint32 tag, size_t &length)
{
	FT_ULong len = 0;
	FT_Byte *result = nullptr;

	FT_Load_Sfnt_Table(this->face, tag, 0, nullptr, &len);

	if (len > 0) {
		result = MallocT<FT_Byte>(len);
		FT_Load_Sfnt_Table(this->face, tag, 0, result, &len);
	}

	length = len;
	return result;
}
#endif /* WITH_FREETYPE */


/**
 * (Re)initialize the freetype related things, i.e. load the non-sprite fonts.
 * @param monospace Whether to initialise the monospace or regular fonts.
 */
void InitFreeType(bool monospace)
{
	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		if (monospace != (fs == FS_MONO)) continue;

		FontCache *fc = FontCache::Get(fs);
		if (fc->HasParent()) delete fc;

#ifdef WITH_FREETYPE
		LoadFreeTypeFont(fs);
#elif defined(_WIN32)
		extern void LoadWin32Font(FontSize fs);
		LoadWin32Font(fs);
#elif defined(WITH_COCOA)
		extern void LoadCoreTextFont(FontSize fs);
		LoadCoreTextFont(fs);
#endif
	}
}

/**
 * Free everything allocated w.r.t. fonts.
 */
void UninitFreeType()
{
	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		FontCache *fc = FontCache::Get(fs);
		if (fc->HasParent()) delete fc;
	}

#ifdef WITH_FREETYPE
	FT_Done_FreeType(_library);
	_library = nullptr;
#endif /* WITH_FREETYPE */
}

/**
 * Should any of the active fonts be anti-aliased?
 * @return True if any of the loaded fonts want anti-aliased drawing.
 */
bool HasAntialiasedFonts()
{
	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		if (!FontCache::Get(fs)->IsBuiltInFont() && GetFontAAState(fs, false)) return true;
	}

	return false;
}

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(WITH_FONTCONFIG) && !defined(WITH_COCOA)

#ifdef WITH_FREETYPE
FT_Error GetFontByFaceName(const char *font_name, FT_Face *face) { return FT_Err_Cannot_Open_Resource; }
#endif /* WITH_FREETYPE */

bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, MissingGlyphSearcher *callback) { return false; }
#endif /* !defined(_WIN32) && !defined(__APPLE__) && !defined(WITH_FONTCONFIG) && !defined(WITH_COCOA) */
