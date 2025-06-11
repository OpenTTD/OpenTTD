/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file fontcache.h Functions to read fonts from files and cache them. */

#ifndef ICONGLYPHS_H
#define ICONGLYPHS_H

#include "cargo_type.h"
#include "company_type.h"
#include "engine_type.h"
#include "fontcache.h"
#include "newgrf_badge_type.h"
#include "table/control_codes.h"

using IconGlyph = std::variant<CargoType, BadgeID, CompanyID, EngineID>;

class IconGlyphs {
	std::vector<IconGlyph> glyphs;
public:
	char32_t GetOrCreate(IconGlyph ig)
	{
		auto it = std::ranges::find(this->glyphs, ig);
		if (it == std::end(this->glyphs)) it = this->glyphs.emplace(it, ig);

		return static_cast<char32_t>(std::distance(std::begin(this->glyphs), it)) + SCC_ICON_START;
	}

	inline const IconGlyph *GetGlyph(GlyphID glyph) const
	{
		if (glyph < std::size(glyphs)) return &this->glyphs[glyph];
		return nullptr;
	}

	inline const IconGlyph *GetGlyph(char32_t c) const
	{
		if (c >= SCC_ICON_START && c <= SCC_ICON_END) return GetGlyph(c - SCC_ICON_START);
		return nullptr;
	}

	inline void Clear()
	{
		this->glyphs.clear();
	}
};

extern IconGlyphs _icon_glyphs;

#endif /* ICONGLYPHS_H */
