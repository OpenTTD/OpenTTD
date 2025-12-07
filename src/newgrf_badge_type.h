/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_badge_type.h Types related to NewGRF badges. */

#ifndef NEWGRF_BADGE_TYPE_H
#define NEWGRF_BADGE_TYPE_H

#include "core/enum_type.hpp"
#include "core/strong_typedef_type.hpp"

using BadgeID = StrongType::Typedef<uint16_t, struct BadgeIDTag, StrongType::Compare>;
using BadgeClassID = StrongType::Typedef<uint16_t, struct BadgeClassIDTag, StrongType::Compare>;

template <> struct std::hash<BadgeClassID> {
	std::size_t operator()(const BadgeClassID &badge_class_index) const noexcept
	{
		return std::hash<BadgeClassID::BaseType>{}(badge_class_index.base());
	}
};

enum class BadgeFlag : uint8_t {
	Copy = 0, ///< Copy badge to related things.
	NameListStop = 1, ///< Stop adding names to the name list after this badge.
	NameListFirstOnly = 2, ///< Don't add this name to the name list if not first.
	UseCompanyColour = 3, ///< Apply company colour palette to this badge.
	NameListSkip = 4, ///< Don't show name in name list at all.

	HasText, ///< Internal flag set if the badge has text.
};
using BadgeFlags = EnumBitSet<BadgeFlag, uint8_t>;

using BadgeFilterChoices = std::unordered_map<BadgeClassID, BadgeID>;

#endif /* NEWGRF_BADGE_TYPE_H */
