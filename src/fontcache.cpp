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
#include "strings_func.h"
#include "zoom_type.h"

#include "table/sprites.h"
#include "table/control_codes.h"

static const int ASCII_LETTERSTART = 32; ///< First printable ASCII letter.
static const int MAX_FONT_SIZE     = 72; ///< Maximum font size.

/** Semi-constant for the height of the different sizes of fonts. */
int _font_height[FS_END];
/** Default heights for the different sizes of fonts. */
static const int _default_font_height[FS_END] = {10, 6, 18, 10};

/**
 * Reset the font sizes to the defaults of the sprite based fonts.
 * @param monospace Whether to reset the monospace or regular fonts.
 */
void ResetFontSizes(bool monospace)
{
	if (monospace) {
		_font_height[FS_MONO]   = _default_font_height[FS_MONO];
	} else {
		_font_height[FS_SMALL]  = _default_font_height[FS_SMALL];
		_font_height[FS_NORMAL] = _default_font_height[FS_NORMAL];
		_font_height[FS_LARGE]  = _default_font_height[FS_LARGE];
	}
}

/**
 * Create a new font cache.
 * @param fs The size of the font.
 */
FontCache::FontCache(FontSize fs) : parent(FontCache::Get(fs)), fs(fs)
{
	assert(parent == NULL || this->fs == parent->fs);
	FontCache::caches[this->fs] = this;
}

/** Clean everything up. */
FontCache::~FontCache()
{
	assert(this->fs == parent->fs);
	FontCache::caches[this->fs] = this->parent;
}

/** Font cache for fonts that are based on a freetype font. */
class SpriteFontCache : public FontCache {
private:
	SpriteID **glyph_to_spriteid_map; ///< Mapping of glyphs to sprite IDs.

	void ClearGlyphToSpriteMap();
public:
	SpriteFontCache(FontSize fs);
	~SpriteFontCache();
	virtual SpriteID GetUnicodeGlyph(uint32 key);
	virtual void SetUnicodeGlyph(uint32 key, SpriteID sprite);
	virtual void InitializeUnicodeGlyphMap();
	virtual void ClearFontCache();
	virtual const Sprite *GetGlyph(uint32 key);
	virtual uint GetGlyphWidth(uint32 key);
	virtual bool GetDrawGlyphShadow();
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

void SpriteFontCache::ClearFontCache() {}

const Sprite *SpriteFontCache::GetGlyph(uint32 key)
{
	SpriteID sprite = this->GetUnicodeGlyph(key);
	if (sprite == 0) sprite = this->GetUnicodeGlyph('?');
	return GetSprite(sprite, ST_FONT);
}

uint SpriteFontCache::GetGlyphWidth(uint32 key)
{
	SpriteID sprite = this->GetUnicodeGlyph(key);
	if (sprite == 0) sprite = this->GetUnicodeGlyph('?');
	return SpriteExists(sprite) ? GetSprite(sprite, ST_FONT)->width + (this->fs != FS_NORMAL) : 0;
}

bool SpriteFontCache::GetDrawGlyphShadow()
{
	return false;
}

/*static */ FontCache *FontCache::caches[FS_END] = { new SpriteFontCache(FS_NORMAL), new SpriteFontCache(FS_SMALL), new SpriteFontCache(FS_LARGE), new SpriteFontCache(FS_MONO) };

#ifdef WITH_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H

/** Font cache for fonts that are based on a freetype font. */
class FreeTypeFontCache : public FontCache {
private:
	FT_Face face;  ///< The font face associated with this font.
public:
	FreeTypeFontCache(FontSize fs, FT_Face face, int pixels);
	~FreeTypeFontCache();
	virtual SpriteID GetUnicodeGlyph(uint32 key) { return this->parent->GetUnicodeGlyph(key); }
	virtual void SetUnicodeGlyph(uint32 key, SpriteID sprite) { this->parent->SetUnicodeGlyph(key, sprite); }
	virtual void InitializeUnicodeGlyphMap() { this->parent->InitializeUnicodeGlyphMap(); }
	virtual void ClearFontCache();
	virtual const Sprite *GetGlyph(uint32 key);
	virtual uint GetGlyphWidth(uint32 key);
	virtual bool GetDrawGlyphShadow();
};

FT_Library _library = NULL;
static int _ascender[FS_END];

FreeTypeSettings _freetype;

static const byte FACE_COLOUR   = 1;
static const byte SHADOW_COLOUR = 2;

/**
 * Create a new FreeTypeFontCache.
 * @param fs     The font size that is going to be cached.
 * @param face   The font that has to be loaded.
 * @param pixels The number of pixels this font should be high.
 */
FreeTypeFontCache::FreeTypeFontCache(FontSize fs, FT_Face face, int pixels) : FontCache(fs), face(face)
{
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
	if (err == FT_Err_Invalid_Pixel_Size) {

		/* Find nearest size to that requested */
		FT_Bitmap_Size *bs = this->face->available_sizes;
		int i = this->face->num_fixed_sizes;
		int n = bs->height;
		for (; --i; bs++) {
			if (abs(pixels - bs->height) < abs(pixels - n)) n = bs->height;
		}

		FT_Set_Pixel_Sizes(this->face, 0, n);
	}

	int asc = this->face->size->metrics.ascender >> 6;
	int dec = this->face->size->metrics.descender >> 6;

	_ascender[this->fs] = asc;
	_font_height[this->fs] = asc - dec;
}

/**
 * Loads the freetype font.
 * First type to load the fontname as if it were a path. If that fails,
 * try to resolve the filename of the font using fontconfig, where the
 * format is 'font family name' or 'font family name, font style'.
 */
static void LoadFreeTypeFont(const char *font_name, FT_Face *face, const char *type)
{
	FT_Error error;

	if (StrEmpty(font_name)) return;

	error = FT_New_Face(_library, font_name, 0, face);

	if (error != FT_Err_Ok) error = GetFontByFaceName(font_name, face);

	if (error == FT_Err_Ok) {
		DEBUG(freetype, 2, "Requested '%s', using '%s %s'", font_name, (*face)->family_name, (*face)->style_name);

		/* Attempt to select the unicode character map */
		error = FT_Select_Charmap(*face, ft_encoding_unicode);
		if (error == FT_Err_Ok) return; // Success

		if (error == FT_Err_Invalid_CharMap_Handle) {
			/* Try to pick a different character map instead. We default to
			 * the first map, but platform_id 0 encoding_id 0 should also
			 * be unicode (strange system...) */
			FT_CharMap found = (*face)->charmaps[0];
			int i;

			for (i = 0; i < (*face)->num_charmaps; i++) {
				FT_CharMap charmap = (*face)->charmaps[i];
				if (charmap->platform_id == 0 && charmap->encoding_id == 0) {
					found = charmap;
				}
			}

			if (found != NULL) {
				error = FT_Set_Charmap(*face, found);
				if (error == FT_Err_Ok) return;
			}
		}
	}

	FT_Done_Face(*face);
	*face = NULL;

	ShowInfoF("Unable to use '%s' for %s font, FreeType reported error 0x%X, using sprite font instead", font_name, type, error);
}


static void ResetGlyphCache(bool monospace);

/**
 * Unload the face
 */
FreeTypeFontCache::~FreeTypeFontCache()
{
	if (this->face != NULL) FT_Done_Face(this->face);
}

/**
 * (Re)initialize the freetype related things, i.e. load the non-sprite fonts.
 * @param monospace Whether to initialise the monospace or regular fonts.
 */
void InitFreeType(bool monospace)
{
	ResetFontSizes(monospace);
	ResetGlyphCache(monospace);

	if (StrEmpty(_freetype.small.font) && StrEmpty(_freetype.medium.font) && StrEmpty(_freetype.large.font) && StrEmpty(_freetype.mono.font)) {
		DEBUG(freetype, 1, "No font faces specified, using sprite fonts instead");
		return;
	}

	if (_library == NULL) {
		if (FT_Init_FreeType(&_library) != FT_Err_Ok) {
			ShowInfoF("Unable to initialize FreeType, using sprite fonts instead");
			return;
		}

		DEBUG(freetype, 2, "Initialized");
	}

	/* Load each font */
	if (monospace) {
		FT_Face mono = NULL;
		LoadFreeTypeFont(_freetype.mono.font ,  &mono,   "mono");

		if (mono != NULL) new FreeTypeFontCache(FS_MONO, mono, _freetype.mono.size);
	} else {
		FT_Face small  = NULL;
		FT_Face medium = NULL;
		FT_Face large  = NULL;

		LoadFreeTypeFont(_freetype.small.font,  &small,  "small");
		LoadFreeTypeFont(_freetype.medium.font, &medium, "medium");
		LoadFreeTypeFont(_freetype.large.font,  &large,  "large");

		/* Set each font size */
		if (small != NULL)  new FreeTypeFontCache(FS_SMALL,  small,  _freetype.small.size);
		if (medium != NULL) new FreeTypeFontCache(FS_NORMAL, medium, _freetype.medium.size);
		if (large != NULL)  new FreeTypeFontCache(FS_LARGE,  large,  _freetype.large.size);
	}
}

/**
 * Free everything allocated w.r.t. fonts.
 */
void UninitFreeType()
{
	ResetFontSizes(true);
	ResetFontSizes(false);
	ClearFontCache();

	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		FontCache *fc = FontCache::Get(fs);
		if (fc->HasParent()) delete fc;
	}

	FT_Done_FreeType(_library);
	_library = NULL;
}

/**
 * Reset cached glyphs.
 */
void FreeTypeFontCache::ClearFontCache()
{
	ResetGlyphCache(true);
	ResetGlyphCache(false);
}

struct GlyphEntry {
	Sprite *sprite;
	byte width;
	bool duplicate;
};


/* The glyph cache. This is structured to reduce memory consumption.
 * 1) There is a 'segment' table for each font size.
 * 2) Each segment table is a discrete block of characters.
 * 3) Each block contains 256 (aligned) characters sequential characters.
 *
 * The cache is accessed in the following way:
 * For character 0x0041  ('A'): _glyph_ptr[FS_NORMAL][0x00][0x41]
 * For character 0x20AC (Euro): _glyph_ptr[FS_NORMAL][0x20][0xAC]
 *
 * Currently only 256 segments are allocated, "limiting" us to 65536 characters.
 * This can be simply changed in the two functions Get & SetGlyphPtr.
 */
static GlyphEntry **_glyph_ptr[FS_END];

/**
 * Clear the complete cache
 * @param monospace Whether to reset the monospace or regular font.
 */
static void ResetGlyphCache(bool monospace)
{
	for (FontSize i = FS_BEGIN; i < FS_END; i++) {
		if (monospace != (i == FS_MONO)) continue;
		if (_glyph_ptr[i] == NULL) continue;

		for (int j = 0; j < 256; j++) {
			if (_glyph_ptr[i][j] == NULL) continue;

			for (int k = 0; k < 256; k++) {
				if (_glyph_ptr[i][j][k].duplicate) continue;
				free(_glyph_ptr[i][j][k].sprite);
			}

			free(_glyph_ptr[i][j]);
		}

		free(_glyph_ptr[i]);
		_glyph_ptr[i] = NULL;
	}
}

static GlyphEntry *GetGlyphPtr(FontSize size, WChar key)
{
	if (_glyph_ptr[size] == NULL) return NULL;
	if (_glyph_ptr[size][GB(key, 8, 8)] == NULL) return NULL;
	return &_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)];
}


static void SetGlyphPtr(FontSize size, WChar key, const GlyphEntry *glyph, bool duplicate = false)
{
	if (_glyph_ptr[size] == NULL) {
		DEBUG(freetype, 3, "Allocating root glyph cache for size %u", size);
		_glyph_ptr[size] = CallocT<GlyphEntry*>(256);
	}

	if (_glyph_ptr[size][GB(key, 8, 8)] == NULL) {
		DEBUG(freetype, 3, "Allocating glyph cache for range 0x%02X00, size %u", GB(key, 8, 8), size);
		_glyph_ptr[size][GB(key, 8, 8)] = CallocT<GlyphEntry>(256);
	}

	DEBUG(freetype, 4, "Set glyph for unicode character 0x%04X, size %u", key, size);
	_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)].sprite    = glyph->sprite;
	_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)].width     = glyph->width;
	_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)].duplicate = duplicate;
}

static void *AllocateFont(size_t size)
{
	return MallocT<byte>(size);
}


/* Check if a glyph should be rendered with antialiasing */
static bool GetFontAAState(FontSize size)
{
	/* AA is only supported for 32 bpp */
	if (BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth() != 32) return false;

	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return _freetype.medium.aa;
		case FS_SMALL:  return _freetype.small.aa;
		case FS_LARGE:  return _freetype.large.aa;
		case FS_MONO:   return _freetype.mono.aa;
	}
}


const Sprite *FreeTypeFontCache::GetGlyph(WChar key)
{
	FT_GlyphSlot slot;
	GlyphEntry new_glyph;
	GlyphEntry *glyph;
	SpriteLoader::Sprite sprite;
	int width;
	int height;
	int x;
	int y;

	assert(IsPrintable(key));

	/* Bail out if no face loaded, or for our special characters */
	if (this->face == NULL || (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END)) {
		SpriteID sprite = this->GetUnicodeGlyph(key);
		if (sprite == 0) sprite = this->GetUnicodeGlyph('?');

		/* Load the sprite if it's known. */
		if (sprite != 0) return GetSprite(sprite, ST_FONT);

		/* For the 'rare' case there is no font available at all. */
		if (this->face == NULL) error("No sprite font and no real font either... bailing!");

		/* Use the '?' from the freetype font. */
		key = '?';
	}

	/* Check for the glyph in our cache */
	glyph = GetGlyphPtr(this->fs, key);
	if (glyph != NULL && glyph->sprite != NULL) return glyph->sprite;

	slot = this->face->glyph;

	bool aa = GetFontAAState(this->fs);

	FT_UInt glyph_index = FT_Get_Char_Index(this->face, key);
	if (glyph_index == 0) {
		if (key == '?') {
			/* The font misses the '?' character. Use sprite font. */
			SpriteID sprite = this->GetUnicodeGlyph(key);
			Sprite *spr = (Sprite*)GetRawSprite(sprite, ST_FONT, AllocateFont);
			assert(spr != NULL);
			new_glyph.sprite = spr;
			new_glyph.width  = spr->width + (this->fs != FS_NORMAL);
			SetGlyphPtr(this->fs, key, &new_glyph, false);
			return new_glyph.sprite;
		} else {
			/* Use '?' for missing characters. */
			this->GetGlyph('?');
			glyph = GetGlyphPtr(this->fs, '?');
			SetGlyphPtr(this->fs, key, glyph, true);
			return glyph->sprite;
		}
	}
	FT_Load_Glyph(this->face, glyph_index, FT_LOAD_DEFAULT);
	FT_Render_Glyph(this->face->glyph, aa ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);

	/* Despite requesting a normal glyph, FreeType may have returned a bitmap */
	aa = (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);

	/* Add 1 pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel */
	width  = max(1, slot->bitmap.width + (this->fs == FS_NORMAL));
	height = max(1, slot->bitmap.rows  + (this->fs == FS_NORMAL));

	/* Limit glyph size to prevent overflows later on. */
	if (width > 256 || height > 256) usererror("Font glyph is too large");

	/* FreeType has rendered the glyph, now we allocate a sprite and copy the image into it */
	sprite.AllocateData(ZOOM_LVL_NORMAL, width * height);
	sprite.type = ST_FONT;
	sprite.width = width;
	sprite.height = height;
	sprite.x_offs = slot->bitmap_left;
	sprite.y_offs = _ascender[this->fs] - slot->bitmap_top;

	/* Draw shadow for medium size */
	if (this->fs == FS_NORMAL && !aa) {
		for (y = 0; y < slot->bitmap.rows; y++) {
			for (x = 0; x < slot->bitmap.width; x++) {
				if (aa ? (slot->bitmap.buffer[x + y * slot->bitmap.pitch] > 0) : HasBit(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
					sprite.data[1 + x + (1 + y) * sprite.width].m = SHADOW_COLOUR;
					sprite.data[1 + x + (1 + y) * sprite.width].a = aa ? slot->bitmap.buffer[x + y * slot->bitmap.pitch] : 0xFF;
				}
			}
		}
	}

	for (y = 0; y < slot->bitmap.rows; y++) {
		for (x = 0; x < slot->bitmap.width; x++) {
			if (aa ? (slot->bitmap.buffer[x + y * slot->bitmap.pitch] > 0) : HasBit(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
				sprite.data[x + y * sprite.width].m = FACE_COLOUR;
				sprite.data[x + y * sprite.width].a = aa ? slot->bitmap.buffer[x + y * slot->bitmap.pitch] : 0xFF;
			}
		}
	}

	new_glyph.sprite = BlitterFactoryBase::GetCurrentBlitter()->Encode(&sprite, AllocateFont);
	new_glyph.width  = slot->advance.x >> 6;

	SetGlyphPtr(this->fs, key, &new_glyph);

	return new_glyph.sprite;
}


bool FreeTypeFontCache::GetDrawGlyphShadow()
{
	return GetFontAAState(FS_NORMAL);
}


uint FreeTypeFontCache::GetGlyphWidth(WChar key)
{
	GlyphEntry *glyph;

	if (this->face == NULL || (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END)) {
		SpriteID sprite = this->GetUnicodeGlyph(key);
		if (sprite == 0) sprite = this->GetUnicodeGlyph('?');
		return SpriteExists(sprite) ? GetSprite(sprite, ST_FONT)->width + (this->fs != FS_NORMAL && this->fs != FS_MONO) : 0;
	}

	glyph = GetGlyphPtr(this->fs, key);
	if (glyph == NULL || glyph->sprite == NULL) {
		this->GetGlyph(key);
		glyph = GetGlyphPtr(this->fs, key);
	}

	return glyph->width;
}


#endif /* WITH_FREETYPE */

/* Sprite based glyph mapping */

#include "table/unicode.h"

/** Get the SpriteID of the first glyph for the given font size */
static SpriteID GetFontBase(FontSize size)
{
	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return SPR_ASCII_SPACE;
		case FS_SMALL:  return SPR_ASCII_SPACE_SMALL;
		case FS_LARGE:  return SPR_ASCII_SPACE_BIG;
		case FS_MONO:   return SPR_ASCII_SPACE;
	}
}


SpriteID SpriteFontCache::GetUnicodeGlyph(uint32 key)
{
	if (this->glyph_to_spriteid_map[GB(key, 8, 8)] == NULL) return 0;
	return this->glyph_to_spriteid_map[GB(key, 8, 8)][GB(key, 0, 8)];
}


void SpriteFontCache::SetUnicodeGlyph(uint32 key, SpriteID sprite)
{
	if (this->glyph_to_spriteid_map == NULL) this->glyph_to_spriteid_map = CallocT<SpriteID*>(256);
	if (this->glyph_to_spriteid_map[GB(key, 8, 8)] == NULL) this->glyph_to_spriteid_map[GB(key, 8, 8)] = CallocT<SpriteID>(256);
	this->glyph_to_spriteid_map[GB(key, 8, 8)][GB(key, 0, 8)] = sprite;
}


void SpriteFontCache::InitializeUnicodeGlyphMap()
{
	/* Clear out existing glyph map if it exists */
	this->ClearGlyphToSpriteMap();

	SpriteID base = GetFontBase(this->fs);

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
