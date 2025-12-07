/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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

/** Default unscaled heights for the different sizes of fonts. */
/* static */ const int FontCache::DEFAULT_FONT_HEIGHT[FS_END] = {10, 6, 18, 10};
/** Default unscaled ascenders for the different sizes of fonts. */
/* static */ const int FontCache::DEFAULT_FONT_ASCENDER[FS_END] = {8, 6, 15, 8};

FontCacheSettings _fcsettings;

/**
 * Try loading a font with any fontcache factory.
 * @param fs Font size to load.
 * @param fonttype Font type requested.
 * @return FontCache of the font if loaded, or nullptr.
 */
/* static */ std::unique_ptr<FontCache> FontProviderManager::LoadFont(FontSize fs, FontType fonttype, bool search, const std::string &font_name, std::span<const std::byte> os_handle)
{
	for (auto &provider : FontProviderManager::GetProviders()) {
		auto fc = provider->LoadFont(fs, fonttype, search, font_name, os_handle);
		if (fc != nullptr) return fc;
	}

	return nullptr;
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
/* static */ bool FontProviderManager::FindFallbackFont(const std::string &language_isocode, FontSizes fontsizes, MissingGlyphSearcher *callback)
{
	return std::ranges::any_of(FontProviderManager::GetProviders(),
		[&](auto *provider) { return provider->FindFallbackFont(language_isocode, fontsizes, callback); });
}

int FontCache::GetDefaultFontHeight(FontSize fs)
{
	return FontCache::DEFAULT_FONT_HEIGHT[fs];
}

/* static */ void FontCache::UpdateCharacterHeight(FontSize fs)
{
	FontMetrics &metrics = FontCache::metrics[fs];

	int ascender = 0;
	int descender = 0;

	for (const auto &fc : FontCache::caches) {
		if (fc == nullptr || fc->fs != fs) continue;
		if (fc->load_reason == FontLoadReason::MissingFallback) continue; // Avoid dynamically loaded fonts affecting widget sizes.
		ascender = std::max(ascender, fc->ascender);
		descender = std::min(descender, fc->descender);
	}

	if (ascender == 0 && descender == 0) {
		/* It's possible that no font is loaded yet, in which case use default values. */
		ascender = ScaleGUITrad(FontCache::DEFAULT_FONT_ASCENDER[fs]);
		descender = ScaleGUITrad(FontCache::DEFAULT_FONT_ASCENDER[fs] - FontCache::DEFAULT_FONT_HEIGHT[fs]);
	}

	metrics.height = ascender - descender;
	metrics.baseline = ascender;
}

int FontCache::GetGlyphYOffset()
{
	return FontCache::GetFontBaseline(this->fs) - this->ascender;
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
/* static */ std::array<FontCache::FontMetrics, FS_END> FontCache::metrics{};
/* static */ std::array<FontIndex, FS_END> FontCache::default_font_index{};

/**
 * Initialise font caches with the base sprite font cache for all sizes.
 */
/* static */ void FontCache::InitializeFontCaches()
{
	for (FontSize fs : FONTSIZES_ALL) {
		UpdateCharacterHeight(fs);
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
		FontCache::LoadFontCaches(fontsize);
	}

	FontCache::UpdateCharacterHeight(fontsize);
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

/* static */ void FontCache::Register(std::unique_ptr<FontCache> &&fc, FontLoadReason load_reason)
{
	if (fc == nullptr) return;

	FontSize fs = fc->fs;

	/* Find an empty font cache slot. */
	auto it = std::find(std::begin(FontCache::caches), std::end(FontCache::caches), nullptr);
	if (it == std::end(FontCache::caches)) it = FontCache::caches.insert(it, nullptr);

	/* Set up our font index and make us the default font cache for this font size. */
	fc->font_index = static_cast<FontIndex>(std::distance(std::begin(FontCache::caches), it));
	fc->load_reason = load_reason;
	FontCache::default_font_index[fs] = fc->font_index;

	/* Register this font cache in the slot. */
	*it = std::move(fc);
}

/**
 * Add a fallback font, with optional OS-specific handle.
 * @param fontsizes Fontsizes to add fallback to.
 * @param name Name of font to add.
 * @param handle OS-specific handle or data of font.
 */
/* static */ void FontCache::AddFallback(FontSizes fontsizes, FontLoadReason load_reason, std::string_view name, std::span<const std::byte> os_data)
{
	for (FontSize fs : fontsizes) {
		GetFontCacheSubSetting(fs)->fallback_fonts.emplace_back(load_reason, std::string{name}, std::vector<std::byte>{os_data.begin(), os_data.end()});
	}
}

/* static */ void FontCache::LoadDefaultFonts(FontSize fs)
{
	/* Load the sprite font, even if it's not preferred. */
	FontCache::Register(FontProviderManager::LoadFont(fs, FontType::Sprite, false, {}, {}), FontLoadReason::Default);
	if (!_fcsettings.prefer_sprite) {
		/* Load the default truetype font if sprite font is not preferred. */
		FontCache::Register(FontProviderManager::LoadFont(fs, FontType::TrueType, false, GetDefaultTruetypeFontFile(fs), {}), FontLoadReason::Default);
	}
}

/* static */ void FontCache::LoadFallbackFonts(FontSize fs, FontLoadReason load_reason)
{
	const FontCacheSubSetting *setting = GetFontCacheSubSetting(fs);
	for (auto it = setting->fallback_fonts.rbegin(); it != setting->fallback_fonts.rend(); ++it) {
		if (it->load_reason != load_reason) continue;
		FontCache::Register(FontProviderManager::LoadFont(fs, FontType::TrueType, false, it->name, it->os_handle), it->load_reason);
	}
}

/**
 * (Re)initialize the font cache related things, i.e. load the non-sprite fonts.
 * @param fontsizes Font sizes to be initialised.
 */
/* static */ void FontCache::LoadFontCaches(FontSizes fontsizes)
{
	static constexpr std::string_view FALLBACK_FONT = "fallback";
	static constexpr std::string_view DEFAULT_FONT = "default";
	static constexpr std::initializer_list<std::string_view> extra_prefer_default = {DEFAULT_FONT, FALLBACK_FONT};
	static constexpr std::initializer_list<std::string_view> extra_prefer_fallback = {FALLBACK_FONT, DEFAULT_FONT};

	for (FontSize fs : fontsizes) {
		Layouter::ResetFontCache(fs);
		FontCache::default_font_index[fs] = INVALID_FONT_INDEX;
	}

	/* Remove all existing FontCaches. */
	if (fontsizes == FONTSIZES_ALL) {
		FontCache::caches.clear();
	} else {
		for (auto it = std::begin(FontCache::caches); it != std::end(FontCache::caches); ++it) {
			if (*it == nullptr) continue;
			if (!fontsizes.Test((*it)->fs)) continue;
			it->reset();
		}
	}

	for (FontSize fs : fontsizes) {
		/* Parse configured fonts, separated by ';' into a list. */
		std::vector<std::string_view> fontnames;
		StringConsumer consumer(GetFontCacheSubSetting(fs)->font);
		do {
			auto fontname = StrTrimView(consumer.ReadUntilChar(';', StringConsumer::SKIP_ONE_SEPARATOR), " \t");
			if (!fontname.empty()) fontnames.push_back(fontname);
		} while (consumer.AnyBytesLeft());

		/* Add the default and fallback fonts as lowest priority if not manually specified. */
		for (const auto &extra_font : _fcsettings.prefer_default ? extra_prefer_default : extra_prefer_fallback) {
			if (std::ranges::find(fontnames, extra_font) == std::end(fontnames)) fontnames.push_back(extra_font);
		}

		/* First load fonts for missing glyphs discovered during string formatting. */
		FontCache::LoadFallbackFonts(fs, FontLoadReason::MissingFallback);

		/* Load configured fonts in reverse order so that the first entry has priority. */
		for (auto it = fontnames.rbegin(); it != fontnames.rend(); ++it) {
			if (*it == DEFAULT_FONT) {
				FontCache::LoadDefaultFonts(fs);
			} else if (*it == FALLBACK_FONT) {
				FontCache::LoadFallbackFonts(fs, FontLoadReason::LanguageFallback);
			} else {
				FontCache::Register(FontProviderManager::LoadFont(fs, FontType::TrueType, true, std::string{*it}, {}), FontLoadReason::Configured);
			}
		}

		FontCache::UpdateCharacterHeight(fs);
	}
}

/**
 * Clear cached information for the specified font caches.
 * @param fontsizes Font sizes to clear.
 */
/* static */ void FontCache::ClearFontCaches(FontSizes fontsizes)
{
	for (const auto &fc : FontCache::caches) {
		if (fc == nullptr) continue;
		if (!fontsizes.Test(fc->GetSize())) continue;
		fc->ClearFontCache();
	}

	for (FontSize fs : fontsizes) {
		FontCache::UpdateCharacterHeight(fs);
	}
}

/**
 * Free everything allocated w.r.t. fonts.
 */
/* static */ void FontCache::UninitializeFontCaches()
{
	FontCache::caches.clear();
}
