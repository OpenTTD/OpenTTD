/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file font_unix.h Functions related to detecting/finding the right font. */

#ifndef FONT_UNIX_H
#define FONT_UNIX_H

#ifdef WITH_FONTCONFIG

#include "../../fontcache.h"

#include <ft2build.h>
#include FT_FREETYPE_H

FT_Error GetFontByFaceName(std::string_view font_name, FT_Face *face);

bool FontConfigFindFallbackFont(const std::string &language_isocode, FontSizes fontsizes, MissingGlyphSearcher *callback);

#endif /* WITH_FONTCONFIG */

#endif /* FONT_UNIX_H */
