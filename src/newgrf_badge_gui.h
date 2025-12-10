/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_badge_gui.h GUI functions related to NewGRF badges. */

#ifndef NEWGRF_BADGE_GUI_H
#define NEWGRF_BADGE_GUI_H

#include "dropdown_type.h"
#include "newgrf.h"
#include "newgrf_badge.h"
#include "newgrf_badge_type.h"
#include "timer/timer_game_calendar.h"

class GUIBadgeClasses : public UsedBadgeClasses {
public:
	struct Element {
		BadgeClassID class_index; ///< Badge class index.
		uint8_t column_group; ///< Column group in UI. 0 = left, 1 = centre, 2 = right.
		bool visible; ///< Whether the icon is visible.
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
std::unique_ptr<DropDownListItem> MakeDropDownListBadgeItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, Money cost, std::string &&str, int value, bool masked = false, bool shaded = false);
std::unique_ptr<DropDownListItem> MakeDropDownListBadgeIconItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, Money cost, const Dimension &dim, SpriteID sprite, PaletteID palette, std::string &&str, int value, bool masked = false, bool shaded = false);

DropDownList BuildBadgeClassConfigurationList(const class GUIBadgeClasses &badge_class, uint columns, std::span<const StringID> column_separators, Colours bg_colour);
bool HandleBadgeConfigurationDropDownClick(GrfSpecFeature feature, uint columns, int result, int click_result, BadgeFilterChoices &choices);

std::pair<WidgetID, WidgetID> AddBadgeDropdownFilters(Window *window, WidgetID container_id, WidgetID widget, Colours colour, GrfSpecFeature feature);

class NWidgetBadgeFilter : public NWidgetLeaf {
public:
	NWidgetBadgeFilter(Colours colour, WidgetID index, GrfSpecFeature feature, BadgeClassID badge_class);

	BadgeClassID GetBadgeClassID() const { return this->badge_class; }
	std::string GetStringParameter(const BadgeFilterChoices &choices) const;
	DropDownList GetDropDownList(PaletteID palette = PAL_NONE) const;

private:
	GrfSpecFeature feature; ///< Feature of this dropdown.
	BadgeClassID badge_class; ///< Badge class of this dropdown.
};

void ResetBadgeFilter(BadgeFilterChoices &choices, BadgeClassID badge_class_index);
void SetBadgeFilter(BadgeFilterChoices &choices, BadgeID badge_index);

#endif /* NEWGRF_BADGE_GUI_H */
