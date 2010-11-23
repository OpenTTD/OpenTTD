/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fontcache.h Functions to read fonts from files and cache them. */

#ifndef FONTCACHE_H
#define FONTCACHE_H

#include "spritecache.h"

/** Get the SpriteID mapped to the given font size and key */
SpriteID GetUnicodeGlyph(FontSize size, uint32 key);

/** Map a SpriteID to the font size and key */
void SetUnicodeGlyph(FontSize size, uint32 key, SpriteID sprite);

/** Initialize the glyph map */
void InitializeUnicodeGlyphMap();

void ResetFontSizes();

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
const Sprite *GetGlyph(FontSize size, uint32 key);
uint GetGlyphWidth(FontSize size, uint32 key);

typedef bool (SetFallbackFontCallback)(const char **);
/**
 * We would like to have a fallback font as the current one
 * doesn't contain all characters we need.
 * This function must set all fonts of settings.
 * @param settings the settings to overwrite the fontname of.
 * @param language_isocode the language, e.g. en_GB.
 * @param winlangid the language ID windows style.
 * @param callback The function to call to check for missing glyphs.
 * @return true if a font has been set, false otherwise.
 */
bool SetFallbackFont(FreeTypeSettings *settings, const char *language_isocode, int winlangid, SetFallbackFontCallback *callback);

#else

/* Stub for initializiation */
static inline void InitFreeType() { ResetFontSizes(); }
static inline void UninitFreeType() { ResetFontSizes(); }

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
