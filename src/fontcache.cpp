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
#include "gfx_layout.h"
#include "fontcache/spritefontcache.h"
#include "openttd.h"
#include "settings_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "window_func.h"
#include "fileio_func.h"

#include <regex>

#include "safeguards.h"

/** Default heights for the different sizes of fonts. */
static const int _default_font_height[FS_END]   = {10, 6, 18, 10};
static const int _default_font_ascender[FS_END] = { 8, 5, 15,  8};

FontCacheSettings _fcsettings;

/**
 * Create a new font cache.
 * @param fs The size of the font.
 */
FontCache::FontCache(FontSize fs) : fs(fs), height(_default_font_height[fs]),
		ascender(_default_font_ascender[fs]), descender(_default_font_ascender[fs] - _default_font_height[fs])
{
	/* Find an empty font cache slot. */
	auto it = std::find(std::begin(FontCache::caches), std::end(FontCache::caches), nullptr);
	if (it == std::end(FontCache::caches)) it = FontCache::caches.insert(it, nullptr);

	/* Register this font cache in the slot. */
	it->reset(this);

	/* Set up our font index and make us the default font cache for this font size. */
	this->font_index = static_cast<FontIndex>(std::distance(std::begin(FontCache::caches), it));
	FontCache::default_font_index[fs] = this->font_index;

	Layouter::ResetFontCache(this->fs);
}

/** Clean everything up. */
FontCache::~FontCache()
{
	Layouter::ResetFontCache(this->fs);
}

int FontCache::GetDefaultFontHeight(FontSize fs)
{
	return _default_font_height[fs];
}

/* static */ void FontCache::UpdateCharacterHeight(FontSize fs)
{
	FontCache::max_height[fs] = 0;
	for (const auto &fc : FontCache::caches) {
		if (fc == nullptr || fc->fs != fs) continue;
		FontCache::max_height[fs] = std::max(FontCache::max_height[fs], fc->height);
	}
}

/**
 * Get height of a character for a given font size.
 * @param size Font size to get height of
 * @return     Height of characters in the given font (pixels)
 */
int GetCharacterHeight(FontSize size)
{
	return FontCache::GetCharacterHeight(size);
}

/* static */ FontCache::FontCaches FontCache::caches;
/* static */ std::array<int, FS_END> FontCache::max_height{};
/* static */ std::array<std::unordered_map<char32_t, FontIndex>, FS_END> FontCache::character_to_fontcache;
/* static */ std::array<FontIndex, FS_END> FontCache::sprite_font_index{};
/* static */ std::array<FontIndex, FS_END> FontCache::default_font_index{};

/* static */ void FontCache::InitializeFontCaches()
{
}

/* Check if a glyph should be rendered with anti-aliasing. */
bool GetFontAAState()
{
	/* AA is only supported for 32 bpp */
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 32) return false;

	return _fcsettings.global_aa;
}

void SetFont(FontSize fontsize, const std::string &font, uint size)
{
	FontCacheSubSetting *setting = GetFontCacheSubSetting(fontsize);
	bool changed = false;

	if (setting->font != font) {
		setting->font = font;
		changed = true;
	}

	if (setting->size != size) {
		setting->size = size;
		changed = true;
	}

	if (!changed) return;

	if (fontsize != FS_MONO) {
		/* Check if fallback fonts are needed. */
		CheckForMissingGlyphs();
	} else {
		InitFontCache(true);
	}

	LoadStringWidthTable();
	UpdateAllVirtCoords();
	ReInitAllWindows(true);

	if (_save_config) SaveToConfig();
}

#ifdef WITH_FREETYPE
extern void LoadFreeTypeFont(FontSize fs, bool search, const std::string &font_name, std::span<const uint8_t> os_handle);
extern void UninitFreeType();
#elif defined(_WIN32)
extern void LoadWin32Font(FontSize fs, bool search, const std::string &font_name, std::span<const uint8_t> os_handle);
#elif defined(WITH_COCOA)
extern void LoadCoreTextFont(FontSize fs, bool search, const std::string &font_name, std::span<const uint8_t> os_handle);
#endif

/**
 * Test if a font setting uses the default font.
 * @return true iff the font is not configured and no fallback font data is present.
 */
static bool IsDefaultFont(const FontCacheSubSetting &setting)
{
	return setting.font.empty();
}

/**
 * Get the scalable font size to use for a FontSize.
 * @param fs FontSize to get the scalable font size for.
 * @return Scalable font size to use.
 */
uint GetFontCacheFontSize(FontSize fs)
{
	const FontCacheSubSetting &setting = *GetFontCacheSubSetting(fs);
	return IsDefaultFont(setting) ? FontCache::GetDefaultFontHeight(fs) : setting.size;
}

#if defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA)
/**
 * Get name of default font file for a given font size.
 * @param fs Font size.
 * @return Name of default font file.
 */
static std::string GetDefaultTruetypeFont(FontSize fs)
{
	switch (fs) {
		case FS_NORMAL: return "OpenTTD-Sans.ttf";
		case FS_SMALL: return "OpenTTD-Small.ttf";
		case FS_LARGE: return "OpenTTD-Serif.ttf";
		case FS_MONO: return "OpenTTD-Mono.ttf";
		default: NOT_REACHED();
	}
}
#endif /* defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA) */

/**
 * Get path of default font file for a given font size.
 * @param fs Font size.
 * @return Full path of default font file.
 */
std::string GetDefaultTruetypeFontFile([[maybe_unused]] FontSize fs)
{
#if defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA)
	/* Find font file. */
	return FioFindFullPath(BASESET_DIR, GetDefaultTruetypeFont(fs));
#else
	return {};
#endif /* defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA) */
}

/**
 * Load a font for any platform.
 * @param fs FontSize of font.
 * @param load_type Type of font (for debug only)
 * @param search Whether to search for the font.
 * @param font Font name, or filename, or path to a font.
 * @param os_handle OS-specific data.
 */
static void LoadFont(FontSize fs, std::string_view load_type, [[maybe_unused]] bool search, const std::string &font, [[maybe_unused]] std::span<uint8_t> os_handle)
{
	if (font.empty()) return;

	Debug(fontcache, 2, "InitFontCache: Adding '{}' as {} for {} font", font, load_type, FontSizeToName(fs));

#ifdef WITH_FREETYPE
	LoadFreeTypeFont(fs, search, font, os_handle);
#elif defined(_WIN32)
	LoadWin32Font(fs, search, font, os_handle);
#elif defined(WITH_COCOA)
	LoadCoreTextFont(fs, search, font, os_handle);
#endif
}

/**
 * Get font to use for a given font size.
 * @param fs Font size.
 * @return If configured, the font name to use, or the path of the default TrueType font if sprites are not preferred.
 */
std::string GetFontCacheFontName(FontSize fs)
{
	const FontCacheSubSetting *settings = GetFontCacheSubSetting(fs);
	if (!settings->font.empty()) return settings->font;
	if (_fcsettings.prefer_sprite) return {};
	return GetDefaultTruetypeFontFile(fs);
}

/**
 * (Re)initialize the font cache related things, i.e. load the non-sprite fonts.
 * @param monospace Whether to initialise the monospace or regular fonts.
 */
void InitFontCache(bool monospace)
{
	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		if (monospace != (fs == FS_MONO)) continue;

		Layouter::ResetFontCache(fs);
		FontCache::character_to_fontcache[fs].clear();
		FontCache::sprite_font_index[fs] = INVALID_FONT_INDEX;
		FontCache::default_font_index[fs] = INVALID_FONT_INDEX;
	}

	/* Remove all existing FontCaches. */
	for (auto it = std::begin(FontCache::caches); it != std::end(FontCache::caches); ++it) {
		if (*it == nullptr) continue;
		if (monospace != ((*it)->fs == FS_MONO)) continue;
		it->reset();
	}

	std::regex re("([^;]+)");

	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		if (monospace != (fs == FS_MONO)) continue;

		FontCacheSubSetting *setting = GetFontCacheSubSetting(fs);

		for (auto &fallback : setting->fallback_fonts) {
			LoadFont(fs, "fallback", false, fallback.first, fallback.second);
		}

		using re_iterator = std::regex_iterator<std::string::const_iterator>;
		re_iterator rit(setting->font.begin(), setting->font.end(), re);
		re_iterator rend;
		std::vector<std::string> result;
		std::transform(rit, rend, std::back_inserter(result), [](const auto &it) { return it[1]; });

		if (std::find(result.begin(), result.end(), "default") == result.end()) result.push_back("default");

		for (auto it = result.rbegin(); it != result.rend(); ++it) {
			std::string &f = *it;
			StrTrimInPlace(f);
			if (f == "default") {
				new SpriteFontCache(fs);
				if (!_fcsettings.prefer_sprite) {
					LoadFont(fs, "default", false, GetDefaultTruetypeFontFile(fs), {});
				}
			} else {
				LoadFont(fs, "configured", true, f, {});
			}
		}
	}
}

/**
 * Free everything allocated w.r.t. fonts.
 */
void UninitFontCache()
{
	FontCache::caches.clear();

#ifdef WITH_FREETYPE
	UninitFreeType();
#endif /* WITH_FREETYPE */
}

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(WITH_FONTCONFIG) && !defined(WITH_COCOA)

bool SetFallbackFont(const std::string &, int, MissingGlyphSearcher *) { return false; }
#endif /* !defined(_WIN32) && !defined(__APPLE__) && !defined(WITH_FONTCONFIG) && !defined(WITH_COCOA) */
