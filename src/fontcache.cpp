/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fontcache.cpp Cache for characters from fonts. */

#include "stdafx.h"

#include "core/string_consumer.hpp"
#include "fontcache.h"
#include "blitter/factory.hpp"
#include "gfx_layout.h"
#include "openttd.h"
#include "settings_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "window_func.h"
#include "fileio_func.h"
#include "zoom_func.h"

#include "safeguards.h"

FontCacheSettings _fcsettings;

/**
 * Try loading a font with any fontcache factory.
 * @param fs Font size to load.
 * @param fonttype Font type requested.
 */
/* static */ void FontProviderManager::LoadFont(FontSize fs, FontType fonttype, bool search, const std::string &font_name, std::span<const std::byte> os_handle)
{
	for (auto &provider : FontProviderManager::GetProviders()) {
		provider->LoadFont(fs, fonttype, search, font_name, os_handle);
	}
}

/**
 * We would like to have a fallback font as the current one
 * doesn't contain all characters we need.
 * This function must set all fonts of settings.
 * @param settings the settings to overwrite the fontname of.
 * @param language_isocode the language, e.g. en_GB.
 * @param callback The function to call to check for missing glyphs.
 * @return true if a font has been set, false otherwise.
 */
/* static */ bool FontProviderManager::SetFallbackFont(const std::string &language_isocode, FontSizes  bad_mask, class MissingGlyphSearcher *callback)
{
	for (auto &provider : FontProviderManager::GetProviders()) {
		if (provider->SetFallbackFont(language_isocode, bad_mask, callback)) {
			return true;
		}
	}
	return false;
}

/**
 * Create a new font cache.
 * @param fs The size of the font.
 */
FontCache::FontCache(FontSize fs) : fs(fs)
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
	return DEFAULT_FONT_HEIGHT[fs];
}

/* static */ void FontCache::UpdateCharacterHeight(FontSize fs)
{
	FontCache::max_height[fs] = 0;
	for (const auto &fc : FontCache::caches) {
		if (fc == nullptr || fc->fs != fs) continue;
		FontCache::max_height[fs] = std::max(FontCache::max_height[fs], fc->height);
	}
	if (FontCache::max_height[fs] == 0) FontCache::max_height[fs] = GetDefaultFontHeight(fs);
}

int FontCache::GetGlyphYOffset()
{
	int fs_height = FontCache::GetCharacterHeight(this->GetSize());
	int height = this->GetAscender() - this->GetDescender();
	return (fs_height - height) / 2;
}

/**
 * Get height of a character for a given font size.
 * @param size Font size to get height of
 * @return     Height of characters in the given font (pixels)
 */
int GetCharacterHeight(FontSize size)
{
	uint height = FontCache::GetCharacterHeight(size);
	if (height == 0) height = ScaleGUITrad(FontCache::GetDefaultFontHeight(FS_MONO));
	return height;
}

/* static */ FontCache::FontCaches FontCache::caches;
/* static */ std::array<int, FS_END> FontCache::max_height{};
/* static */ std::array<FontIndex, FS_END> FontCache::default_font_index{};

/* static */ void FontCache::InitializeFontCaches()
{
	for (FontSize fs : FONTSIZES_ALL) {
		FontCache::max_height[fs] = ScaleSpriteTrad(GetDefaultFontHeight(fs));
	}
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
		InitFontCache(fontsize);
	}

	LoadStringWidthTable(fontsize);
	UpdateAllVirtCoords();
	ReInitAllWindows(true);

	if (_save_config) SaveToConfig();
}

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
void InitFontCache(FontSizes fontsizes)
{
	static constexpr std::string_view DEFAULT_FONT = "default";

	for (FontSize fs : fontsizes) {
		Layouter::ResetFontCache(fs);
		FontCache::default_font_index[fs] = INVALID_FONT_INDEX;
	}

	/* Remove all existing FontCaches. */
	for (auto it = std::begin(FontCache::caches); it != std::end(FontCache::caches); ++it) {
		if (*it == nullptr) continue;
		if (!fontsizes.Test((*it)->fs)) continue;
		it->reset();
	}

	for (FontSize fs : fontsizes) {
		const FontCacheSubSetting *setting = GetFontCacheSubSetting(fs);

		/* Add all detected fallback fonts. */
		for (auto &fallback : setting->fallback_fonts) {
			FontProviderManager::LoadFont(fs, FontType::TrueType, /*fallback.dynamic ? "missing-fallback" : "language-fallback", */ false, fallback.name, fallback.os_handle);
		}

		/* Parse configured fonts, separated by ';' into a list. */
		std::vector<std::string_view> fontnames;
		StringConsumer consumer(setting->font);
		do {
			auto fontname = StrTrimView(consumer.ReadUntilChar(';', StringConsumer::SKIP_ONE_SEPARATOR), " \t");
			if (!fontname.empty()) fontnames.push_back(fontname);
		} while (consumer.AnyBytesLeft());

		/* Add the default font as lowest priority if not manually specified. */
		if (std::ranges::find(fontnames, DEFAULT_FONT) == std::end(fontnames)) fontnames.push_back(DEFAULT_FONT);

		/* Load configured fonts in reverse order so that the first entry has priority. */
		for (auto it = fontnames.rbegin(); it != fontnames.rend(); ++it) {
			if (*it == DEFAULT_FONT) {
				/* Load the sprite font, even if it's not preferred. */
				FontProviderManager::LoadFont(fs, FontType::Sprite, /*"default"*/ false, {}, {});
				if (!_fcsettings.prefer_sprite) {
					/* Load the default truetype font if sprite not isn't preferred. */
					FontProviderManager::LoadFont(fs, FontType::TrueType, /*"default",*/ false, GetDefaultTruetypeFontFile(fs), {});
				}
			} else {
				FontProviderManager::LoadFont(fs, FontType::TrueType, /*"configured",*/ true, std::string{*it}, {});
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
}
