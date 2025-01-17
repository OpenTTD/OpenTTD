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
using FontIndex = uint8_t;

static const FontIndex INVALID_FONT_INDEX = MAX_UVALUE(uint8_t);

/** Font cache for basic fonts. */
class FontCache {
protected:
	using FontCaches = std::vector<std::unique_ptr<FontCache>>;
	static FontCaches caches;
	static std::array<int, FS_END> max_height;
	static std::array<std::unordered_map<char32_t, FontIndex>, FS_END> character_to_fontcache;
	static std::array<FontIndex, FS_END> sprite_font_index;
	static std::array<FontIndex, FS_END> default_font_index;

	const FontSize fs;                ///< The size of the font.
	FontIndex font_index; ///< The index of the font.
	int height;                       ///< The height of the font.
	int ascender;                     ///< The ascender value of the font.
	int descender;                    ///< The descender value of the font.

	friend void UninitFontCache();
	friend void InitFontCache(bool monospace);

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

	inline FontIndex GetIndex() const { return this->font_index; }

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
	 * @return The glyph ID used to draw the character.
	 */
	virtual GlyphID MapCharToGlyph(char32_t key) = 0;

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
	 * Get span of all FontCaches.
	 * @return Span of all FontCaches.
	 */
	static inline std::span<const std::unique_ptr<FontCache>> Get()
	{
		return FontCache::caches;
	}

	/**
	 * Get the font cache of a given font size.
	 * @param fs The font size to look up.
	 * @return The font cache.
	 */
	static inline FontCache *Get(FontIndex font_index)
	{
		assert(font_index < FontCache::caches.size());
		return FontCache::caches[font_index].get();
	}

	static inline int GetCharacterHeight(FontSize fs)
	{
		return FontCache::max_height[fs];
	}

	static void UpdateCharacterHeight(FontSize fs);

	static inline FontIndex GetDefaultFontIndex(FontSize fs)
	{
		return FontCache::default_font_index[fs];
	}

	static inline class FontCache *GetDefaultFontCache(FontSize fs)
	{
		FontIndex index = FontCache::GetDefaultFontIndex(fs);
		if (index != INVALID_FONT_INDEX) return FontCache::Get(index);
		NOT_REACHED();
	}

	static inline FontIndex GetSpriteFontIndex(FontSize fs)
	{
		return FontCache::sprite_font_index[fs];
	}

	static inline class SpriteFontCache *GetSpriteFontCache(FontSize fs)
	{
		FontIndex index = FontCache::GetSpriteFontIndex(fs);
		if (index != INVALID_FONT_INDEX) return reinterpret_cast<class SpriteFontCache *>(FontCache::Get(index));
		NOT_REACHED();
	}

	inline void ClaimCharacter(char32_t c)
	{
		auto it = FontCache::character_to_fontcache[this->fs].find(c);
		if (it != std::end(FontCache::character_to_fontcache[this->fs]) && it->second > this->font_index) return;
		FontCache::character_to_fontcache[this->fs][c] = this->font_index;
	}

	static inline FontIndex GetFontIndexForCharacter(FontSize fs, char32_t c)
	{
		const auto found = FontCache::character_to_fontcache[fs].find(c);
		if (found == std::end(FontCache::character_to_fontcache[fs])) return INVALID_FONT_INDEX;
		return found->second;
	}

	virtual void UpdateCharacterMap() = 0;

	/**
	 * Is this a built-in sprite font?
	 */
	virtual bool IsBuiltInFont() = 0;
};

inline void ClearFontCache()
{
	for (const auto &fc : FontCache::Get()) {
		if (fc == nullptr) continue;
		fc->ClearFontCache();
	}
}

/** Get the Sprite for a glyph */
inline const Sprite *GetGlyph(FontSize size, char32_t key)
{
	FontIndex font_index = FontCache::GetFontIndexForCharacter(size, key);
	FontCache *fc = font_index != INVALID_FONT_INDEX ? FontCache::Get(font_index) : FontCache::GetDefaultFontCache(size);
	if (fc == nullptr) return nullptr;
	return fc->GetGlyph(fc->MapCharToGlyph(key));
}

/** Get the width of a glyph */
inline uint GetGlyphWidth(FontSize size, char32_t key)
{
	FontIndex font_index = FontCache::GetFontIndexForCharacter(size, key);
	FontCache *fc = font_index != INVALID_FONT_INDEX ? FontCache::Get(font_index) : FontCache::GetDefaultFontCache(size);
	if (fc == nullptr) return 0;
	return fc->GetGlyphWidth(fc->MapCharToGlyph(key));
}

/** Settings for a single font. */
struct FontCacheSubSetting {
	std::string font; ///< The name of the font, or path to the font.
	uint size;        ///< The (requested) size of the font.

	std::vector<std::pair<std::string, std::vector<uint8_t>>> fallback_fonts;
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

inline const std::string_view FontSizeToName(FontSize fs)
{
	static const std::string_view SIZE_TO_NAME[] = { "medium", "small", "large", "mono" };
	assert(fs < FS_END);
	return SIZE_TO_NAME[fs];
}

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

void InitializeUnicodeGlyphMap();
void SetUnicodeGlyph(FontSize size, char32_t key, SpriteID sprite);

uint GetFontCacheFontSize(FontSize fs);
std::string GetFontCacheFontName(FontSize fs);
void InitFontCache(bool monospace);
void UninitFontCache();

bool GetFontAAState();
void SetFont(FontSize fontsize, const std::string &font, uint size);

#endif /* FONTCACHE_H */
