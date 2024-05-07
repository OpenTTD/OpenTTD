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

#include "safeguards.h"

/** Default heights for the different sizes of fonts. */
static const int _default_font_height[FS_END]   = {10, 6, 18, 10};
static const int _default_font_ascender[FS_END] = { 8, 5, 15,  8};

FontCacheSettings _fcsettings;

/**
 * Create a new font cache.
 * @param fs The size of the font.
 */
FontCache::FontCache(FontSize fs) : parent(FontCache::Get(fs)), fs(fs), height(_default_font_height[fs]),
		ascender(_default_font_ascender[fs]), descender(_default_font_ascender[fs] - _default_font_height[fs]),
		units_per_em(1)
{
	assert(this->parent == nullptr || this->fs == this->parent->fs);
	FontCache::caches[this->fs] = this;
	Layouter::ResetFontCache(this->fs);
}

/** Clean everything up. */
FontCache::~FontCache()
{
	assert(this->fs == this->parent->fs);
	FontCache::caches[this->fs] = this->parent;
	Layouter::ResetFontCache(this->fs);
}

int FontCache::GetDefaultFontHeight(FontSize fs)
{
	return _default_font_height[fs];
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


/* static */ FontCache *FontCache::caches[FS_END];

/* static */ void FontCache::InitializeFontCaches()
{
	for (FontSize fs = FS_BEGIN; fs != FS_END; fs++) {
		if (FontCache::caches[fs] == nullptr) new SpriteFontCache(fs); /* FontCache inserts itself into to the cache. */
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
		_fcsettings = backup;
	} else {
		InitFontCache(true);
	}

	LoadStringWidthTable();
	UpdateAllVirtCoords();
	ReInitAllWindows(true);

	if (_save_config) SaveToConfig();
}

#ifdef WITH_FREETYPE
extern void LoadFreeTypeFont(FontSize fs);
extern void UninitFreeType();
#elif defined(_WIN32)
extern void LoadWin32Font(FontSize fs);
#elif defined(WITH_COCOA)
extern void LoadCoreTextFont(FontSize fs);
#endif

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
 * (Re)initialize the font cache related things, i.e. load the non-sprite fonts.
 * @param monospace Whether to initialise the monospace or regular fonts.
 */
void InitFontCache(bool monospace)
{
	FontCache::InitializeFontCaches();

	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		if (monospace != (fs == FS_MONO)) continue;

		FontCache *fc = FontCache::Get(fs);
		if (fc->HasParent()) delete fc;

#ifdef WITH_FREETYPE
		LoadFreeTypeFont(fs);
#elif defined(_WIN32)
		LoadWin32Font(fs);
#elif defined(WITH_COCOA)
		LoadCoreTextFont(fs);
#endif
	}
}

/**
 * Free everything allocated w.r.t. fonts.
 */
void UninitFontCache()
{
	for (FontSize fs = FS_BEGIN; fs < FS_END; fs++) {
		FontCache *fc = FontCache::Get(fs);
		if (fc->HasParent()) delete fc;
	}

#ifdef WITH_FREETYPE
	UninitFreeType();
#endif /* WITH_FREETYPE */
}

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(WITH_FONTCONFIG) && !defined(WITH_COCOA)

bool SetFallbackFont(FontCacheSettings *, const std::string &, int, MissingGlyphSearcher *) { return false; }
#endif /* !defined(_WIN32) && !defined(__APPLE__) && !defined(WITH_FONTCONFIG) && !defined(WITH_COCOA) */
