/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "macros.h"
#include "debug.h"
#include "table/sprites.h"
#include "table/control_codes.h"
#include "spritecache.h"
#include "gfx.h"
#include "string.h"
#include "fontcache.h"

#ifdef WITH_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

static FT_Library _library = NULL;
static FT_Face _face_small = NULL;
static FT_Face _face_medium = NULL;
static FT_Face _face_large = NULL;

FreeTypeSettings _freetype;

enum {
	FACE_COLOUR = 1,
	SHADOW_COLOUR = 2,
};


static void LoadFreeTypeFont(const char *font_name, FT_Face *face, const char *type)
{
	FT_Error error;

	if (strlen(font_name) == 0) return;

	error = FT_New_Face(_library, font_name, 0, face);
	if (error == FT_Err_Ok) {
		/* Attempt to select the unicode character map */
		error = FT_Select_Charmap(*face, ft_encoding_unicode);
		if (error == FT_Err_Ok) {
			/* Success */
			return;
		} else if (error == FT_Err_Invalid_CharMap_Handle) {
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

		FT_Done_Face(*face);
		*face = NULL;
	}

	ShowInfoF("Unable to use '%s' for %s font, FreeType reported error 0x%X, using sprite font instead", font_name, type, error);
}


void InitFreeType(void)
{
	if (strlen(_freetype.small_font) == 0 && strlen(_freetype.medium_font) == 0 && strlen(_freetype.large_font) == 0) {
		DEBUG(freetype, 1) ("[FreeType] No font faces specified, using sprite fonts instead");
		return;
	}

	if (FT_Init_FreeType(&_library) != FT_Err_Ok) {
		ShowInfoF("Unable to initialize FreeType, using sprite fonts instead");
		return;
	}

	DEBUG(freetype, 2) ("[FreeType] Initialized");

	/* Load each font */
	LoadFreeTypeFont(_freetype.small_font,  &_face_small,  "small");
	LoadFreeTypeFont(_freetype.medium_font, &_face_medium, "medium");
	LoadFreeTypeFont(_freetype.large_font,  &_face_large,  "large");

	/* Set each font size */
	if (_face_small  != NULL) FT_Set_Pixel_Sizes(_face_small,  0, _freetype.small_size);
	if (_face_medium != NULL) FT_Set_Pixel_Sizes(_face_medium, 0, _freetype.medium_size);
	if (_face_large  != NULL) FT_Set_Pixel_Sizes(_face_large,  0, _freetype.large_size);
}


static FT_Face GetFontFace(FontSize size)
{
	switch (size) {
		default: NOT_REACHED();
		case FS_NORMAL: return _face_medium;
		case FS_SMALL:  return _face_small;
		case FS_LARGE:  return _face_large;
	}
}


typedef struct GlyphEntry {
	Sprite *sprite;
	byte width;
} GlyphEntry;


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


static GlyphEntry *GetGlyphPtr(FontSize size, WChar key)
{
	if (_glyph_ptr[size] == NULL) return NULL;
	if (_glyph_ptr[size][GB(key, 8, 8)] == NULL) return NULL;
	return &_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)];
}


static void SetGlyphPtr(FontSize size, WChar key, const GlyphEntry *glyph)
{
	if (_glyph_ptr[size] == NULL) {
		DEBUG(freetype, 3) ("[FreeType] Allocating root glyph cache for size %u", size);
		_glyph_ptr[size] = calloc(256, sizeof(**_glyph_ptr));
	}

	if (_glyph_ptr[size][GB(key, 8, 8)] == NULL) {
		DEBUG(freetype, 3) ("[FreeType] Allocating glyph cache for range 0x%02X00, size %u", GB(key, 8, 8), size);
		_glyph_ptr[size][GB(key, 8, 8)] = calloc(256, sizeof(***_glyph_ptr));
	}

	DEBUG(freetype, 4) ("[FreeType] Set glyph for unicode character 0x%04X, size %u", key, size);
	_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)].sprite = glyph->sprite;
	_glyph_ptr[size][GB(key, 8, 8)][GB(key, 0, 8)].width  = glyph->width;
}


const Sprite *GetGlyph(FontSize size, WChar key)
{
	FT_Face face = GetFontFace(size);
	FT_GlyphSlot slot;
	GlyphEntry new_glyph;
	GlyphEntry *glyph;
	Sprite *sprite;
	int width;
	int height;
	int x;
	int y;
	int y_adj;

	assert(IsPrintable(key));

	/* Bail out if no face loaded, or for our special characters */
	if (face == NULL || (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END)) {
		SpriteID sprite = GetUnicodeGlyph(size, key);
		if (sprite == 0) sprite = GetUnicodeGlyph(size, '?');
		return GetSprite(sprite);
	}

	/* Check for the glyph in our cache */
	glyph = GetGlyphPtr(size, key);
	if (glyph != NULL && glyph->sprite != NULL) return glyph->sprite;

	slot = face->glyph;

	FT_Load_Char(face, key, FT_LOAD_DEFAULT);
	FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);

	/* Add 1 pixel for the shadow on the medium font. Our sprite must be at least 1x1 pixel */
	width  = max(1, slot->bitmap.width + (size == FS_NORMAL));
	height = max(1, slot->bitmap.rows  + (size == FS_NORMAL));

	/* FreeType has rendered the glyph, now we allocate a sprite and copy the image into it */
	sprite = calloc(width * height + 8, 1);
	sprite->info   = 1;
	sprite->width  = width;
	sprite->height = height;
	sprite->x_offs = slot->bitmap_left;
	// XXX 2 should be determined somehow... it's right for the normal face
	y_adj = (size == FS_NORMAL) ? 2 : 0;
	sprite->y_offs = GetCharacterHeight(size) - slot->bitmap_top - y_adj;

	/* Draw shadow for medium size */
	if (size == FS_NORMAL) {
		for (y = 0; y < slot->bitmap.rows; y++) {
			for (x = 0; x < slot->bitmap.width; x++) {
				if (HASBIT(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
					sprite->data[1 + x + (1 + y) * sprite->width] = SHADOW_COLOUR;
				}
			}
		}
	}

	for (y = 0; y < slot->bitmap.rows; y++) {
		for (x = 0; x < slot->bitmap.width; x++) {
			if (HASBIT(slot->bitmap.buffer[(x / 8) + y * slot->bitmap.pitch], 7 - (x % 8))) {
				sprite->data[x + y * sprite->width] = FACE_COLOUR;
			}
		}
	}

	new_glyph.sprite = sprite;
	new_glyph.width  = (slot->advance.x >> 6) + (size != FS_NORMAL);

	SetGlyphPtr(size, key, &new_glyph);

	return sprite;
}


uint GetGlyphWidth(FontSize size, WChar key)
{
	FT_Face face = GetFontFace(size);
	GlyphEntry *glyph;

	if (face == NULL || (key >= SCC_SPRITE_START && key <= SCC_SPRITE_END)) {
		SpriteID sprite = GetUnicodeGlyph(size, key);
		if (sprite == 0) sprite = GetUnicodeGlyph(size, '?');
		return SpriteExists(sprite) ? GetSprite(sprite)->width + (size != FS_NORMAL) : 0;
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
	}
}


SpriteID GetUnicodeGlyph(FontSize size, uint32 key)
{
	if (_unicode_glyph_map[size][GB(key, 8, 8)] == NULL) return 0;
	return _unicode_glyph_map[size][GB(key, 8, 8)][GB(key, 0, 8)];
}


void SetUnicodeGlyph(FontSize size, uint32 key, SpriteID sprite)
{
	if (_unicode_glyph_map[size] == NULL) _unicode_glyph_map[size] = calloc(256, sizeof(*_unicode_glyph_map[size]));
	if (_unicode_glyph_map[size][GB(key, 8, 8)] == NULL) _unicode_glyph_map[size][GB(key, 8, 8)] = calloc(256, sizeof(**_unicode_glyph_map[size]));
	_unicode_glyph_map[size][GB(key, 8, 8)][GB(key, 0, 8)] = sprite;
}


void InitializeUnicodeGlyphMap(void)
{
	FontSize size;
	SpriteID base;
	SpriteID sprite;
	uint i;

	for (size = FS_NORMAL; size != FS_END; size++) {
		base = GetFontBase(size);
		for (i = ASCII_LETTERSTART; i < 256; i++) {
			sprite = base + i - ASCII_LETTERSTART;
			if (!SpriteExists(sprite)) continue;
			SetUnicodeGlyph(size, i, sprite);
			SetUnicodeGlyph(size, i + SCC_SPRITE_START, sprite);
		}
		for (i = 0; i < lengthof(_default_unicode_map); i++) {
			sprite = base + _default_unicode_map[i].key - ASCII_LETTERSTART;
			SetUnicodeGlyph(size, _default_unicode_map[i].code, sprite);
		}
	}
}

