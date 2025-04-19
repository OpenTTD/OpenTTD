/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge.cpp Functionality for NewGRF badges. */

#include "stdafx.h"

#include "dropdown_type.h"
#include "dropdown_func.h"
#include "newgrf.h"
#include "newgrf_badge.h"
#include "newgrf_badge_config.h"
#include "newgrf_badge_gui.h"
#include "newgrf_badge_type.h"
#include "settings_gui.h"
#include "strings_func.h"
#include "timer/timer_game_calendar.h"
#include "window_gui.h"
#include "zoom_func.h"

#include "table/strings.h"

#include "dropdown_common_type.h"

#include "safeguards.h"

static constexpr uint MAX_BADGE_HEIGHT = 12; ///< Maximal height of a badge sprite.
static constexpr uint MAX_BADGE_WIDTH = MAX_BADGE_HEIGHT * 2; ///< Maximal width.

/**
 * Get the largest badge size (within limits) for a badge class.
 * @param class_index Badge class.
 * @param feature Feature being used.
 * @returns Largest base size of the badge class for the feature.
 */
static Dimension GetBadgeMaximalDimension(BadgeClassID class_index, GrfSpecFeature feature)
{
	Dimension d = {0, MAX_BADGE_HEIGHT};

	for (const auto &badge : GetBadges()) {
		if (badge.class_index != class_index) continue;

		PalSpriteID ps = GetBadgeSprite(badge, feature, std::nullopt, PAL_NONE);
		if (ps.sprite == 0) continue;

		d.width = std::max(d.width, GetSpriteSize(ps.sprite, nullptr, ZOOM_LVL_NORMAL).width);
		if (d.width > MAX_BADGE_WIDTH) break;
	}

	d.width = std::min(d.width, MAX_BADGE_WIDTH);
	return d;
}

static bool operator<(const GUIBadgeClasses::Element &a, const GUIBadgeClasses::Element &b)
{
	if (a.column_group != b.column_group) return a.column_group < b.column_group;
	if (a.sort_order != b.sort_order) return a.sort_order < b.sort_order;
	return a.label < b.label;
}

/**
 * Construct of list of badge classes and column groups to display.
 * @param feature feature being used.
 */
GUIBadgeClasses::GUIBadgeClasses(GrfSpecFeature feature)
{
	/* Get list of classes used by feature. */
	UsedBadgeClasses used(feature);

	uint max_column = 0;
	for (BadgeClassID class_index : used.Classes()) {
		const Badge *class_badge = GetClassBadge(class_index);
		if (class_badge->name == STR_NULL) continue;

		Dimension size = GetBadgeMaximalDimension(class_index, feature);
		if (size.width == 0) continue;

		const BadgeClassConfigItem &config = GetBadgeClassConfigItem(feature, class_badge->label);

		this->gui_classes.emplace_back(class_index, config.column, config.show_icon, config.sort_order, size, class_badge->label);
		if (config.show_icon) max_column = std::max<uint>(max_column, config.column);
	}

	std::sort(std::begin(this->gui_classes), std::end(this->gui_classes));

	/* Determine total width of visible badge columns. */
	this->column_widths.resize(max_column + 1);
	for (const auto &el : this->gui_classes) {
		if (!el.visible) continue;
		this->column_widths[el.column_group] += ScaleGUITrad(el.size.width) + WidgetDimensions::scaled.hsep_normal;
	}

	/* Replace trailing `hsep_normal` spacer with wider `hsep_wide` spacer. */
	for (uint &badge_width : this->column_widths) {
		if (badge_width == 0) continue;
		badge_width = badge_width - WidgetDimensions::scaled.hsep_normal + WidgetDimensions::scaled.hsep_wide;
	}
}

/**
 * Get total width of all columns.
 * @returns sum of all column widths.
 */
uint GUIBadgeClasses::GetTotalColumnsWidth() const
{
	return std::accumulate(std::begin(this->column_widths), std::end(this->column_widths), 0U);
}

/**
 * Draw names for a list of badge labels.
 * @param r Rect to draw in.
 * @param badges List of badges.
 * @param feature GRF feature being used.
 * @returns Vertical position after drawing is complete.
 */
int DrawBadgeNameList(Rect r, std::span<const BadgeID> badges, GrfSpecFeature)
{
	if (badges.empty()) return r.top;

	std::set<BadgeClassID> class_indexes;
	for (const BadgeID &index : badges) class_indexes.insert(GetBadge(index)->class_index);

	std::string_view list_separator = GetListSeparator();
	for (const BadgeClassID &class_index : class_indexes) {
		const Badge *class_badge = GetClassBadge(class_index);
		if (class_badge == nullptr || class_badge->name == STR_NULL) continue;

		std::string s;
		for (const BadgeID &index : badges) {
			const Badge *badge = GetBadge(index);
			if (badge == nullptr || badge->name == STR_NULL) continue;
			if (badge->class_index != class_index) continue;

			if (!s.empty()) {
				if (badge->flags.Test(BadgeFlag::NameListFirstOnly)) continue;
				s += list_separator;
			}
			AppendStringInPlace(s, badge->name);
			if (badge->flags.Test(BadgeFlag::NameListStop)) break;
		}

		if (s.empty()) continue;

		r.top = DrawStringMultiLine(r, GetString(STR_BADGE_NAME_LIST, class_badge->name, std::move(s)), TC_BLACK);
	}

	return r.top;
}

/**
 * Draw a badge column group.
 * @param r rect to draw within.
 * @param column_group column to draw.
 * @param gui_classes gui badge classes.
 * @param badges badges to draw.
 * @param feature feature being used.
 * @param introduction_date introduction date of item.
 * @param remap palette remap to for company-coloured badges.
 */
void DrawBadgeColumn(Rect r, int column_group, const GUIBadgeClasses &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, PaletteID remap)
{
	bool rtl = _current_text_dir == TD_RTL;
	for (const auto &gc : gui_classes.GetClasses()) {
		if (gc.column_group != column_group) continue;
		if (!gc.visible) continue;

		int width = ScaleGUITrad(gc.size.width);
		for (const BadgeID &index : badges) {
			const Badge &badge = *GetBadge(index);
			if (badge.class_index != gc.class_index) continue;

			PalSpriteID ps = GetBadgeSprite(badge, feature, introduction_date, remap);
			if (ps.sprite == 0) continue;

			DrawSpriteIgnorePadding(ps.sprite, ps.pal, r.WithWidth(width, rtl), SA_CENTER);
			break;
		}

		r = r.Indent(width + WidgetDimensions::scaled.hsep_normal, rtl);
	}
}

/** Drop down element that draws a list of badges. */
template <class TBase, bool TEnd = true, FontSize TFs = FS_NORMAL>
class DropDownBadges : public TBase {
public:
	template <typename... Args>
	explicit DropDownBadges(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, Args &&...args) :
		TBase(std::forward<Args>(args)...), gui_classes(gui_classes), badges(badges), feature(feature), introduction_date(introduction_date)
	{
		for (const auto &gc : gui_classes->GetClasses()) {
			if (gc.column_group != 0) continue;
			dim.width += gc.size.width + WidgetDimensions::scaled.hsep_normal;
			dim.height = std::max(dim.height, gc.size.height);
		}
	}

	uint Height() const override
	{
		return std::max<uint>(this->dim.height, this->TBase::Height());
	}

	uint Width() const override
	{
		return this->dim.width + WidgetDimensions::scaled.hsep_wide + this->TBase::Width();
	}

	void Draw(const Rect &full, const Rect &r, bool sel, Colours bg_colour) const override
	{
		bool rtl = TEnd ^ (_current_text_dir == TD_RTL);

		DrawBadgeColumn(r.WithWidth(this->dim.width, rtl), 0, *this->gui_classes, this->badges, this->feature, this->introduction_date, PAL_NONE);

		this->TBase::Draw(full, r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_wide, rtl), sel, bg_colour);
	}

private:
	std::shared_ptr<GUIBadgeClasses> gui_classes;

	const std::span<const BadgeID> badges;
	const GrfSpecFeature feature;
	const std::optional<TimerGameCalendar::Date> introduction_date;

	Dimension dim{};
};

using DropDownListBadgeItem = DropDownBadges<DropDownListStringItem>;
using DropDownListBadgeIconItem = DropDownBadges<DropDownListIconItem>;

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, std::string &&str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListBadgeItem>(gui_classes, badges, feature, introduction_date, std::move(str), value, masked, shaded);
}

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeIconItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, const Dimension &dim, SpriteID sprite, PaletteID palette, std::string &&str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListBadgeIconItem>(gui_classes, badges, feature, introduction_date, dim, sprite, palette, std::move(str), value, masked, shaded);
}

/**
 * Drop down component that shows extra buttons to indicate that the item can be moved up or down.
 */
template <class TBase, bool TEnd = true, FontSize TFs = FS_NORMAL>
class DropDownMover : public TBase {
public:
	template <typename... Args>
	explicit DropDownMover(bool up, bool down, Args &&...args) : TBase(std::forward<Args>(args)...), up(up), down(down)
	{
	}

	uint Height() const override
	{
		return std::max<uint>(SETTING_BUTTON_HEIGHT, this->TBase::Height());
	}

	uint Width() const override
	{
		return SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_wide + this->TBase::Width();
	}

	void Draw(const Rect &full, const Rect &r, bool sel, Colours bg_colour) const override
	{
		bool rtl = (_current_text_dir == TD_RTL);
		int w = SETTING_BUTTON_WIDTH;

		int state = 0;
		if (sel) {
			auto [dim, pt] = GetLastDropDownClickPosition();
			int pos = (_current_text_dir == TD_LTR ? dim.width - pt.x : pt.x) - WidgetDimensions::scaled.dropdowntext.left;
			if (IsInsideMM(pos, 0, w / 2)) state = 2;
			if (IsInsideMM(pos, w / 2, w)) state = 1;
		}

		Rect br = r.WithWidth(w, TEnd ^ rtl).CentreTo(w, SETTING_BUTTON_HEIGHT);
		DrawArrowButtons(br.left, br.top, COLOUR_GREY, state, this->up, this->down);

		this->TBase::Draw(full, r.Indent(w + WidgetDimensions::scaled.hsep_wide, TEnd ^ rtl), sel, bg_colour);
	}

private:
	bool up; ///< Can be moved up.
	bool down; ///< Can be moved down.
};

using DropDownListCheckedMoverItem = DropDownMover<DropDownCheck<DropDownString<DropDownListItem>>>;

DropDownList BuildBadgeClassConfigurationList(const GUIBadgeClasses &gui_classes, uint columns, std::span<const StringID> column_separators)
{
	DropDownList list;

	if (gui_classes.GetClasses().empty()) return list;

	list.push_back(MakeDropDownListStringItem(STR_BADGE_CONFIG_RESET, INT_MAX));
	list.push_back(MakeDropDownListDividerItem());

	const BadgeClassID front = gui_classes.GetClasses().front().class_index;
	const BadgeClassID back = gui_classes.GetClasses().back().class_index;

	for (uint i = 0; i < columns; ++i) {
		for (const auto &gc : gui_classes.GetClasses()) {
			if (gc.column_group != i) continue;

			bool first = (i == 0 && gc.class_index == front);
			bool last = (i == columns - 1 && gc.class_index == back);
			list.push_back(std::make_unique<DropDownListCheckedMoverItem>(!first, !last, gc.visible, GetString(GetClassBadge(gc.class_index)->name),
			                                                              gc.class_index.base()));
		}

		if (i >= column_separators.size()) continue;

		if (column_separators[i] == STR_NULL) {
			list.push_back(MakeDropDownListDividerItem());
		} else {
			list.push_back(MakeDropDownListStringItem(column_separators[i], INT_MIN + i, false, true));
		}
	}

	return list;
}

/**
 * Toggle badge class visibility.
 * @param feature Feature being used.
 * @param class_badge Class badge.
 */
static void BadgeClassToggleVisibility(GrfSpecFeature feature, Badge &class_badge)
{
	auto config = GetBadgeClassConfiguration(feature);
	auto it = std::ranges::find(config, class_badge.label, &BadgeClassConfigItem::label);
	if (it == std::end(config)) return;

	it->show_icon = !it->show_icon;
}

/**
 * Move the badge class to the previous position.
 * @param feature Feature being used.
 * @param class_badge Class badge.
 */
static void BadgeClassMovePrevious(GrfSpecFeature feature, Badge &class_badge)
{
	GUIBadgeClasses gui_classes(feature);
	if (gui_classes.GetClasses().empty()) return;

	auto config = GetBadgeClassConfiguration(feature);
	auto it = std::ranges::find(config, class_badge.label, &BadgeClassConfigItem::label);
	if (it == std::end(config)) return;

	auto pos_cur = std::ranges::find(gui_classes.GetClasses(), class_badge.class_index, &GUIBadgeClasses::Element::class_index);
	if (pos_cur == std::begin(gui_classes.GetClasses())) {
		if (it->column > 0) --it->column;
		return;
	}

	auto pos_prev = std::ranges::find(config, std::prev(pos_cur)->label, &BadgeClassConfigItem::label);
	if (it->column > pos_prev->column) {
		--it->column;
	} else {
		/* Rotate elements right so that it is placed before itprev, maintaining order of non-visible elements. */
		std::rotate(pos_prev, it, std::next(it));
	}
}

/**
 * Move the badge class to the next position.
 * @param feature Feature being used.
 * @param class_badge Class badge.
 * @param columns Maximum column number permitted.
 */
static void BadgeClassMoveNext(GrfSpecFeature feature, Badge &class_badge, uint columns)
{
	GUIBadgeClasses gui_classes(feature);
	if (gui_classes.GetClasses().empty()) return;

	auto config = GetBadgeClassConfiguration(feature);
	auto it = std::ranges::find(config, class_badge.label, &BadgeClassConfigItem::label);
	if (it == std::end(config)) return;

	auto pos_cur = std::ranges::find(gui_classes.GetClasses(), class_badge.class_index, &GUIBadgeClasses::Element::class_index);
	if (std::next(pos_cur) == std::end(gui_classes.GetClasses())) {
		if (it->column < static_cast<int>(columns - 1)) ++it->column;
		return;
	}

	auto pos_next = std::ranges::find(config, std::next(pos_cur)->label, &BadgeClassConfigItem::label);
	if (it->column < pos_next->column) {
		++it->column;
	} else {
		/* Rotate elements left so that it is placed after itnext, maintaining order of non-visible elements. */
		std::rotate(it, std::next(it), std::next(pos_next));
	}
}

/**
 * Handle the badge configuration drop down selection.
 * @param feature Feature being used.
 * @param class_index Selected badge class index.
 * @param columns Maximum column number permitted.
 */
void HandleBadgeConfigurationDropDownClick(GrfSpecFeature feature, BadgeClassID class_index, uint columns)
{
	Badge *class_badge = GetClassBadge(class_index);
	if (class_badge == nullptr) return;

	int w = SETTING_BUTTON_WIDTH;
	auto [dim, pt] = GetLastDropDownClickPosition();

	int pos = (_current_text_dir == TD_LTR ? dim.width - pt.x : pt.x) - WidgetDimensions::scaled.dropdowntext.left;
	if (IsInsideMM(pos, 0, w / 2)) {
		/* Move down */
		BadgeClassMoveNext(feature, *class_badge, columns);
	} else if (IsInsideMM(pos, w / 2, w)) {
		/* Move up */
		BadgeClassMovePrevious(feature, *class_badge);
	} else {
		/* Toggle */
		BadgeClassToggleVisibility(feature, *class_badge);
	}
}
