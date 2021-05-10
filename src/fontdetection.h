/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fontdetection.h Functions related to detecting/finding the right font. */

#ifndef FONTDETECTION_H
#define FONTDETECTION_H

#include "fontcache.h"

#ifdef WITH_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

/**
 * Load a freetype font face with the given font name.
 * @param font_name The name of the font to load.
 * @param face The face that has been found.
 * @return The error we encountered.
 */
FT_Error GetFontByFaceName(const char *font_name, FT_Face *face);

#endif /* WITH_FREETYPE */

/**
 * We would like to have a fallback font as the current one
 * doesn't contain all characters we need.
 * This function must set all fonts of settings.
 * @param settings the settings to overwrite the fontname of.
 * @param language_isocode the language, e.g. en_GB.
 * @param winlangid the language ID windows style.
 * @param callback The function to call to check for missing glyphs.
 * @return true if a font has been set, false otherwise.
 */
bool SetFallbackFont(struct FreeTypeSettings *settings, const char *language_isocode, int winlangid, class MissingGlyphSearcher *callback);

#endif
