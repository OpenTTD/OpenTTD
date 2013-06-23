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

#ifdef WITH_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H

FT_Library _library = NULL;
static FT_Face _face_small = NULL;
static FT_Face _face_medium = NULL;
static FT_Face _face_large = NULL;
static FT_Face _face_mono = NULL;
static int _ascender[FS_END];

FreeTypeSettings _freetype;

static const byte FACE_COLOUR   = 1;
static const byte SHADOW_COLOUR = 2;

static void SetFontGeometry(FT_Face face, FontSize size, int pixels)
{
	if (pixels == 0) {
		/* Try to determine a good height based on the minimal height recommended by the font. */
		pixels = _default_font_height[size];

		TT_Header *head = (TT_Header *)FT_Get_Sfnt_Table(face, ft_sfnt_head);
		if (head != NULL) {
			/* Font height is minimum height plus the difference between the default
			 * height for this font size and the small size. */
			int diff = _default_font_height[size] - _default_font_height[FS_SMALL];
			pixels = Clamp(min(head->Lowest_Rec_PPEM, 20) + diff, _default_font_height[size], MAX_FONT_SIZE);
		}
	}

	FT_Error err = FT_Set_Pixel_Sizes(face, 0, pixels);
	if (err == FT_Err_Invalid_Pixel_Size) {

		/* Find nearest size to that requested */
		FT_Bitmap_Size *bs = face->available_sizes;
		int i = face->num_fixed_sizes;
		int n = bs->height;
		for (; --i; bs++) {
			if (abs(pixels - bs->height) < abs(pixels - n)) n = bs->height;
		}

		FT_Set_Pixel_Sizes(face, 0, n);
	}

	int asc = face->size->metrics.ascender >> 6;
	int dec = face->size->metrics.descender >> 6;

	_ascender[size] = asc;
	_font_height[size] = asc - dec;
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
 * Unload a face and set it to NULL.
 * @param face the face to unload
 */
static void UnloadFace(FT_Face *face)
{
	if (*face == NULL) return;

	FT_Done_Face(*face);
	*face = NULL;
}

/**
 * (Re)initialize the freetype related things, i.e. load the non-sprite fonts.
 * @param monospace Whether to initialise the monospace or regular fonts.
 */
void InitFreeType(bool monospace)
{
	ResetFontSizes(monospace);
	ResetGlyphCache(monospace);

	if (monospace) {
		UnloadFace(&_face_mono);
	} else {
		UnloadFace(&_face_small);
		UnloadFace(&_face_medium);
		UnloadFace(&_face_large);
	}

	if (StrEmpty(_freetype.small_font) && StrEmpty(_freetype.medium_font) && StrEmpty(_freetype.large_font) && StrEmpty(_freetype.mono_font)) {
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
		LoadFreeTypeFont(_freetype.mono_font ,  &_face_mono,   "mono");

		if (_face_mono != NULL) {
			SetFontGeometry(_face_mono, FS_MONO, _freetype.mono_size);
		}
	} else {
		LoadFreeTypeFont(_freetype.small_font,  &_face_small,  "small");
		LoadFreeTypeFont(_freetype.medium_font, &_face_medium, "medium");
		LoadFreeTypeFont(_freetype.large_font,  &_face_large,  "large");

		/* Set each font size */
		if (_face_small != NULL) {
			SetFontGeometry(_face_small, FS_SMALL, _freetype.small_size);
		}
		if (_face_medium != NULL) {
			SetFontGeometry(_face_medium, FS_NORMAL, _freetype.medium_size);
		}
		if (_face_large != NULL) {
			SetFontGeometry(_face_large, FS_LARGE, _freetype.large_size);
		}
	}
}

/**
 * Free everything allocated w.r.t. fonts.
 */
void UninitFreeType()
{
	ResetGlyphCache(true);
	ResetGlyphCache(false);

	UnloadFace(&_face_small);
	UnloadFace(&_face_medium);
	UnloadFace(&_face_large);
	UnloadFace(&_face_mono);

	FT_Done_FreeType(_library);
	_library = NULL;
}

/**
 * Reset cached glyphs.
 */
void ClearFontCache()
{
	ResetGlyphCache(true);
	ResetGlyphCache(false);
}

static FT_Face GetFontFace(FontSize size)
{
	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return _face_medium;
		case FS_SMALL:  return _face_small;
		case FS_LARGE:  return _face_large;
		case FS_MONO:   return _face_mono;
	}
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
		case FS_NORMAL: return _freetype.medium_aa;
		case FS_SMALL:  return _freetype.small_aa;
		case FS_LARGE:  return _freetype.large_aa;
		case FS_MONO:   return _freetype.mono_aa;
	}
}


const Sprite *GetGlyph(FontSize size, WChar key)
{
	FT_Face face = GetFontFace(size);
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
	if (face == NULL || (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END)) {
		SpriteID sprite = GetUnicodeGlyph(size, key);
		if (sprite == 0) sprite = GetUnicodeGlyph(size, '?');

		/* Load the sprite if it's known. */
		if (sprite != 0) return GetSprite(sprite, ST_FONT);

		/* For the 'rare' case there is no font available at all. */
		if (face == NULL) error("No sprite font and no real font either... bailing!");

		/* Use the '?' from the freetype font. */
		key = '?';
	}

	/* Check for the glyph in our cache */
	glyph = GetGlyphPtr(size, key);
	if (glyph != NULL && glyph->sprite != NULL) return glyph->sprite;

	slot = face->glyph;

	bool aa = GetFontAAState(size);

	FT_UInt glyph_index = FT_Get_Char_Index(face, key);
	if (glyph_index == 0) {
		if (key == '?') {
			/* The font misses the '?' character. Use sprite font. */
			SpriteID sprite = GetUnicodeGlyph(size, key);
			Sprite *spr = (Sprite*)GetRawSprite(sprite, ST_FONT, AllocateFont);
			assert(spr != NULL);
			new_glyph.sprite = spr;
			new_glyph.width  = spr->width + (size != FS_NORMAL);
			SetGlyphPtr(size, key, &new_glyph, false);
			return new_glyph.sprite;
		} else {
			/* Use '?' for missing characters. */
			GetGlyph(size, '?');
			glyph = GetGlyphPtr(size, '?');
			SetGlyphPtr(size, key, glyph, true);
			return glyph->sprite;
		}
	}
	FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
	FT_Render_Glyph(face->glyph, aa ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);

	/* Despite requesting a normal glyph, FreeType may have returned a bitmap */
	aa = (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);

	/* Add 1 pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel */
	width  = max(1, slot->bitmap.width + (size == FS_NORMAL));
	height = max(1, slot->bitmap.rows  + (size == FS_NORMAL));

	/* Limit glyph size to prevent overflows later on. */
	if (width > 256 || height > 256) usererror("Font glyph is too large");

	/* FreeType has rendered the glyph, now we allocate a sprite and copy the image into it */
	sprite.AllocateData(ZOOM_LVL_NORMAL, width * height);
	sprite.type = ST_FONT;
	sprite.width = width;
	sprite.height = height;
	sprite.x_offs = slot->bitmap_left;
	sprite.y_offs = _ascender[size] - slot->bitmap_top;

	/* Draw shadow for medium size */
	if (size == FS_NORMAL && !aa) {
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

	SetGlyphPtr(size, key, &new_glyph);

	return new_glyph.sprite;
}


bool GetDrawGlyphShadow()
{
	return GetFontFace(FS_NORMAL) != NULL && GetFontAAState(FS_NORMAL);
}


uint GetGlyphWidth(FontSize size, WChar key)
{
	FT_Face face = GetFontFace(size);
	GlyphEntry *glyph;

	if (face == NULL || (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END)) {
		SpriteID sprite = GetUnicodeGlyph(size, key);
		if (sprite == 0) sprite = GetUnicodeGlyph(size, '?');
		return SpriteExists(sprite) ? GetSprite(sprite, ST_FONT)->width + (size != FS_NORMAL && size != FS_MONO) : 0;
	}

	glyph = GetGlyphPtr(size, key);
	if (glyph == NULL || glyph->sprite == NULL) {
		GetGlyph(size, key);
		glyph = GetGlyphPtr(size, key);
	}

	return glyph->width;
}


#endif /* WITH_FREETYPE */

/* Sprite based glyph mapping */

#include "table/unicode.h"

static SpriteID **_unicode_glyph_map[FS_END];


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


SpriteID GetUnicodeGlyph(FontSize size, uint32 key)
{
	if (_unicode_glyph_map[size][GB(key, 8, 8)] == NULL) return 0;
	return _unicode_glyph_map[size][GB(key, 8, 8)][GB(key, 0, 8)];
}


void SetUnicodeGlyph(FontSize size, uint32 key, SpriteID sprite)
{
	if (_unicode_glyph_map[size] == NULL) _unicode_glyph_map[size] = CallocT<SpriteID*>(256);
	if (_unicode_glyph_map[size][GB(key, 8, 8)] == NULL) _unicode_glyph_map[size][GB(key, 8, 8)] = CallocT<SpriteID>(256);
	_unicode_glyph_map[size][GB(key, 8, 8)][GB(key, 0, 8)] = sprite;
}


void InitializeUnicodeGlyphMap()
{
	for (FontSize size = FS_BEGIN; size != FS_END; size++) {
		/* Clear out existing glyph map if it exists */
		if (_unicode_glyph_map[size] != NULL) {
			for (uint i = 0; i < 256; i++) {
				free(_unicode_glyph_map[size][i]);
			}
			free(_unicode_glyph_map[size]);
			_unicode_glyph_map[size] = NULL;
		}

		SpriteID base = GetFontBase(size);

		for (uint i = ASCII_LETTERSTART; i < 256; i++) {
			SpriteID sprite = base + i - ASCII_LETTERSTART;
			if (!SpriteExists(sprite)) continue;
			SetUnicodeGlyph(size, i, sprite);
			SetUnicodeGlyph(size, i + SCC_SPRITE_START, sprite);
		}

		for (uint i = 0; i < lengthof(_default_unicode_map); i++) {
			byte key = _default_unicode_map[i].key;
			if (key == CLRA) {
				/* Clear the glyph. This happens if the glyph at this code point
				 * is non-standard and should be accessed by an SCC_xxx enum
				 * entry only. */
				SetUnicodeGlyph(size, _default_unicode_map[i].code, 0);
			} else {
				SpriteID sprite = base + key - ASCII_LETTERSTART;
				SetUnicodeGlyph(size, _default_unicode_map[i].code, sprite);
			}
		}
	}
}
