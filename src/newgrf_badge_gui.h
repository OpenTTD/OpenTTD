/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge.h Functions related to NewGRF badges. */

#ifndef NEWGRF_BADGE_GUI_H
#define NEWGRF_BADGE_GUI_H

#include "dropdown_type.h"
#include "newgrf.h"
#include "newgrf_badge_type.h"
#include "timer/timer_game_calendar.h"

class GUIBadgeClasses {
public:
	struct Element {
		BadgeClassID class_index; ///< Badge class index.
		uint8_t column_group; ///< Column group in UI. 0 = left, 1 = centre, 2 = right.
		bool visible; ///< Whether this element is visible.
		uint sort_order; ///< Order of element.
		Dimension size; ///< Maximal size of this element.
		std::string_view label; ///< Class label (string owned by the class badge)
	};

	GUIBadgeClasses() = default;
	explicit GUIBadgeClasses(GrfSpecFeature feature);

	inline std::span<const Element> GetClasses() const
	{
		return this->gui_classes;
	}

	inline std::span<const uint> GetColumnWidths() const
	{
		return this->column_widths;
	}

	uint GetTotalColumnsWidth() const;

private:
	std::vector<Element> gui_classes{};
	std::vector<uint> column_widths{};
};

int DrawBadgeNameList(Rect r, std::span<const BadgeID> badges, GrfSpecFeature feature);
void DrawBadgeColumn(Rect r, int column_group, const GUIBadgeClasses &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, PaletteID remap);

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, std::string &&str, int value, bool masked = false, bool shaded = false);
std::unique_ptr<DropDownListItem> MakeDropDownListBadgeIconItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, const Dimension &dim, SpriteID sprite, PaletteID palette, std::string &&str, int value, bool masked = false, bool shaded = false);

DropDownList BuildBadgeClassConfigurationList(const class GUIBadgeClasses &badge_class, uint columns, std::span<const StringID> column_separators);
bool HandleBadgeConfigurationDropDownClick(GrfSpecFeature feature, uint columns, int result, int click_result);

#endif /* NEWGRF_BADGE_GUI_H */
