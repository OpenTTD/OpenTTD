/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_badge.h Functions related to NewGRF badges. */

#ifndef NEWGRF_BADGE_H
#define NEWGRF_BADGE_H

#include "core/flatset_type.hpp"
#include "newgrf.h"
#include "newgrf_badge_type.h"
#include "newgrf_commons.h"
#include "strings_type.h"
#include "timer/timer_game_calendar.h"

class Badge {
public:
	std::string label; ///< Label of badge.
	BadgeID index; ///< Index assigned to badge.
	BadgeClassID class_index; ///< Index of class this badge belongs to.
	BadgeFlags flags = {}; ///< Display flags
	StringID name = 0; ///< Short name.
	GrfSpecFeatures features{}; ///< Bitmask of which features use this badge.
	VariableGRFFileProps<GrfSpecFeature> grf_prop; ///< Sprite information.

	Badge(std::string_view label, BadgeID index, BadgeClassID class_index) : label(label), index(index), class_index(class_index) {}
};

/** Utility class to create a list of badge classes used by a feature. */
class UsedBadgeClasses {
public:
	UsedBadgeClasses() = default;
	explicit UsedBadgeClasses(GrfSpecFeature feature);

	inline GrfSpecFeature GetFeature() const
	{
		return this->feature;
	}

	inline std::span<const BadgeClassID> Classes() const
	{
		return this->classes;
	}

private:
	GrfSpecFeature feature;
	std::vector<BadgeClassID> classes; ///< List of badge classes.
};

void ResetBadges();

Badge &GetOrCreateBadge(std::string_view label);
void MarkBadgeSeen(BadgeID index, GrfSpecFeature feature);
void AppendCopyableBadgeList(std::vector<BadgeID> &dst, std::span<const BadgeID> src, GrfSpecFeature feature);
void ApplyBadgeFeaturesToClassBadges();

std::span<const Badge> GetBadges();
Badge *GetBadge(BadgeID index);
Badge *GetBadgeByLabel(std::string_view label);
Badge *GetClassBadge(BadgeClassID class_index);
std::span<const BadgeID> GetClassBadges();

uint32_t GetBadgeVariableResult(const struct GRFFile &grffile, std::span<const BadgeID> badges, uint32_t parameter);

PalSpriteID GetBadgeSprite(const Badge &badge, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, PaletteID remap);

class BadgeTextFilter {
public:
	BadgeTextFilter(struct StringFilter &filter, GrfSpecFeature feature);
	bool Filter(std::span<const BadgeID> badges) const;

private:
	FlatSet<BadgeID> badges{};
};

class BadgeDropdownFilter {
public:
	BadgeDropdownFilter(const BadgeFilterChoices &conf) : badges(conf) {}
	bool Filter(std::span<const BadgeID> badges) const;

private:
	const BadgeFilterChoices &badges;
};

#endif /* NEWGRF_BADGE_H */
