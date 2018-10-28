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

#include "string_type.h"
#include "spritecache.h"

/** Glyphs are characters from a font. */
typedef uint32 GlyphID;
static const GlyphID SPRITE_GLYPH = 1U << 30;

/** Font cache for basic fonts. */
class FontCache {
private:
	static FontCache *caches[FS_END]; ///< All the font caches.
protected:
	FontCache *parent;                ///< The parent of this font cache.
	const FontSize fs;                ///< The size of the font.
	int height;                       ///< The height of the font.
	int ascender;                     ///< The ascender value of the font.
	int descender;                    ///< The descender value of the font.
	int units_per_em;                 ///< The units per EM value of the font.
public:
	FontCache(FontSize fs);
	virtual ~FontCache();

	/**
	 * Get the FontSize of the font.
	 * @return The FontSize.
	 */
	inline FontSize GetSize() const { return this->fs; }

	/**
	 * Get the height of the font.
	 * @return The height of the font.
	 */
	virtual int GetHeight() const { return this->height; }

	/**
	 * Get the ascender value of the font.
	 * @return The ascender value of the font.
	 */
	inline int GetAscender() const { return this->ascender; }

	/**
	 * Get the descender value of the font.
	 * @return The descender value of the font.
	 */
	inline int GetDescender() const{ return this->descender; }

	/**
	 * Get the units per EM value of the font.
	 * @return The units per EM value of the font.
	 */
	inline int GetUnitsPerEM() const { return this->units_per_em; }

	/**
	 * Get the nominal font size of the font.
	 * @return The nominal font size.
	 */
	virtual int GetFontSize() const { return this->height; }

	/**
	 * Get the SpriteID mapped to the given key
	 * @param key The key to get the sprite for.
	 * @return The sprite.
	 */
	virtual SpriteID GetUnicodeGlyph(WChar key) = 0;

	/**
	 * Map a SpriteID to the key
	 * @param key The key to map to.
	 * @param sprite The sprite that is being mapped.
	 */
	virtual void SetUnicodeGlyph(WChar key, SpriteID sprite) = 0;

	/** Initialize the glyph map */
	virtual void InitializeUnicodeGlyphMap() = 0;

	/** Clear the font cache. */
	virtual void ClearFontCache() = 0;

	/**
	 * Get the glyph (sprite) of the given key.
	 * @param key The key to look up.
	 * @return The sprite.
	 */
	virtual const Sprite *GetGlyph(GlyphID key) = 0;

	/**
	 * Get the width of the glyph with the given key.
	 * @param key The key to look up.
	 * @return The width.
	 */
	virtual uint GetGlyphWidth(GlyphID key) = 0;

	/**
	 * Do we need to draw a glyph shadow?
	 * @return True if it has to be done, otherwise false.
	 */
	virtual bool GetDrawGlyphShadow() = 0;

	/**
	 * Map a character into a glyph.
	 * @param key The character.
	 * @return The glyph ID used to draw the character.
	 */
	virtual GlyphID MapCharToGlyph(WChar key) = 0;

	/**
	 * Read a font table from the font.
	 * @param tag The of the table to load.
	 * @param length The length of the read data.
	 * @return The loaded table data.
	 */
	virtual const void *GetFontTable(uint32 tag, size_t &length) = 0;

	/**
	 * Get the name of this font.
	 * @return The name of the font.
	 */
	virtual const char *GetFontName() = 0;

	/**
	 * Get the font cache of a given font size.
	 * @param fs The font size to look up.
	 * @return The font cache.
	 */
	static inline FontCache *Get(FontSize fs)
	{
		assert(fs < FS_END);
		return FontCache::caches[fs];
	}

	/**
	 * Check whether the font cache has a parent.
	 */
	inline bool HasParent()
	{
		return this->parent != NULL;
	}

	/**
	 * Is this a built-in sprite font?
	 */
	virtual bool IsBuiltInFont() = 0;
};

/** Get the SpriteID mapped to the given font size and key */
static inline SpriteID GetUnicodeGlyph(FontSize size, WChar key)
{
	return FontCache::Get(size)->GetUnicodeGlyph(key);
}

/** Map a SpriteID to the font size and key */
static inline void SetUnicodeGlyph(FontSize size, WChar key, SpriteID sprite)
{
	FontCache::Get(size)->SetUnicodeGlyph(key, sprite);
}

/** Initialize the glyph map */
static inline void InitializeUnicodeGlyphMap()
{
	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		FontCache::Get(fs)->InitializeUnicodeGlyphMap();
	}
}

static inline void ClearFontCache()
{
	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		FontCache::Get(fs)->ClearFontCache();
	}
}

/** Get the Sprite for a glyph */
static inline const Sprite *GetGlyph(FontSize size, WChar key)
{
	FontCache *fc = FontCache::Get(size);
	return fc->GetGlyph(fc->MapCharToGlyph(key));
}

/** Get the width of a glyph */
static inline uint GetGlyphWidth(FontSize size, WChar key)
{
	FontCache *fc = FontCache::Get(size);
	return fc->GetGlyphWidth(fc->MapCharToGlyph(key));
}

static inline bool GetDrawGlyphShadow(FontSize size)
{
	return FontCache::Get(size)->GetDrawGlyphShadow();
}

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

#endif /* WITH_FREETYPE */

void InitFreeType(bool monospace);
void UninitFreeType();

#endif /* FONTCACHE_H */
