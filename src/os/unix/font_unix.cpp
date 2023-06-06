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


FT_Error GetFontByFaceName(const char *font_name, FT_Face *face)
{
	FT_Error err = FT_Err_Cannot_Open_Resource;

	if (!FcInit()) {
		ShowInfo("Unable to load font configuration");
		return err;
	}

	auto fc_instance = FcConfigReference(nullptr);
	assert(fc_instance != nullptr);

	FcPattern *match;
	FcPattern *pat;
	FcFontSet *fs;
	FcResult  result;
	char *font_style;
	char *font_family;

	/* Split & strip the font's style */
	font_family = stredup(font_name);
	font_style = strchr(font_family, ',');
	if (font_style != nullptr) {
		font_style[0] = '\0';
		font_style++;
		while (*font_style == ' ' || *font_style == '\t') font_style++;
	}

	/* Resolve the name and populate the information structure */
	pat = FcNameParse((FcChar8 *)font_family);
	if (font_style != nullptr) FcPatternAddString(pat, FC_STYLE, (FcChar8 *)font_style);
	FcConfigSubstitute(nullptr, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	fs = FcFontSetCreate();
	match = FcFontMatch(nullptr, pat, &result);

	if (fs != nullptr && match != nullptr) {
		int i;
		FcChar8 *family;
		FcChar8 *style;
		FcChar8 *file;
		int32_t index;
		FcFontSetAdd(fs, match);

		for (i = 0; err != FT_Err_Ok && i < fs->nfont; i++) {
			/* Try the new filename */
			if (FcPatternGetString(fs->fonts[i], FC_FILE, 0, &file) == FcResultMatch &&
				FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &family) == FcResultMatch &&
				FcPatternGetString(fs->fonts[i], FC_STYLE, 0, &style) == FcResultMatch &&
				FcPatternGetInteger(fs->fonts[i], FC_INDEX, 0, &index) == FcResultMatch) {

				/* The correct style? */
				if (font_style != nullptr && !StrEqualsIgnoreCase(font_style, (char *)style)) continue;

				/* Font config takes the best shot, which, if the family name is spelled
					* wrongly a 'random' font, so check whether the family name is the
					* same as the supplied name */
				if (StrEqualsIgnoreCase(font_family, (char *)family)) {
					err = FT_New_Face(_library, (char *)file, index, face);
				}
			}
		}
	}

	free(font_family);
	FcPatternDestroy(pat);
	FcFontSetDestroy(fs);
	FcConfigDestroy(fc_instance);

	return err;
}

bool SetFallbackFont(FontCacheSettings *settings, const std::string &language_isocode, int winlangid, MissingGlyphSearcher *callback)
{
	bool ret = false;

	if (!FcInit()) return ret;

	auto fc_instance = FcConfigReference(nullptr);
	assert(fc_instance != nullptr);

	/* Fontconfig doesn't handle full language isocodes, only the part
	 * before the _ of e.g. en_GB is used, so "remove" everything after
	 * the _. */
	std::string lang = fmt::format(":lang={}", language_isocode.substr(0, language_isocode.find('_')));

	/* First create a pattern to match the wanted language. */
	FcPattern *pat = FcNameParse((const FcChar8 *)lang.c_str());
	/* We only want to know the filename. */
	FcObjectSet *os = FcObjectSetBuild(FC_FILE, FC_SPACING, FC_SLANT, FC_WEIGHT, nullptr);
	/* Get the list of filenames matching the wanted language. */
	FcFontSet *fs = FcFontList(nullptr, pat, os);

	/* We don't need these anymore. */
	FcObjectSetDestroy(os);
	FcPatternDestroy(pat);

	if (fs != nullptr) {
		int best_weight = -1;
		const char *best_font = nullptr;

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

			callback->SetFontNames(settings, (const char *)file);

			bool missing = callback->FindMissingGlyphs();
			Debug(fontcache, 1, "Font \"{}\" misses{} glyphs", (char *)file, missing ? "" : " no");

			if (!missing) {
				best_weight = value;
				best_font = (const char *)file;
			}
		}

		if (best_font != nullptr) {
			ret = true;
			callback->SetFontNames(settings, best_font);
			InitFontCache(callback->Monospace());
		}

		/* Clean up the list of filenames. */
		FcFontSetDestroy(fs);
	}

	FcConfigDestroy(fc_instance);
	return ret;
}
