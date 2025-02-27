/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge.h Functions related to NewGRF badges. */

#ifndef NEWGRF_BADGE_H
#define NEWGRF_BADGE_H

#include "dropdown_type.h"
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
	VariableGRFFileProps grf_prop; ///< Sprite information.

	Badge(std::string_view label, BadgeID index, BadgeClassID class_index) : label(label), index(index), class_index(class_index) {}
};

void ResetBadges();

Badge &GetOrCreateBadge(std::string_view label);
void MarkBadgeSeen(BadgeID index, GrfSpecFeature feature);
void AppendCopyableBadgeList(std::vector<BadgeID> &dst, std::span<const BadgeID> src, GrfSpecFeature feature);
void ApplyBadgeFeaturesToClassBadges();

Badge *GetBadge(BadgeID index);
Badge *GetBadgeByLabel(std::string_view label);
Badge *GetClassBadge(BadgeClassID class_index);

class GUIBadgeClasses {
public:
	struct Element {
		BadgeClassID badge_class; ///< Badge class index.
		uint8_t column_group; ///< Column group in UI. 0 = left, 1 = centre, 2 = right.
		bool visible; ///< Whether this element is visible.
		uint sort_order; ///< Order of element.
		Dimension size; ///< Maximal size of this element.
		std::string_view label; ///< Class label (string owned by the class badge)

		constexpr Element(BadgeClassID badge_class, uint8_t column_group, bool visible, uint sort_order, Dimension size, std::string_view label) :
			badge_class(badge_class), column_group(column_group), visible(visible), sort_order(sort_order), size(size), label(label) {}
	};

	GUIBadgeClasses() = default;
	explicit GUIBadgeClasses(GrfSpecFeature feature);

	inline std::span<const Element> GetClasses() const { return this->gui_classes; }

	inline std::span<const uint> GetColumnWidths() const { return this->column_widths; }

	uint GetTotalColumnsWidth() const;

private:
	std::vector<Element> gui_classes{};
	std::vector<uint> column_widths{};
};

int DrawBadgeNameList(Rect r, std::span<const BadgeID> badges, GrfSpecFeature feature);
void DrawBadgeColumn(Rect r, int column_group, const GUIBadgeClasses &badge_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, PaletteID remap);

uint32_t GetBadgeVariableResult(const struct GRFFile &grffile, std::span<const BadgeID> badges, uint32_t parameter);

class BadgeTextFilter {
public:
	BadgeTextFilter(struct StringFilter &filter, GrfSpecFeature feature);
	bool Filter(std::span<const BadgeID> badges) const;

private:
	std::vector<BadgeID> badges{};
};

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, std::string &&str, int value, bool masked = false, bool shaded = false);
std::unique_ptr<DropDownListItem> MakeDropDownListBadgeIconItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, const Dimension &dim, SpriteID sprite, PaletteID palette, std::string &&str, int value, bool masked = false, bool shaded = false);

#endif /* NEWGRF_BADGE_H */
