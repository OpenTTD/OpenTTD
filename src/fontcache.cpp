/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file fontcache.cpp Cache for characters from fonts. */

#include "stdafx.h"
#include "fontcache.h"
#include "blitter/factory.hpp"
#include "gfx_layout.h"
#include "openttd.h"
#include "settings_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "window_func.h"
#include "fileio_func.h"

#include "safeguards.h"

/** Default unscaled heights for the different sizes of fonts. */
/* static */ const int FontCache::DEFAULT_FONT_HEIGHT[FS_END] = {10, 6, 18, 10};
/** Default unscaled ascenders for the different sizes of fonts. */
/* static */ const int FontCache::DEFAULT_FONT_ASCENDER[FS_END] = {8, 5, 15, 8};

FontCacheSettings _fcsettings;

/**
 * Try loading a font with any fontcache factory.
 * @param fs Font size to load.
 * @param fonttype Font type requested.
 * @return FontCache of the font if loaded, or nullptr.
 */
/* static */ std::unique_ptr<FontCache> FontProviderManager::LoadFont(FontSize fs, FontType fonttype)
{
	for (auto &provider : FontProviderManager::GetProviders()) {
		auto fc = provider->LoadFont(fs, fonttype);
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
/* static */ bool FontProviderManager::FindFallbackFont(FontCacheSettings *settings, const std::string &language_isocode, MissingGlyphSearcher *callback)
{
	return std::ranges::any_of(FontProviderManager::GetProviders(),
		[&](auto *provider) { return provider->FindFallbackFont(settings, language_isocode, callback); });
}

int FontCache::GetDefaultFontHeight(FontSize fs)
{
	return FontCache::DEFAULT_FONT_HEIGHT[fs];
}

/**
 * Get the font name of a given font size.
 * @param fs The font size to look up.
 * @return The font name.
 */
std::string FontCache::GetName(FontSize fs)
{
	FontCache *fc = FontCache::Get(fs);
	if (fc != nullptr) {
		return fc->GetFontName();
	} else {
		return "[NULL]";
	}
}


/**
 * Get height of a character for a given font size.
 * @param size Font size to get height of
 * @return     Height of characters in the given font (pixels)
 */
int GetCharacterHeight(FontSize size)
{
	return FontCache::Get(size)->GetHeight();
}


/* static */ std::array<std::unique_ptr<FontCache>, FS_END> FontCache::caches{};

/**
 * Initialise font caches with the base sprite font cache for all sizes.
 */
/* static */ void FontCache::InitializeFontCaches()
{
	for (FontSize fs = FS_BEGIN; fs != FS_END; fs++) {
		if (FontCache::Get(fs) != nullptr) continue;
		FontCache::Register(FontProviderManager::LoadFont(fs, FontType::Sprite));
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
		/* Try to reload only the modified font. */
		FontCacheSettings backup = _fcsettings;
		for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
			if (fs == fontsize) continue;
			FontCache *fc = FontCache::Get(fs);
			GetFontCacheSubSetting(fs)->font = fc->HasParent() ? fc->GetFontName() : "";
		}
		CheckForMissingGlyphs();
		_fcsettings = std::move(backup);
	} else {
		FontCache::LoadFontCaches(fontsize);
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
	return setting.font.empty() && setting.os_handle == nullptr;
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
static std::string GetDefaultTruetypeFontFile([[maybe_unused]] FontSize fs)
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
 * Register a FontCache for its font size.
 * @param fc FontCache to register.
 */
/* static */ void FontCache::Register(std::unique_ptr<FontCache> &&fc)
{
	if (fc == nullptr) return;

	FontSize fs = fc->fs;

	fc->parent = std::move(FontCache::caches[fs]);
	FontCache::caches[fs] = std::move(fc);
}

/**
 * (Re)initialize the font cache related things, i.e. load the non-sprite fonts.
 * @param fontsizes Font sizes to be initialised.
 */
/* static */ void FontCache::LoadFontCaches(FontSizes fontsizes)
{
	FontCache::InitializeFontCaches();

	for (FontSize fs : fontsizes) {
		Layouter::ResetFontCache(fs);

		/* Unload everything except the sprite font cache. */
		while (FontCache::Get(fs)->HasParent()) {
			FontCache::caches[fs] = std::move(FontCache::caches[fs]->parent);
		}

		FontCache::Register(FontProviderManager::LoadFont(fs, FontType::TrueType));
	}
}

/**
 * Clear cached information for the specified font caches.
 * @param fontsizes Font sizes to clear.
 */
/* static */ void FontCache::ClearFontCaches(FontSizes fontsizes)
{
	for (FontSize fs : fontsizes) {
		FontCache::Get(fs)->ClearFontCache();
	}
}

/**
 * Free everything allocated w.r.t. fonts.
 */
/* static */ void FontCache::UninitializeFontCaches()
{
	for (auto &fc : FontCache::caches) {
		fc.reset();
	}
}
