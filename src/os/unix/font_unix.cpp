/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file font_unix.cpp Functions related to font handling on Unix/Fontconfig. */

#include "../../stdafx.h"

#include "../../misc/autorelease.hpp"
#include "../../debug.h"
#include "../../fontcache.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "font_unix.h"

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "../../safeguards.h"

extern FT_Library _ft_library;

/**
 * Get a FontConfig-style string from a std::string.
 * @param str String to be passed to FontConfig.
 * @return FontConfig-style string.
 */
static const FcChar8 *ToFcString(const std::string &str)
{
	return reinterpret_cast<const FcChar8 *>(str.c_str());
}

/**
 * Get a C-style string from a FontConfig-style string.
 * @param str String from FontConfig.
 * @return C-style string.
 */
static const char *FromFcString(const FcChar8 *str)
{
	return reinterpret_cast<const char *>(str);
}

/**
 * Split the font name into the font family and style. These fields are separated by a comma,
 * but the style does not necessarily need to exist.
 * @param font_name The font name.
 * @return The font family and style.
 */
static std::tuple<std::string, std::string> SplitFontFamilyAndStyle(std::string_view font_name)
{
	auto separator = font_name.find(',');
	if (separator == std::string_view::npos) return { std::string(font_name), std::string() };

	auto begin = font_name.find_first_not_of("\t ", separator + 1);
	if (begin == std::string_view::npos) return { std::string(font_name.substr(0, separator)), std::string() };

	return { std::string(font_name.substr(0, separator)), std::string(font_name.substr(begin)) };
}

/**
 * Load a freetype font face with the given font name.
 * @param font_name The name of the font to load.
 * @param face The face that has been found.
 * @return The error we encountered.
 */
FT_Error GetFontByFaceName(std::string_view font_name, FT_Face *face)
{
	FT_Error err = FT_Err_Cannot_Open_Resource;

	if (!FcInit()) {
		ShowInfo("Unable to load font configuration");
		return err;
	}

	auto fc_instance = AutoRelease<FcConfig, FcConfigDestroy>(FcConfigReference(nullptr));
	assert(fc_instance != nullptr);

	/* Split & strip the font's style */
	auto [font_family, font_style] = SplitFontFamilyAndStyle(font_name);

	/* Resolve the name and populate the information structure */
	auto pat = AutoRelease<FcPattern, FcPatternDestroy>(FcPatternCreate());
	if (!font_family.empty()) FcPatternAddString(pat.get(), FC_FAMILY, ToFcString(font_family));
	if (!font_style.empty()) FcPatternAddString(pat.get(), FC_STYLE, ToFcString(font_style));
	FcConfigSubstitute(nullptr, pat.get(), FcMatchPattern);
	FcDefaultSubstitute(pat.get());
	auto fs = AutoRelease<FcFontSet, FcFontSetDestroy>(FcFontSetCreate());
	FcResult result;
	FcPattern *match = FcFontMatch(nullptr, pat.get(), &result);

	if (fs == nullptr || match == nullptr) return err;

	FcFontSetAdd(fs.get(), match);

	for (FcPattern *font : std::span(fs->fonts, fs->nfont)) {
		FcChar8 *family;
		FcChar8 *style;
		FcChar8 *file;
		int32_t index;

		/* Try the new filename */
		if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch) continue;
		if (FcPatternGetString(font, FC_FAMILY, 0, &family) != FcResultMatch) continue;
		if (FcPatternGetString(font, FC_STYLE, 0, &style) != FcResultMatch) continue;
		if (FcPatternGetInteger(font, FC_INDEX, 0, &index) != FcResultMatch) continue;

		/* The correct style? */
		if (!font_style.empty() && !StrEqualsIgnoreCase(font_style, FromFcString(style))) continue;

		/* Font config takes the best shot, which, if the family name is spelled
		 * wrongly a 'random' font, so check whether the family name is the
		 * same as the supplied name */
		if (StrEqualsIgnoreCase(font_family, FromFcString(family))) {
			err = FT_New_Face(_ft_library, FromFcString(file), index, face);
			if (err == FT_Err_Ok) return err;
		}
	}

	return err;
}

/**
 * Get distance between font weight and preferred font weights.
 * @param weight Font weight from FontConfig.
 * @return Distance from preferred weight, where lower values are preferred.
 */
static int GetPreferredWeightDistance(int weight)
{
	/* Prefer a font between normal and medium weight. */
	static constexpr int PREFERRED_WEIGHT_MIN = FC_WEIGHT_NORMAL;
	static constexpr int PREFERRED_WEIGHT_MAX = FC_WEIGHT_MEDIUM;

	if (weight < PREFERRED_WEIGHT_MIN) return std::abs(weight - PREFERRED_WEIGHT_MIN);
	if (weight > PREFERRED_WEIGHT_MAX) return weight - PREFERRED_WEIGHT_MAX;
	return 0;
}

bool FontConfigFindFallbackFont(FontCacheSettings *settings, const std::string &language_isocode, MissingGlyphSearcher *callback)
{
	bool ret = false;

	if (!FcInit()) return ret;

	auto fc_instance = AutoRelease<FcConfig, FcConfigDestroy>(FcConfigReference(nullptr));
	assert(fc_instance != nullptr);

	/* Fontconfig doesn't handle full language isocodes, only the part
	 * before the _ of e.g. en_GB is used, so "remove" everything after
	 * the _. */
	std::string lang = fmt::format(":lang={}", language_isocode.substr(0, language_isocode.find('_')));

	/* First create a pattern to match the wanted language. */
	auto pat = AutoRelease<FcPattern, FcPatternDestroy>(FcNameParse(ToFcString(lang)));
	/* We only want to know these attributes. */
	auto os = AutoRelease<FcObjectSet, FcObjectSetDestroy>(FcObjectSetBuild(FC_FILE, FC_INDEX, FC_SPACING, FC_SLANT, FC_WEIGHT, nullptr));
	/* Get the list of filenames matching the wanted language. */
	auto fs = AutoRelease<FcFontSet, FcFontSetDestroy>(FcFontList(nullptr, pat.get(), os.get()));

	if (fs == nullptr) return ret;

	int best_weight = -1;
	const char *best_font = nullptr;
	int best_index = 0;

	for (FcPattern *font : std::span(fs->fonts, fs->nfont)) {
		FcChar8 *file = nullptr;
		FcResult res = FcPatternGetString(font, FC_FILE, 0, &file);
		if (res != FcResultMatch || file == nullptr) continue;

		/* Get a font with the right spacing .*/
		int value = 0;
		FcPatternGetInteger(font, FC_SPACING, 0, &value);
		if (callback->Monospace() != (value == FC_MONO) && value != FC_DUAL) continue;

		/* Do not use those that explicitly say they're slanted. */
		FcPatternGetInteger(font, FC_SLANT, 0, &value);
		if (value != 0) continue;

		/* We want a font near to medium weight. */
		FcPatternGetInteger(font, FC_WEIGHT, 0, &value);
		int weight = GetPreferredWeightDistance(value);
		if (best_weight != -1 && weight > best_weight) continue;

		/* Possible match based on attributes, get index. */
		int32_t index;
		res = FcPatternGetInteger(font, FC_INDEX, 0, &index);
		if (res != FcResultMatch) continue;

		callback->SetFontNames(settings, FromFcString(file), &index);

		bool missing = callback->FindMissingGlyphs();
		Debug(fontcache, 1, "Font \"{}\" misses{} glyphs", FromFcString(file), missing ? "" : " no");

		if (!missing) {
			best_weight = weight;
			best_font = FromFcString(file);
			best_index = index;
		}
	}

	if (best_font == nullptr) return false;

	callback->SetFontNames(settings, best_font, &best_index);
	FontCache::LoadFontCaches(callback->Monospace() ? FontSizes{FS_MONO} : FONTSIZES_REQUIRED);
	return true;
}
