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
using FontIndex = uint8_t;

static const FontIndex INVALID_FONT_INDEX = std::numeric_limits<FontIndex>::max();

enum class FontLoadReason : uint8_t {
	Default,
	Configured,
	LanguageFallback,
	MissingFallback,
	End,
};

/** Font cache for basic fonts. */
class FontCache {
protected:
	using FontCaches = std::vector<std::unique_ptr<FontCache>>;
	static FontCaches caches;

	struct FontMetrics {
		int height = 0;
		int baseline = 0;
	};

	static std::array<FontMetrics, FS_END> metrics;
	static std::array<FontIndex, FS_END> default_font_index;

	const FontSize fs; ///< The size of the font.
	FontIndex font_index; ///< The index of the font.
	FontLoadReason load_reason; ///< Reason why the font is loaded.
	int height = 0; ///< The height of the font.
	int ascender = 0; ///< The ascender value of the font.
	int descender = 0; ///< The descender value of the font.

	FontCache(FontSize fs) : fs(fs) {}
	static void Register(std::unique_ptr<FontCache> &&fc, FontLoadReason load_reason);
	static void LoadDefaultFonts(FontSize fs);
	static void LoadFallbackFonts(FontSize fs, FontLoadReason load_reason);

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

	static inline int GetFontBaseline(FontSize fs)
	{
		return FontCache::metrics[fs].baseline;
	}

	static void AddFallback(FontSizes fontsizes, FontLoadReason load_reason, std::string_view name, std::span<const std::byte> os_data = {});

	/**
	 * Add a fallback font, with OS-specific handle.
	 * @param fontsizes Fontsizes to add fallback to.
	 * @param name Name of font to add.
	 * @param handle OS-specific handle or data of font.
	 */
	template <typename T>
	static void AddFallbackWithHandle(FontSizes fontsizes, FontLoadReason load_reason, std::string_view name, T &handle)
	{
		auto os_data = std::as_bytes(std::span(&handle, 1));
		FontCache::AddFallback(fontsizes, load_reason, name, os_data);
	}

	/**
	 * Get the FontSize of the font.
	 * @return The FontSize.
	 */
	inline FontSize GetSize() const { return this->fs; }

	inline FontIndex GetIndex() const { return this->font_index; }

	inline FontLoadReason GetFontLoadReason() const { return this->load_reason; }

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

	virtual int GetGlyphYOffset();

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
		return FontCache::metrics[fs].height;
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

	static inline FontIndex GetFontIndexForCharacter(FontSize fs, char32_t c)
	{
		for (auto it = std::rbegin(FontCache::caches); it != std::rend(FontCache::caches); ++it) {
			FontCache *fc = it->get();
			if (fc == nullptr) continue;
			if (fc->GetSize() != fs) continue;
			if (fc->MapCharToGlyph(c) == 0) continue;
			return std::distance(std::begin(FontCache::caches), std::next(it).base());
		}
		return INVALID_FONT_INDEX;
	}

	/**
	 * Is this a built-in sprite font?
	 */
	virtual bool IsBuiltInFont() = 0;
};

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

	struct FontCacheFallback {
		FontLoadReason load_reason = FontLoadReason::LanguageFallback;
		std::string name;
		std::vector<std::byte> os_handle;
	};

	std::vector<FontCacheFallback> fallback_fonts;
};

/** Settings for the four different fonts. */
struct FontCacheSettings {
	FontCacheSubSetting small;  ///< The smallest font; mostly used for zoomed out view.
	FontCacheSubSetting medium; ///< The normal font size.
	FontCacheSubSetting large;  ///< The largest font; mostly used for newspapers.
	FontCacheSubSetting mono;   ///< The mono space font used for license/readme viewers.
	bool prefer_sprite;         ///< Whether to prefer the built-in sprite font over resizable fonts.
	bool global_aa;             ///< Whether to anti alias all font sizes.
	bool prefer_default; ///< Prefer OpenTTD's default font over autodetected fallback fonts.
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

	virtual std::unique_ptr<FontCache> LoadFont(FontSize fs, FontType fonttype, bool search, const std::string &font_name, std::span<const std::byte> os_handle) const = 0;
	virtual bool FindFallbackFont(const std::string &language_isocode, FontSizes fontsizes, class MissingGlyphSearcher *callback) const = 0;
};

class FontProviderManager : ProviderManager<FontCacheFactory> {
public:
	static std::unique_ptr<FontCache> LoadFont(FontSize fs, FontType fonttype, bool search, const std::string &font_name, std::span<const std::byte> os_handle);
	static bool FindFallbackFont(const std::string &language_isocode, FontSizes fontsizes, class MissingGlyphSearcher *callback);
};

/* Implemented in spritefontcache.cpp */
void InitializeUnicodeGlyphMap();
void SetUnicodeGlyph(FontSize size, char32_t key, SpriteID sprite);

#endif /* FONTCACHE_H */
