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
typedef uint32_t GlyphID;
static const GlyphID SPRITE_GLYPH = 1U << 30;

/** Font cache for basic fonts. */
class FontCache {
protected:
	static FontCache *caches[FS_END]; ///< All the font caches.
	FontCache *parent;                ///< The parent of this font cache.
	const FontSize fs;                ///< The size of the font.
	int height;                       ///< The height of the font.
	int ascender;                     ///< The ascender value of the font.
	int descender;                    ///< The descender value of the font.

public:
	FontCache(FontSize fs);
	virtual ~FontCache();

	static void InitializeFontCaches();

	static int GetDefaultFontHeight(FontSize fs);

	/**
	 * Get the FontSize of the font.
	 * @return The FontSize.
	 */
	inline FontSize GetSize() const { return this->fs; }

	/**
	 * Get the height of the font.
	 * @return The height of the font.
	 */
	inline int GetHeight() const { return this->height; }

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
	 * Get the nominal font size of the font.
	 * @return The nominal font size.
	 */
	virtual int GetFontSize() const { return this->height; }

	/**
	 * Map a SpriteID to the key
	 * @param key The key to map to.
	 * @param sprite The sprite that is being mapped.
	 */
	virtual void SetUnicodeGlyph(char32_t key, SpriteID sprite) = 0;

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
	 * @param fallback Allow fallback to the parent font.
	 * @return The glyph ID used to draw the character.
	 */
	virtual GlyphID MapCharToGlyph(char32_t key, bool fallback = true) = 0;

	/**
	 * Get the native OS font handle, if there is one.
	 * @return Opaque OS font handle.
	 */
	virtual const void *GetOSHandle()
	{
		return nullptr;
	}

	/**
	 * Get the name of this font.
	 * @return The name of the font.
	 */
	virtual std::string GetFontName() = 0;

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

	static std::string GetName(FontSize fs);

	/**
	 * Check whether the font cache has a parent.
	 */
	inline bool HasParent()
	{
		return this->parent != nullptr;
	}

	/**
	 * Is this a built-in sprite font?
	 */
	virtual bool IsBuiltInFont() = 0;
};

/** Map a SpriteID to the font size and key */
inline void SetUnicodeGlyph(FontSize size, char32_t key, SpriteID sprite)
{
	FontCache::Get(size)->SetUnicodeGlyph(key, sprite);
}

/** Initialize the glyph map */
inline void InitializeUnicodeGlyphMap()
{
	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		FontCache::Get(fs)->InitializeUnicodeGlyphMap();
	}
}

inline void ClearFontCache()
{
	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		FontCache::Get(fs)->ClearFontCache();
	}
}

/** Get the Sprite for a glyph */
inline const Sprite *GetGlyph(FontSize size, char32_t key)
{
	FontCache *fc = FontCache::Get(size);
	return fc->GetGlyph(fc->MapCharToGlyph(key));
}

/** Get the width of a glyph */
inline uint GetGlyphWidth(FontSize size, char32_t key)
{
	FontCache *fc = FontCache::Get(size);
	return fc->GetGlyphWidth(fc->MapCharToGlyph(key));
}

inline bool GetDrawGlyphShadow(FontSize size)
{
	return FontCache::Get(size)->GetDrawGlyphShadow();
}

/** Settings for a single font. */
struct FontCacheSubSetting {
	std::string font; ///< The name of the font, or path to the font.
	uint size;        ///< The (requested) size of the font.

	const void *os_handle = nullptr; ///< Optional native OS font info. Only valid during font search.
};

/** Settings for the four different fonts. */
struct FontCacheSettings {
	FontCacheSubSetting small;  ///< The smallest font; mostly used for zoomed out view.
	FontCacheSubSetting medium; ///< The normal font size.
	FontCacheSubSetting large;  ///< The largest font; mostly used for newspapers.
	FontCacheSubSetting mono;   ///< The mono space font used for license/readme viewers.
	bool prefer_sprite;         ///< Whether to prefer the built-in sprite font over resizable fonts.
	bool global_aa;             ///< Whether to anti alias all font sizes.
};

extern FontCacheSettings _fcsettings;

/**
 * Get the settings of a given font size.
 * @param fs The font size to look up.
 * @return The settings.
 */
inline FontCacheSubSetting *GetFontCacheSubSetting(FontSize fs)
{
	switch (fs) {
		default: NOT_REACHED();
		case FS_SMALL:  return &_fcsettings.small;
		case FS_NORMAL: return &_fcsettings.medium;
		case FS_LARGE:  return &_fcsettings.large;
		case FS_MONO:   return &_fcsettings.mono;
	}
}

void InitFontCache(bool monospace);
void UninitFontCache();

bool GetFontAAState();
void SetFont(FontSize fontsize, const std::string &font, uint size);

#endif /* FONTCACHE_H */
