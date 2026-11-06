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
/* static */ const EnumIndexArray<int, FontSize, FontSize::End> FontCache::DEFAULT_FONT_HEIGHT{10, 6, 18, 10};
/** Default unscaled ascenders for the different sizes of fonts. */
/* static */ const EnumIndexArray<int, FontSize, FontSize::End> FontCache::DEFAULT_FONT_ASCENDER{8, 6, 15, 8};

FontCacheSettings _fcsettings;

/**
 * Try loading a font with any fontcache factory.
 * @param fs Font size to load.
 * @param fonttype Font type requested.
 * @param search Set if searching for the font.
 * @param font_name Font name to load.
 * @param os_handle Font handle to load.
 * @return FontCache of the font if loaded, or nullptr.
 */
/* static */ std::unique_ptr<FontCache> FontProviderManager::LoadFont(FontSize fs, FontType fonttype, bool search, const std::string &font_name, const std::any &os_handle)
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
 * @param language_isocode the language, e.g. en_GB.
 * @param callback The function to call to check for missing glyphs.
 * @return true if a font has been set, false otherwise.
 */
/* static */ bool FontProviderManager::FindFallbackFont(const std::string &language_isocode, MissingGlyphSearcher *callback)
{
	return std::ranges::any_of(FontProviderManager::GetProviders(),
		[&](auto *provider) { return provider->FindFallbackFont(language_isocode, callback); });
}

int FontCache::GetDefaultFontHeight(FontSize fs)
{
	return FontCache::DEFAULT_FONT_HEIGHT[fs];
}

/**
 * Update the character height for a given font size.
 * @param fs The font size.
 */
/* static */ void FontCache::UpdateCharacterHeight(FontSize fs)
{
	FontMetrics &metrics = FontCache::metrics[fs];

	int ascender = 0;
	int descender = 0;

	for (const auto &fc : FontCache::caches) {
		if (fc == nullptr || fc->fs != fs) continue;
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
	if (height == 0) height = ScaleGUITrad(FontCache::GetDefaultFontHeight(FontSize::Monospace));
	return height;
}

/**
 * Initialise font caches with the base sprite font cache for all sizes.
 */
/* static */ void FontCache::InitializeFontCaches()
{
	for (FontSize fs : EnumRange(FontSize::End)) {
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

	if (fontsize != FontSize::Monospace) {
		/* Try to reload only the modified font. */
		CheckForMissingGlyphs(fontsize);
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
 * @param setting The setting to consider.
 * @return true iff the font is not configured and no fallback font data is present.
 */
static bool IsDefaultFont(const FontCacheSubSetting &setting)
{
	return setting.font.empty() && setting.fallback_fonts.empty();
}

/**
 * Get the scalable font size to use for a FontSize.
 * @param fs FontSize to get the scalable font size for.
 * @return Scalable font size to use.
 */
uint GetFontCacheFontSize(FontSize fs)
{
	const FontCacheSubSetting &setting = *GetFontCacheSubSetting(fs);
	return IsDefaultFont(setting) && setting.size == 0 ? FontCache::GetDefaultFontHeight(fs) : setting.size;
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
		case FontSize::Normal: return "OpenTTD-Sans.ttf";
		case FontSize::Small: return "OpenTTD-Small.ttf";
		case FontSize::Large: return "OpenTTD-Serif.ttf";
		case FontSize::Monospace: return "OpenTTD-Mono.ttf";
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
	return FioFindFullPath(Subdirectory::Baseset, GetDefaultTruetypeFont(fs));
#else
	return {};
#endif /* defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA) */
}

/**
 * Register a font into the list of font caches.
 * @note Ownership of the font will move to the font caches.
 * @param fc The font cache.
 * @param load_reason The reason for loading the font.
 */
/* static */ void FontCache::Register(std::unique_ptr<FontCache> &&fc, FontLoadReason load_reason)
{
	if (fc == nullptr) return;

	/* Find an empty font cache slot. */
	auto it = std::find(std::begin(FontCache::caches), std::end(FontCache::caches), nullptr);
	if (it == std::end(FontCache::caches)) {
		/* No empty slot found. */
		if (FontCache::caches.size() == INVALID_FONT_INDEX) {
			Debug(Facility::Fontcache, Severity::Error, "Too many fonts loaded while trying to register '{}'", fc->GetFontName());
			return;
		}
		it = FontCache::caches.insert(it, nullptr);
	}

	/* Set up our font index and make us the default font cache for this font size. */
	fc->font_index = static_cast<FontIndex>(std::distance(std::begin(FontCache::caches), it));
	fc->load_reason = load_reason;
	FontCache::default_font_index[fc->fs] = fc->font_index;

	/* Register this font cache in the slot. */
	*it = std::move(fc);
}

/**
 * Add a fallback font, with optional OS-specific handle.
 * @param fontsizes Fontsizes to add fallback to.
 * @param load_reason The reason for loading the font.
 * @param name Name of font to add.
 * @param os_handle OS-specific handle or data of font.
 * @return true iff the fallback was added.
 */
/* static */ bool FontCache::AddFallback(FontSizes fontsizes, FontLoadReason load_reason, std::string_view name, const std::any &os_handle)
{
	for (FontSize fs : fontsizes) {
		auto *setting = GetFontCacheSubSetting(fs);
		if (setting->font == name) continue;
		if (std::ranges::find(setting->fallback_fonts, name, &FontCacheSubSetting::FontCacheFallback::name) == std::end(setting->fallback_fonts)) {
			setting->fallback_fonts.emplace_back(load_reason, std::string{name}, os_handle);
		} else {
			fontsizes.Reset(fs);
		}
	}

	return fontsizes.Any();
}

/**
 * Test a fallback font, with optional OS-specific handle, for specific glyphs.
 * @param fontsizes The fontsizes the font will be used for.
 * @param glyphs Glyphs to search for.
 * @param name Name of font to test.
 * @param os_handle OS-specific handle or data of font.
 * @return true iff the font can be used.
 */
/* static */ bool FontCache::TryFallback(FontSizes fontsizes, const std::set<char32_t> &glyphs, const std::string &name, const std::any &os_handle)
{
	for (FontSize fs : fontsizes) {
		const auto *setting = GetFontCacheSubSetting(fs);
		if (setting->font == name) continue;
		if (std::ranges::find(setting->fallback_fonts, name, &FontCacheSubSetting::FontCacheFallback::name) == std::end(setting->fallback_fonts)) continue;
		fontsizes.Reset(fs);
	}

	if (fontsizes.None()) return false;

	/* Load the font without registering it. The font size does not matter. */
	auto fc = FontProviderManager::LoadFont(FontSize::Normal, FontType::TrueType, false, name, os_handle);
	if (fc == nullptr) return false;

	size_t matching_chars = 0;
	for (const char32_t &c : glyphs) {
		if (fc->MapCharToGlyph(c) != 0) ++matching_chars;
	}

	if (matching_chars < glyphs.size()) {
		Debug(Facility::Fontcache, Severity::Info, "Font \"{}\" misses {} glyph(s)", name, glyphs.size() - matching_chars);
		return false;
	}

	return true;
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

/* static */ void FontCache::LoadFallbackFonts(FontSize fs)
{
	const FontCacheSubSetting *setting = GetFontCacheSubSetting(fs);
	for (auto it = setting->fallback_fonts.rbegin(); it != setting->fallback_fonts.rend(); ++it) {
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

		/* Load configured fonts in reverse order so that the first entry has priority. */
		for (auto it = fontnames.rbegin(); it != fontnames.rend(); ++it) {
			if (*it == DEFAULT_FONT) {
				FontCache::LoadDefaultFonts(fs);
			} else if (*it == FALLBACK_FONT) {
				FontCache::LoadFallbackFonts(fs);
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
