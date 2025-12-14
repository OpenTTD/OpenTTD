/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fontcache.h Functions to read fonts from files and cache them. */

#ifndef FONTCACHE_H
#define FONTCACHE_H

#include "gfx_type.h"
#include "provider_manager.h"
#include "spritecache_type.h"

/** Glyphs are characters from a font. */
typedef uint32_t GlyphID;
static const GlyphID SPRITE_GLYPH = 1U << 30;

/** Font cache for basic fonts. */
class FontCache {
protected:
	static std::array<std::unique_ptr<FontCache>, FS_END> caches; ///< All the font caches.
	std::unique_ptr<FontCache> parent; ///< The parent of this font cache.
	const FontSize fs; ///< The size of the font.
	int height = 0; ///< The height of the font.
	int ascender = 0; ///< The ascender value of the font.
	int descender = 0; ///< The descender value of the font.

	FontCache(FontSize fs) : fs(fs) {}
	static void Register(std::unique_ptr<FontCache> &&fc);

public:
	virtual ~FontCache() = default;

	static void InitializeFontCaches();
	static void UninitializeFontCaches();
	static void LoadFontCaches(FontSizes fontsizes);
	static void ClearFontCaches(FontSizes fontsizes);

	/** Default unscaled font heights. */
	static const int DEFAULT_FONT_HEIGHT[FS_END];
	/** Default unscaled font ascenders. */
	static const int DEFAULT_FONT_ASCENDER[FS_END];

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
		return FontCache::caches[fs].get();
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

uint GetFontCacheFontSize(FontSize fs);
std::string GetFontCacheFontName(FontSize fs);

bool GetFontAAState();
void SetFont(FontSize fontsize, const std::string &font, uint size);

/** Different types of font that can be loaded. */
enum class FontType : uint8_t {
	Sprite, ///< Bitmap sprites from GRF files.
	TrueType, ///< Scalable TrueType fonts.
};

/** Factory for FontCaches. */
class FontCacheFactory : public BaseProvider<FontCacheFactory> {
public:
	FontCacheFactory(std::string_view name, std::string_view description) : BaseProvider<FontCacheFactory>(name, description)
	{
		ProviderManager<FontCacheFactory>::Register(*this);
	}

	virtual ~FontCacheFactory()
	{
		ProviderManager<FontCacheFactory>::Unregister(*this);
	}

	virtual std::unique_ptr<FontCache> LoadFont(FontSize fs, FontType fonttype) const = 0;
	virtual bool FindFallbackFont(struct FontCacheSettings *settings, const std::string &language_isocode, class MissingGlyphSearcher *callback) const = 0;
};

class FontProviderManager : ProviderManager<FontCacheFactory> {
public:
	static std::unique_ptr<FontCache> LoadFont(FontSize fs, FontType fonttype);
	static bool FindFallbackFont(FontCacheSettings *settings, const std::string &language_isocode, MissingGlyphSearcher *callback);
};

/* Implemented in spritefontcache.cpp */
void InitializeUnicodeGlyphMap();
void SetUnicodeGlyph(FontSize size, char32_t key, SpriteID sprite);

#endif /* FONTCACHE_H */
