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

#ifdef WITH_FREETYPE

/** Settings for a single freetype font. */
struct FreeTypeSubSetting {
	char font[MAX_PATH]; ///< The name of the font, or path to the font.
	uint size;           ///< The (requested) size of the font.
	bool aa;             ///< Whether to do anti aliasing or not.
};

/** Settings for the freetype fonts. */
struct FreeTypeSettings {
	FreeTypeSubSetting small;  ///< The smallest font; mostly used for zoomed out view.
	FreeTypeSubSetting medium; ///< The normal font size.
	FreeTypeSubSetting large;  ///< The largest font; mostly used for newspapers.
	FreeTypeSubSetting mono;   ///< The mono space font used for license/readme viewers.
};

extern FreeTypeSettings _freetype;

void InitFreeType(bool monospace);
void UninitFreeType();
void ClearFontCache();
const Sprite *GetGlyph(FontSize size, uint32 key);
uint GetGlyphWidth(FontSize size, uint32 key);
bool GetDrawGlyphShadow();

#else

/* Stub for initializiation */
static inline void InitFreeType(bool monospace) { extern void ResetFontSizes(bool monospace); ResetFontSizes(monospace); }
static inline void UninitFreeType() {}
static inline void ClearFontCache() {}

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

static inline bool GetDrawGlyphShadow()
{
	return false;
}

#endif /* WITH_FREETYPE */

#endif /* FONTCACHE_H */
