/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_badge.cpp Functionality for NewGRF badges. */

#include "stdafx.h"

#include "core/flatset_type.hpp"
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
#include "window_type.h"
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

		d.width = std::max(d.width, GetSpriteSize(ps.sprite, nullptr, ZoomLevel::Normal).width);
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
GUIBadgeClasses::GUIBadgeClasses(GrfSpecFeature feature) : UsedBadgeClasses(feature)
{
	/* Get list of classes used by feature. */
	uint max_column = 0;
	for (BadgeClassID class_index : this->Classes()) {
		const Badge *class_badge = GetClassBadge(class_index);
		if (class_badge->name == STR_NULL) continue;

		Dimension size = GetBadgeMaximalDimension(class_index, feature);
		if (size.width == 0) continue;

		const auto [config, sort_order] = GetBadgeClassConfigItem(feature, class_badge->label);

		this->gui_classes.emplace_back(class_index, config.column, config.show_icon, sort_order, size, class_badge->label);
		if (size.width != 0 && config.show_icon) max_column = std::max(max_column, config.column);
	}

	std::sort(std::begin(this->gui_classes), std::end(this->gui_classes));

	/* Determine total width of visible badge columns. */
	this->column_widths.resize(max_column + 1);
	for (const auto &el : this->gui_classes) {
		if (!el.visible || el.size.width == 0) continue;
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

	FlatSet<BadgeClassID> class_indexes;
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
			if (badge->flags.Test(BadgeFlag::NameListSkip)) continue;

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
			dim.width += ScaleGUITrad(gc.size.width) + WidgetDimensions::scaled.hsep_normal;
			dim.height = std::max<uint>(dim.height, ScaleGUITrad(gc.size.height));
		}

		/* Remove trailing `hsep_normal` spacer. */
		if (dim.width > 0) dim.width -= WidgetDimensions::scaled.hsep_normal;
	}

	uint Height() const override
	{
		return std::max<uint>(this->dim.height, this->TBase::Height());
	}

	uint Width() const override
	{
		if (this->dim.width == 0) return this->TBase::Width();
		return this->dim.width + WidgetDimensions::scaled.hsep_normal + this->TBase::Width();
	}

	int OnClick(const Rect &r, const Point &pt) const override
	{
		if (this->dim.width == 0) {
			return this->TBase::OnClick(r, pt);
		} else {
			bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
			return this->TBase::OnClick(r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_normal, rtl), pt);
		}
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		if (this->dim.width == 0) {
			this->TBase::Draw(full, r, sel, click_result, bg_colour);
		} else {
			bool rtl = TEnd ^ (_current_text_dir == TD_RTL);
			DrawBadgeColumn(r.WithWidth(this->dim.width, rtl), 0, *this->gui_classes, this->badges, this->feature, this->introduction_date, PAL_NONE);
			this->TBase::Draw(full, r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_normal, rtl), sel, click_result, bg_colour);
		}
	}

private:
	std::shared_ptr<GUIBadgeClasses> gui_classes;

	const std::span<const BadgeID> badges;
	const GrfSpecFeature feature;
	const std::optional<TimerGameCalendar::Date> introduction_date;

	Dimension dim{};
};

using DropDownListBadgeItem = DropDownBadges<DropDownString<DropDownSpacer<DropDownListStringItem, true>, FS_SMALL, true>>;
using DropDownListBadgeIconItem = DropDownBadges<DropDownString<DropDownSpacer<DropDownListIconItem, true>, FS_SMALL, true>>;

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, std::string &&str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListBadgeItem>(gui_classes, badges, feature, introduction_date, "", std::move(str), value, masked, shaded);
}

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, Money cost, std::string &&str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListBadgeItem>(gui_classes, badges, feature, introduction_date, GetString(STR_JUST_CURRENCY_SHORT, cost), std::move(str), value, masked, shaded);
}

std::unique_ptr<DropDownListItem> MakeDropDownListBadgeIconItem(const std::shared_ptr<GUIBadgeClasses> &gui_classes, std::span<const BadgeID> badges, GrfSpecFeature feature, std::optional<TimerGameCalendar::Date> introduction_date, Money cost, const Dimension &dim, SpriteID sprite, PaletteID palette, std::string &&str, int value, bool masked, bool shaded)
{
	return std::make_unique<DropDownListBadgeIconItem>(gui_classes, badges, feature, introduction_date, GetString(STR_JUST_CURRENCY_SHORT, cost), dim, sprite, palette, std::move(str), value, masked, shaded);
}

/**
 * Drop down component that shows extra buttons to indicate that the item can be moved up or down.
 */
template <class TBase, bool TEnd = true, FontSize TFs = FS_NORMAL>
class DropDownMover : public TBase {
public:
	template <typename... Args>
	explicit DropDownMover(int click_up, int click_down, Colours button_colour, Args &&...args)
		: TBase(std::forward<Args>(args)...), click_up(click_up), click_down(click_down), button_colour(button_colour)
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

	int OnClick(const Rect &r, const Point &pt) const override
	{
		bool rtl = (_current_text_dir == TD_RTL);
		int w = SETTING_BUTTON_WIDTH;

		Rect br = r.WithWidth(w, TEnd ^ rtl).CentreToHeight(SETTING_BUTTON_HEIGHT);
		if (br.WithWidth(w / 2, rtl).Contains(pt)) return this->click_up;
		if (br.WithWidth(w / 2, !rtl).Contains(pt)) return this->click_down;

		return this->TBase::OnClick(r.Indent(w + WidgetDimensions::scaled.hsep_wide, TEnd ^ rtl), pt);
	}

	void Draw(const Rect &full, const Rect &r, bool sel, int click_result, Colours bg_colour) const override
	{
		bool rtl = (_current_text_dir == TD_RTL);
		int w = SETTING_BUTTON_WIDTH;

		int state = 0;
		if (sel && click_result != 0) {
			if (click_result == this->click_up) state = 1;
			if (click_result == this->click_down) state = 2;
		}

		Rect br = r.WithWidth(w, TEnd ^ rtl).CentreToHeight(SETTING_BUTTON_HEIGHT);
		DrawUpDownButtons(br.left, br.top, this->button_colour, state, this->click_up != 0, this->click_down != 0);

		this->TBase::Draw(full, r.Indent(w + WidgetDimensions::scaled.hsep_wide, TEnd ^ rtl), sel, click_result, bg_colour);
	}

private:
	int click_up; ///< Click result for up button. Button is inactive if 0.
	int click_down; ///< Click result for down button. Button is inactive if 0.
	Colours button_colour; ///< Colour of buttons.
};

using DropDownListToggleMoverItem = DropDownMover<DropDownToggle<DropDownString<DropDownListItem>>>;
using DropDownListToggleItem = DropDownToggle<DropDownString<DropDownListItem>>;

enum BadgeClick : int {
	BADGE_CLICK_NONE,
	BADGE_CLICK_MOVE_UP,
	BADGE_CLICK_MOVE_DOWN,
	BADGE_CLICK_TOGGLE_ICON,
	BADGE_CLICK_TOGGLE_FILTER,
};

DropDownList BuildBadgeClassConfigurationList(const GUIBadgeClasses &gui_classes, uint columns, std::span<const StringID> column_separators, Colours bg_colour)
{
	DropDownList list;

	list.push_back(MakeDropDownListStringItem(STR_BADGE_CONFIG_RESET, INT_MAX));
	if (gui_classes.GetClasses().empty()) return list;
	list.push_back(MakeDropDownListDividerItem());
	list.push_back(std::make_unique<DropDownUnselectable<DropDownListStringItem>>(GetString(STR_BADGE_CONFIG_ICONS), -1));

	const BadgeClassID front = gui_classes.GetClasses().front().class_index;
	const BadgeClassID back = gui_classes.GetClasses().back().class_index;

	for (uint i = 0; i < columns; ++i) {
		for (const auto &gc : gui_classes.GetClasses()) {
			if (gc.column_group != i) continue;
			if (gc.size.width == 0) continue;

			bool first = (i == 0 && gc.class_index == front);
			bool last = (i == columns - 1 && gc.class_index == back);
			list.push_back(std::make_unique<DropDownListToggleMoverItem>(first ? 0 : BADGE_CLICK_MOVE_UP, last ? 0 : BADGE_CLICK_MOVE_DOWN, COLOUR_YELLOW, gc.visible, BADGE_CLICK_TOGGLE_ICON, COLOUR_YELLOW, bg_colour, GetString(GetClassBadge(gc.class_index)->name), gc.class_index.base()));
		}

		if (i >= column_separators.size()) continue;

		if (column_separators[i] == STR_NULL) {
			list.push_back(MakeDropDownListDividerItem());
		} else {
			list.push_back(MakeDropDownListStringItem(column_separators[i], INT_MIN + i, false, true));
		}
	}

	list.push_back(MakeDropDownListDividerItem());
	list.push_back(std::make_unique<DropDownUnselectable<DropDownListStringItem>>(GetString(STR_BADGE_CONFIG_FILTERS), -1));

	for (const BadgeClassID &badge_class_index : gui_classes.Classes()) {
		const Badge *badge = GetClassBadge(badge_class_index);
		if (!badge->flags.Test(BadgeFlag::HasText)) continue;

		const auto [config, _] = GetBadgeClassConfigItem(gui_classes.GetFeature(), badge->label);
		list.push_back(std::make_unique<DropDownListToggleItem>(config.show_filter, BADGE_CLICK_TOGGLE_FILTER, COLOUR_YELLOW, bg_colour, GetString(badge->name), (1U << 16) | badge_class_index.base()));
	}

	return list;
}

/**
 * Toggle badge class visibility.
 * @param feature Feature being used.
 * @param class_badge Class badge.
 * @param click Dropdown click result.
 */
static void BadgeClassToggleVisibility(GrfSpecFeature feature, Badge &class_badge, int click_result, BadgeFilterChoices &choices)
{
	auto config = GetBadgeClassConfiguration(feature);
	auto it = std::ranges::find(config, class_badge.label, &BadgeClassConfigItem::label);
	if (it == std::end(config)) return;

	if (click_result == BADGE_CLICK_TOGGLE_ICON) it->show_icon = !it->show_icon;
	if (click_result == BADGE_CLICK_TOGGLE_FILTER) {
		it->show_filter = !it->show_filter;
		if (!it->show_filter) ResetBadgeFilter(choices, class_badge.class_index);
	}
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
		/* Rotate elements right so that it is placed before pos_prev, maintaining order of non-visible elements. */
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
		if (it->column < columns - 1) ++it->column;
		return;
	}

	auto pos_next = std::ranges::find(config, std::next(pos_cur)->label, &BadgeClassConfigItem::label);
	if (it->column < pos_next->column) {
		++it->column;
	} else {
		/* Rotate elements left so that it is placed after pos_next, maintaining order of non-visible elements. */
		std::rotate(it, std::next(it), std::next(pos_next));
	}
}

/**
 * Handle the badge configuration drop down selection.
 * @param feature Feature being used.
 * @param columns Maximum column number permitted.
 * @param result Selected dropdown item value.
 * @param click_result Dropdown click result.
 * @return true iff the caller should reinitialise their widgets.
 */
bool HandleBadgeConfigurationDropDownClick(GrfSpecFeature feature, uint columns, int result, int click_result, BadgeFilterChoices &choices)
{
	if (result == INT_MAX) {
		ResetBadgeClassConfiguration(feature);
		return true;
	}

	Badge *class_badge = GetClassBadge(static_cast<BadgeClassID>(result));
	if (class_badge == nullptr) return false;

	switch (click_result) {
		case BADGE_CLICK_MOVE_DOWN: // Move down button.
			BadgeClassMoveNext(feature, *class_badge, columns);
			break;
		case BADGE_CLICK_MOVE_UP: // Move up button.
			BadgeClassMovePrevious(feature, *class_badge);
			break;
		case BADGE_CLICK_TOGGLE_ICON:
		case BADGE_CLICK_TOGGLE_FILTER:
			BadgeClassToggleVisibility(feature, *class_badge, click_result, choices);
			break;
		default:
			break;
	}

	return true;
}

NWidgetBadgeFilter::NWidgetBadgeFilter(Colours colour, WidgetID index, GrfSpecFeature feature, BadgeClassID badge_class)
	: NWidgetLeaf(WWT_DROPDOWN, colour, index, WidgetData{ .string = STR_JUST_STRING }, STR_NULL)
	, feature(feature), badge_class(badge_class)
{
	this->SetFill(1, 0);
	this->SetResize(1, 0);
}

std::string NWidgetBadgeFilter::GetStringParameter(const BadgeFilterChoices &choices) const
{
	auto it = choices.find(this->badge_class);
	if (it == std::end(choices)) {
		return ::GetString(STR_BADGE_FILTER_ANY_LABEL, GetClassBadge(this->badge_class)->name);
	}

	return ::GetString(STR_BADGE_FILTER_IS_LABEL, GetClassBadge(it->first)->name, GetBadge(it->second)->name);
}

/**
 * Get the drop down list of badges for this filter.
 * @param palette Palette used to remap badge sprites.
 * @return Drop down list for filter.
 */
DropDownList NWidgetBadgeFilter::GetDropDownList(PaletteID palette) const
{
	DropDownList list;

	/* Add item for disabling filtering. */
	list.push_back(MakeDropDownListStringItem(::GetString(STR_BADGE_FILTER_ANY_LABEL, GetClassBadge(this->badge_class)->name), -1));
	list.push_back(MakeDropDownListDividerItem());

	/* Add badges */
	Dimension d = GetBadgeMaximalDimension(this->badge_class, this->feature);
	d.width = ScaleGUITrad(d.width);
	d.height = ScaleGUITrad(d.height);

	auto start = list.size();

	const auto *bc = GetClassBadge(this->badge_class);

	for (const Badge &badge : GetBadges()) {
		if (badge.class_index != this->badge_class) continue;
		if (badge.index == bc->index) continue;
		if (badge.name == STR_NULL) continue;
		if (!badge.features.Test(this->feature)) continue;

		PalSpriteID ps = GetBadgeSprite(badge, this->feature, std::nullopt, palette);
		if (ps.sprite == 0) {
			list.push_back(MakeDropDownListStringItem(badge.name, badge.index.base()));
		} else {
			list.push_back(MakeDropDownListIconItem(d, ps.sprite, ps.pal, badge.name, badge.index.base()));
		}
	}

	std::sort(std::begin(list) + start, std::end(list), DropDownListStringItem::NatSortFunc);

	return list;
}

/**
 * Add badge drop down filter widgets.
 * @param window Window that holds the container.
 * @param container Container widget index to hold filter widgets.
 * @param widget Widget index to apply to first filter.
 * @param colour Background colour of widgets.
 * @param feature GRF feature for filters.
 * @return First and last widget indexes of filter widgets.
 */
std::pair<WidgetID, WidgetID> AddBadgeDropdownFilters(Window *window, WidgetID container_id, WidgetID widget, Colours colour, GrfSpecFeature feature)
{
	auto container = window->GetWidget<NWidgetContainer>(container_id);
	container->Clear(window);
	WidgetID first = ++widget;

	/* Get list of classes used by feature. */
	UsedBadgeClasses used(feature);

	for (BadgeClassID class_index : used.Classes()) {
		const auto [config, _] = GetBadgeClassConfigItem(feature, GetClassBadge(class_index)->label);
		if (!config.show_filter) continue;

		container->Add(std::make_unique<NWidgetBadgeFilter>(colour, widget, feature, class_index));
		++widget;
	}

	return {first, widget};
}

/**
 * Reset badge filter choice for a class.
 * @param choices Badge filter choices.
 * @param badge_class_index Badge class to reset.
 */
void ResetBadgeFilter(BadgeFilterChoices &choices, BadgeClassID badge_class_index)
{
	choices.erase(badge_class_index);
}

/**
 * Set badge filter choice for a class.
 * @param choices Badge filter choices.
 * @param badge_index Badge to set. The badge class is inferred from the badge.
 * @note if the badge_index is invalid, the filter will be reset instead.
 */
void SetBadgeFilter(BadgeFilterChoices &choices, BadgeID badge_index)
{
	const Badge *badge = GetBadge(badge_index);
	assert(badge != nullptr);

	choices[badge->class_index] = badge_index;
}
