/* $Id$ */

/** @file fontcache.h Functions to read fonts from files and cache them. */

#ifndef FONTCACHE_H
#define FONTCACHE_H

#include "gfx_type.h"

/** Get the SpriteID mapped to the given font size and key */
SpriteID GetUnicodeGlyph(FontSize size, uint32 key);

/** Map a SpriteID to the font size and key */
void SetUnicodeGlyph(FontSize size, uint32 key, SpriteID sprite);

/** Initialize the glyph map */
void InitializeUnicodeGlyphMap();

#ifdef WITH_FREETYPE

struct FreeTypeSettings {
	char small_font[MAX_PATH];
	char medium_font[MAX_PATH];
	char large_font[MAX_PATH];
	uint small_size;
	uint medium_size;
	uint large_size;
	bool small_aa;
	bool medium_aa;
	bool large_aa;
};

extern FreeTypeSettings _freetype;

void InitFreeType();
void UninitFreeType();
const struct Sprite *GetGlyph(FontSize size, uint32 key);
uint GetGlyphWidth(FontSize size, uint32 key);

/**
 * We would like to have a fallback font as the current one
 * doesn't contain all characters we need.
 * This function must set all fonts of settings.
 * @param settings the settings to overwrite the fontname of.
 * @param language_isocode the language, e.g. en_GB.
 * @param winlangid the language ID windows style.
 * @return true if a font has been set, false otherwise.
 */
bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid);

#else

/* Stub for initializiation */
static inline void InitFreeType() {}
static inline void UninitFreeType() {}

/** Get the Sprite for a glyph */
static inline const Sprite *GetGlyph(FontSize size, uint32 key)
{
	SpriteID sprite = GetUnicodeGlyph(size, key);
	if (sprite == 0) sprite = GetUnicodeGlyph(size, '?');
	return GetSprite(sprite, ST_FONT);
}


/** Get the width of a glyph */
static inline uint GetGlyphWidth(FontSize size, uint32 key)
{
	SpriteID sprite = GetUnicodeGlyph(size, key);
	if (sprite == 0) sprite = GetUnicodeGlyph(size, '?');
	return SpriteExists(sprite) ? GetSprite(sprite, ST_FONT)->width + (size != FS_NORMAL) : 0;
}

#endif /* WITH_FREETYPE */

#endif /* FONTCACHE_H */
