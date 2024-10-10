/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge_type.h Types related to NewGRF badges. */

#ifndef NEWGRF_BADGE_TYPE_H
#define NEWGRF_BADGE_TYPE_H

#include "core/enum_type.hpp"
#include "core/strong_typedef_type.hpp"

using BadgeID = StrongType::Typedef<uint16_t, struct BadgeIDTag, StrongType::Compare>;
using BadgeClassID = StrongType::Typedef<uint16_t, struct BadgeClassIDTag, StrongType::Compare>;

enum class BadgeFlag : uint8_t {
	Copy              = 0, ///< Copy badge to related things.
	NameListStop      = 1, ///< Stop adding names to the name list after this badge.
	NameListFirstOnly = 2, ///< Don't add this name to the name list if not first.
	UseCompanyColour  = 3, ///< Apply company colour palette to this badge.
};
using BadgeFlags = EnumBitSet<BadgeFlag, uint8_t>;

#endif /* NEWGRF_BADGE_TYPE_H */
