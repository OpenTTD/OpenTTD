/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fontcache.cpp Cache for characters from fonts. */

#include "stdafx.h"
#include "fontcache.h"
#include "fontdetection.h"
#include "blitter/factory.hpp"
#include "core/math_func.hpp"
#include "core/smallmap_type.hpp"
#include "strings_func.h"
#include "zoom_type.h"
#include "gfx_layout.h"
#include "zoom_func.h"

#include "table/sprites.h"
#include "table/control_codes.h"
#include "table/unicode.h"

#include "safeguards.h"

static const int ASCII_LETTERSTART = 32; ///< First printable ASCII letter.
static const int MAX_FONT_SIZE     = 72; ///< Maximum font size.

/** Default heights for the different sizes of fonts. */
static const int _default_font_height[FS_END]   = {10, 6, 18, 10};
static const int _default_font_ascender[FS_END] = { 8, 5, 15,  8};

/**
 * Create a new font cache.
 * @param fs The size of the font.
 */
FontCache::FontCache(FontSize fs) : parent(FontCache::Get(fs)), fs(fs), height(_default_font_height[fs]),
		ascender(_default_font_ascender[fs]), descender(_default_font_ascender[fs] - _default_font_height[fs]),
		units_per_em(1)
{
	assert(this->parent == NULL || this->fs == this->parent->fs);
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
	virtual const void *GetFontTable(uint32 tag, size_t &length) { length = 0; return NULL; }
	virtual const char *GetFontName() { return "sprite"; }
};

/**
 * Create a new sprite font cache.
 * @param fs The font size to create the cache for.
 */
SpriteFontCache::SpriteFontCache(FontSize fs) : FontCache(fs), glyph_to_spriteid_map(NULL)
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

SpriteID SpriteFontCache::GetUnicodeGlyph(GlyphID key)
{
	if (this->glyph_to_spriteid_map[GB(key, 8, 8)] == NULL) return 0;
	return this->glyph_to_spriteid_map[GB(key, 8, 8)][GB(key, 0, 8)];
}

void SpriteFontCache::SetUnicodeGlyph(GlyphID key, SpriteID sprite)
{
	if (this->glyph_to_spriteid_map == NULL) this->glyph_to_spriteid_map = CallocT<SpriteID*>(256);
	if (this->glyph_to_spriteid_map[GB(key, 8, 8)] == NULL) this->glyph_to_spriteid_map[GB(key, 8, 8)] = CallocT<SpriteID>(256);
	this->glyph_to_spriteid_map[GB(key, 8, 8)][GB(key, 0, 8)] = sprite;
}

void SpriteFontCache::InitializeUnicodeGlyphMap()
{
	/* Clear out existing glyph map if it exists */
	this->ClearGlyphToSpriteMap();

	SpriteID base;
	switch (this->fs) {
		default: NOT_REACHED();
		case FS_MONO:   // Use normal as default for mono spaced font, i.e. FALL THROUGH
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
	if (this->glyph_to_spriteid_map == NULL) return;

	for (uint i = 0; i < 256; i++) {
		free(this->glyph_to_spriteid_map[i]);
	}
	free(this->glyph_to_spriteid_map);
	this->glyph_to_spriteid_map = NULL;
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
	return SpriteExists(sprite) ? GetSprite(sprite, ST_FONT)->width + ScaleGUITrad(this->fs != FS_NORMAL ? 1 : 0) : 0;
}

int SpriteFontCache::GetHeight() const
{
	return ScaleGUITrad(this->height);
}

bool SpriteFontCache::GetDrawGlyphShadow()
{
	return false;
}

/* static */ FontCache *FontCache::caches[FS_END] = { new SpriteFontCache(FS_NORMAL), new SpriteFontCache(FS_SMALL), new SpriteFontCache(FS_LARGE), new SpriteFontCache(FS_MONO) };

#ifdef WITH_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H

/** Font cache for fonts that are based on a freetype font. */
class FreeTypeFontCache : public FontCache {
private:
	FT_Face face;  ///< The font face associated with this font.

	typedef SmallMap<uint32, SmallPair<size_t, const void*> > FontTable; ///< Table with font table cache
	FontTable font_tables; ///< Cached font tables.

	/** Container for information about a glyph. */
	struct GlyphEntry {
		Sprite *sprite; ///< The loaded sprite.
		byte width;     ///< The width of the glyph.
		bool duplicate; ///< Whether this glyph entry is a duplicate, i.e. may this be freed?
	};

	/**
	 * The glyph cache. This is structured to reduce memory consumption.
	 * 1) There is a 'segment' table for each font size.
	 * 2) Each segment table is a discrete block of characters.
	 * 3) Each block contains 256 (aligned) characters sequential characters.
	 *
	 * The cache is accessed in the following way:
	 * For character 0x0041  ('A'): glyph_to_sprite[0x00][0x41]
	 * For character 0x20AC (Euro): glyph_to_sprite[0x20][0xAC]
	 *
	 * Currently only 256 segments are allocated, "limiting" us to 65536 characters.
	 * This can be simply changed in the two functions Get & SetGlyphPtr.
	 */
	GlyphEntry **glyph_to_sprite;

	GlyphEntry *GetGlyphPtr(GlyphID key);
	void SetGlyphPtr(GlyphID key, const GlyphEntry *glyph, bool duplicate = false);

public:
	FreeTypeFontCache(FontSize fs, FT_Face face, int pixels);
	~FreeTypeFontCache();
	virtual SpriteID GetUnicodeGlyph(WChar key) { return this->parent->GetUnicodeGlyph(key); }
	virtual void SetUnicodeGlyph(WChar key, SpriteID sprite) { this->parent->SetUnicodeGlyph(key, sprite); }
	virtual void InitializeUnicodeGlyphMap() { this->parent->InitializeUnicodeGlyphMap(); }
	virtual void ClearFontCache();
	virtual const Sprite *GetGlyph(GlyphID key);
	virtual uint GetGlyphWidth(GlyphID key);
	virtual bool GetDrawGlyphShadow();
	virtual GlyphID MapCharToGlyph(WChar key);
	virtual const void *GetFontTable(uint32 tag, size_t &length);
	virtual const char *GetFontName() { return face->family_name; }
};

FT_Library _library = NULL;

FreeTypeSettings _freetype;

static const byte FACE_COLOUR   = 1;
static const byte SHADOW_COLOUR = 2;

/**
 * Create a new FreeTypeFontCache.
 * @param fs     The font size that is going to be cached.
 * @param face   The font that has to be loaded.
 * @param pixels The number of pixels this font should be high.
 */
FreeTypeFontCache::FreeTypeFontCache(FontSize fs, FT_Face face, int pixels) : FontCache(fs), face(face), glyph_to_sprite(NULL)
{
	assert(face != NULL);

	if (pixels == 0) {
		/* Try to determine a good height based on the minimal height recommended by the font. */
		pixels = _default_font_height[this->fs];

		TT_Header *head = (TT_Header *)FT_Get_Sfnt_Table(this->face, ft_sfnt_head);
		if (head != NULL) {
			/* Font height is minimum height plus the difference between the default
			 * height for this font size and the small size. */
			int diff = _default_font_height[this->fs] - _default_font_height[FS_SMALL];
			pixels = Clamp(min(head->Lowest_Rec_PPEM, 20) + diff, _default_font_height[this->fs], MAX_FONT_SIZE);
		}
	}

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
	FreeTypeSubSetting *settings = NULL;
	switch (fs) {
		default: NOT_REACHED();
		case FS_SMALL:  settings = &_freetype.small;  break;
		case FS_NORMAL: settings = &_freetype.medium; break;
		case FS_LARGE:  settings = &_freetype.large;  break;
		case FS_MONO:   settings = &_freetype.mono;   break;
	}

	if (StrEmpty(settings->font)) return;

	if (_library == NULL) {
		if (FT_Init_FreeType(&_library) != FT_Err_Ok) {
			ShowInfoF("Unable to initialize FreeType, using sprite fonts instead");
			return;
		}

		DEBUG(freetype, 2, "Initialized");
	}

	FT_Face face = NULL;
	FT_Error error = FT_New_Face(_library, settings->font, 0, &face);

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

			if (found != NULL) {
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
	this->ClearFontCache();

	for (FontTable::iterator iter = this->font_tables.Begin(); iter != this->font_tables.End(); iter++) {
		free(iter->second.second);
	}
}

/**
 * Reset cached glyphs.
 */
void FreeTypeFontCache::ClearFontCache()
{
	if (this->glyph_to_sprite == NULL) return;

	for (int i = 0; i < 256; i++) {
		if (this->glyph_to_sprite[i] == NULL) continue;

		for (int j = 0; j < 256; j++) {
			if (this->glyph_to_sprite[i][j].duplicate) continue;
			free(this->glyph_to_sprite[i][j].sprite);
		}

		free(this->glyph_to_sprite[i]);
	}

	free(this->glyph_to_sprite);
	this->glyph_to_sprite = NULL;

	Layouter::ResetFontCache(this->fs);
}

FreeTypeFontCache::GlyphEntry *FreeTypeFontCache::GetGlyphPtr(GlyphID key)
{
	if (this->glyph_to_sprite == NULL) return NULL;
	if (this->glyph_to_sprite[GB(key, 8, 8)] == NULL) return NULL;
	return &this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)];
}


void FreeTypeFontCache::SetGlyphPtr(GlyphID key, const GlyphEntry *glyph, bool duplicate)
{
	if (this->glyph_to_sprite == NULL) {
		DEBUG(freetype, 3, "Allocating root glyph cache for size %u", this->fs);
		this->glyph_to_sprite = CallocT<GlyphEntry*>(256);
	}

	if (this->glyph_to_sprite[GB(key, 8, 8)] == NULL) {
		DEBUG(freetype, 3, "Allocating glyph cache for range 0x%02X00, size %u", GB(key, 8, 8), this->fs);
		this->glyph_to_sprite[GB(key, 8, 8)] = CallocT<GlyphEntry>(256);
	}

	DEBUG(freetype, 4, "Set glyph for unicode character 0x%04X, size %u", key, this->fs);
	this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)].sprite    = glyph->sprite;
	this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)].width     = glyph->width;
	this->glyph_to_sprite[GB(key, 8, 8)][GB(key, 0, 8)].duplicate = duplicate;
}

static void *AllocateFont(size_t size)
{
	return MallocT<byte>(size);
}


/* Check if a glyph should be rendered with antialiasing */
static bool GetFontAAState(FontSize size)
{
	/* AA is only supported for 32 bpp */
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 32) return false;

	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return _freetype.medium.aa;
		case FS_SMALL:  return _freetype.small.aa;
		case FS_LARGE:  return _freetype.large.aa;
		case FS_MONO:   return _freetype.mono.aa;
	}
}


const Sprite *FreeTypeFontCache::GetGlyph(GlyphID key)
{
	if ((key & SPRITE_GLYPH) != 0) return this->parent->GetGlyph(key);

	/* Check for the glyph in our cache */
	GlyphEntry *glyph = this->GetGlyphPtr(key);
	if (glyph != NULL && glyph->sprite != NULL) return glyph->sprite;

	FT_GlyphSlot slot = this->face->glyph;

	bool aa = GetFontAAState(this->fs);

	GlyphEntry new_glyph;
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
				builtin_questionmark_data
			};

			Sprite *spr = BlitterFactory::GetCurrentBlitter()->Encode(&builtin_questionmark, AllocateFont);
			assert(spr != NULL);
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
	FT_Load_Glyph(this->face, key, FT_LOAD_DEFAULT);
	FT_Render_Glyph(this->face->glyph, aa ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);

	/* Despite requesting a normal glyph, FreeType may have returned a bitmap */
	aa = (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);

	/* Add 1 pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel */
	uint width  = max(1U, (uint)slot->bitmap.width + (this->fs == FS_NORMAL));
	uint height = max(1U, (uint)slot->bitmap.rows  + (this->fs == FS_NORMAL));

	/* Limit glyph size to prevent overflows later on. */
	if (width > 256 || height > 256) usererror("Font glyph is too large");

	/* FreeType has rendered the glyph, now we allocate a sprite and copy the image into it */
	SpriteLoader::Sprite sprite;
	sprite.AllocateData(ZOOM_LVL_NORMAL, width * height);
	sprite.type = ST_FONT;
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

	new_glyph.sprite = BlitterFactory::GetCurrentBlitter()->Encode(&sprite, AllocateFont);
	new_glyph.width  = slot->advance.x >> 6;

	this->SetGlyphPtr(key, &new_glyph);

	return new_glyph.sprite;
}


bool FreeTypeFontCache::GetDrawGlyphShadow()
{
	return this->fs == FS_NORMAL && GetFontAAState(FS_NORMAL);
}


uint FreeTypeFontCache::GetGlyphWidth(GlyphID key)
{
	if ((key & SPRITE_GLYPH) != 0) return this->parent->GetGlyphWidth(key);

	GlyphEntry *glyph = this->GetGlyphPtr(key);
	if (glyph == NULL || glyph->sprite == NULL) {
		this->GetGlyph(key);
		glyph = this->GetGlyphPtr(key);
	}

	return glyph->width;
}

GlyphID FreeTypeFontCache::MapCharToGlyph(WChar key)
{
	assert(IsPrintable(key));

	if (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END) {
		return this->parent->MapCharToGlyph(key);
	}

	return FT_Get_Char_Index(this->face, key);
}

const void *FreeTypeFontCache::GetFontTable(uint32 tag, size_t &length)
{
	const FontTable::iterator iter = this->font_tables.Find(tag);
	if (iter != this->font_tables.End()) {
		length = iter->second.first;
		return iter->second.second;
	}

	FT_ULong len = 0;
	FT_Byte *result = NULL;

	FT_Load_Sfnt_Table(this->face, tag, 0, NULL, &len);

	if (len > 0) {
		result = MallocT<FT_Byte>(len);
		FT_Load_Sfnt_Table(this->face, tag, 0, result, &len);
	}
	length = len;

	this->font_tables.Insert(tag, SmallPair<size_t, const void *>(length, result));
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
	_library = NULL;
#endif /* WITH_FREETYPE */
}
