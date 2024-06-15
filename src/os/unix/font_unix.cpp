/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file font_unix.cpp Functions related to font handling on Unix/Fontconfig. */

#include "../../stdafx.h"
#include "../../debug.h"
#include "../../fontdetection.h"
#include "../../string_func.h"
#include "../../strings_func.h"

#include <fontconfig/fontconfig.h>

#include "safeguards.h"

#include <ft2build.h>
#include FT_FREETYPE_H

extern FT_Library _library;

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
 * Get language string for FontConfig pattern matching.
 * @param language_isocode Language's ISO code.
 * @return Language code for FontConfig.
 */
static std::string GetFontConfigLanguage(const std::string &language_isocode)
{
	/* Fontconfig doesn't handle full language isocodes, only the part
	 * before the _ of e.g. en_GB is used, so "remove" everything after
	 * the _. */
	return fmt::format(":lang={}", language_isocode.substr(0, language_isocode.find('_')));
}

FT_Error GetFontByFaceName(const char *font_name, FT_Face *face)
{
	FT_Error err = FT_Err_Cannot_Open_Resource;

	if (!FcInit()) {
		ShowInfo("Unable to load font configuration");
		return err;
	}

	auto fc_instance = FcConfigReference(nullptr);
	assert(fc_instance != nullptr);

	/* Split & strip the font's style */
	auto [font_family, font_style] = SplitFontFamilyAndStyle(font_name);

	/* Resolve the name and populate the information structure */
	FcPattern *pat = FcPatternCreate();
	if (!font_family.empty()) FcPatternAddString(pat, FC_FAMILY, reinterpret_cast<const FcChar8 *>(font_family.c_str()));
	if (!font_style.empty()) FcPatternAddString(pat, FC_STYLE, reinterpret_cast<const FcChar8 *>(font_style.c_str()));
	FcConfigSubstitute(nullptr, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	FcFontSet *fs = FcFontSetCreate();
	FcResult  result;
	FcPattern *match = FcFontMatch(nullptr, pat, &result);

	if (fs != nullptr && match != nullptr) {
		FcChar8 *family;
		FcChar8 *style;
		FcChar8 *file;
		int32_t index;
		FcFontSetAdd(fs, match);

		for (int i = 0; err != FT_Err_Ok && i < fs->nfont; i++) {
			/* Try the new filename */
			if (FcPatternGetString(fs->fonts[i], FC_FILE, 0, &file) == FcResultMatch &&
				FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &family) == FcResultMatch &&
				FcPatternGetString(fs->fonts[i], FC_STYLE, 0, &style) == FcResultMatch &&
				FcPatternGetInteger(fs->fonts[i], FC_INDEX, 0, &index) == FcResultMatch) {

				/* The correct style? */
				if (!font_style.empty() && !StrEqualsIgnoreCase(font_style, (char *)style)) continue;

				/* Font config takes the best shot, which, if the family name is spelled
				 * wrongly a 'random' font, so check whether the family name is the
				 * same as the supplied name */
				if (StrEqualsIgnoreCase(font_family, (char *)family)) {
					err = FT_New_Face(_library, (char *)file, index, face);
				}
			}
		}
	}

	FcPatternDestroy(pat);
	FcFontSetDestroy(fs);
	FcConfigDestroy(fc_instance);

	return err;
}

bool SetFallbackFont(FontCacheSettings *settings, const std::string &language_isocode, int, MissingGlyphSearcher *callback)
{
	bool ret = false;

	if (!FcInit()) return ret;

	auto fc_instance = FcConfigReference(nullptr);
	assert(fc_instance != nullptr);

	std::string lang = GetFontConfigLanguage(language_isocode);

	/* First create a pattern to match the wanted language. */
	FcPattern *pat = FcNameParse((const FcChar8 *)lang.c_str());
	/* We only want to know these attributes. */
	FcObjectSet *os = FcObjectSetBuild(FC_FILE, FC_INDEX, FC_SPACING, FC_SLANT, FC_WEIGHT, nullptr);
	/* Get the list of filenames matching the wanted language. */
	FcFontSet *fs = FcFontList(nullptr, pat, os);

	/* We don't need these anymore. */
	FcObjectSetDestroy(os);
	FcPatternDestroy(pat);

	if (fs != nullptr) {
		int best_weight = -1;
		const char *best_font = nullptr;
		int best_index = 0;

		for (int i = 0; i < fs->nfont; i++) {
			FcPattern *font = fs->fonts[i];

			FcChar8 *file = nullptr;
			FcResult res = FcPatternGetString(font, FC_FILE, 0, &file);
			if (res != FcResultMatch || file == nullptr) {
				continue;
			}

			/* Get a font with the right spacing .*/
			int value = 0;
			FcPatternGetInteger(font, FC_SPACING, 0, &value);
			if (callback->Monospace() != (value == FC_MONO) && value != FC_DUAL) continue;

			/* Do not use those that explicitly say they're slanted. */
			FcPatternGetInteger(font, FC_SLANT, 0, &value);
			if (value != 0) continue;

			/* We want the fatter font as they look better at small sizes. */
			FcPatternGetInteger(font, FC_WEIGHT, 0, &value);
			if (value <= best_weight) continue;

			/* Possible match based on attributes, get index. */
			int32_t index;
			res = FcPatternGetInteger(font, FC_INDEX, 0, &index);
			if (res != FcResultMatch) continue;

			callback->SetFontNames(settings, (const char *)file, &index);

			bool missing = callback->FindMissingGlyphs();
			Debug(fontcache, 1, "Font \"{}\" misses{} glyphs", (char *)file, missing ? "" : " no");

			if (!missing) {
				best_weight = value;
				best_font = (const char *)file;
				best_index = index;
			}
		}

		if (best_font != nullptr) {
			ret = true;
			callback->SetFontNames(settings, best_font, &best_index);
			InitFontCache(callback->Monospace());
		}

		/* Clean up the list of filenames. */
		FcFontSetDestroy(fs);
	}

	FcConfigDestroy(fc_instance);
	return ret;
}

/**
 * FontConfig implementation of FontSearcher.
 */
class FontConfigFontSearcher : public FontSearcher {
public:
	void UpdateCachedFonts(const std::string &language_isocode, int winlangid) override;
};

void FontConfigFontSearcher::UpdateCachedFonts(const std::string &language_isocode, int)
{
	this->cached_fonts.clear();

	if (!FcInit()) return;

	FcConfig *fc_instance = FcConfigReference(nullptr);
	assert(fc_instance != nullptr);

	std::string lang = GetFontConfigLanguage(language_isocode);

	/* First create a pattern to match the wanted language. */
	FcPattern *pat = FcNameParse(reinterpret_cast<const FcChar8 *>(lang.c_str()));
	/* We want to know this attributes. */
	FcObjectSet *os = FcObjectSetCreate();
	FcObjectSetAdd(os, FC_FAMILY);
	FcObjectSetAdd(os, FC_STYLE);
	FcObjectSetAdd(os, FC_SLANT);
	FcObjectSetAdd(os, FC_WEIGHT);
	/* Get the list of filenames matching the wanted language. */
	FcFontSet *fs = FcFontList(nullptr, pat, os);

	/* We don't need these anymore. */
	FcObjectSetDestroy(os);
	FcPatternDestroy(pat);

	if (fs != nullptr) {
		this->cached_fonts.reserve(fs->nfont);
		for (const FcPattern *font : std::span(fs->fonts, fs->nfont)) {
			FcChar8 *family;
			FcChar8 *style;
			int32_t slant;
			int32_t weight;

			if (FcPatternGetString(font, FC_FAMILY, 0, &family) != FcResultMatch) continue;
			if (FcPatternGetString(font, FC_STYLE, 0, &style) != FcResultMatch) continue;
			if (FcPatternGetInteger(font, FC_SLANT, 0, &slant) != FcResultMatch) continue;
			if (FcPatternGetInteger(font, FC_WEIGHT, 0, &weight) != FcResultMatch) continue;

			/* Don't add duplicate fonts. */
			std::string_view sv_family = reinterpret_cast<const char *>(family);
			std::string_view sv_style = reinterpret_cast<const char *>(style);
			if (std::any_of(std::begin(this->cached_fonts), std::end(this->cached_fonts), [&sv_family, &sv_style](const FontFamily &ff) { return ff.family == sv_family && ff.style == sv_style; })) continue;

			this->cached_fonts.emplace_back(sv_family, sv_style, slant, weight);
		}

		/* Clean up the list of filenames. */
		FcFontSetDestroy(fs);
	}

	FcConfigDestroy(fc_instance);

	std::sort(std::begin(this->cached_fonts), std::end(this->cached_fonts), FontFamilySorter);
}

static FontConfigFontSearcher _fcfs_instance;
