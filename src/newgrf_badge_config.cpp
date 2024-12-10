 /*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge_config.cpp Functionality for NewGRF badge configuration. */

#include "stdafx.h"
#include "dropdown_type.h"
#include "dropdown_common_type.h"
#include "dropdown_func.h"
#include "ini_type.h"
#include "newgrf.h"
#include "newgrf_badge.h"
#include "newgrf_badge_config.h"
#include "newgrf_badge_type.h"
#include "strings_func.h"
#include "window_gui.h"

#include "table/strings.h"

#include "safeguards.h"

/** Global state for badge class configuration. */
class BadgeClassConfig {
public:
	struct ConfigEntry {
		std::string label;
		int column;
		bool visible;

		ConfigEntry(std::string_view label, int column, bool visible) : label(label), column(column), visible(visible) {}
	};
	std::array<std::vector<ConfigEntry>, GrfSpecFeature::GSF_END> features = {};

	static constexpr GrfSpecFeatureMask CONFIGURABLE_FEATURES =
		(1U << GSF_TRAINS) |
		(1U << GSF_ROADVEHICLES) |
		(1U << GSF_SHIPS) |
		(1U << GSF_AIRCRAFT) |
		(1U << GSF_STATIONS) |
		(1U << GSF_BRIDGES) |
		(1U << GSF_HOUSES) |
		(1U << GSF_INDUSTRYTILES) |
		(1U << GSF_INDUSTRIES) |
		(1U << GSF_AIRPORTS) |
		(1U << GSF_OBJECTS) |
		(1U << GSF_RAILTYPES) |
		(1U << GSF_AIRPORTTILES) |
		(1U << GSF_ROADTYPES) |
		(1U << GSF_TRAMTYPES) |
		(1U << GSF_ROADSTOPS);

	static inline const std::array<std::string_view, GrfSpecFeature::GSF_END> sections = {
		"badge_columns_trains", // GSF_TRAINS
		"badge_columns_roadvehicles", // GSF_ROADVEHICLES
		"badge_columns_ships", // GSF_SHIPS
		"badge_columns_aircraft", // GSF_AIRCRAFT
		"badge_columns_stations", // GSF_STATIONS
		{}, // GSF_CANALS
		"badge_columns_bridges", // GSF_BRIDGES
		"badge_columns_houses", // GSF_HOUSES
		{}, // GSF_GLOBALVAR
		"badge_columns_industrytiles", // GSF_INDUSTRYTILES
		"badge_columns_industries", // GSF_INDUSTRIES
		{}, // GSF_CARGOES
		{}, // GSF_SOUNDFX
		"badge_columns_airports", // GSF_AIRPORTS
		{}, // GSF_SIGNALS
		"badge_columns_objects", // GSF_OBJECTS
		"badge_columns_railtypes", // GSF_RAILTYPES
		"badge_columns_airporttiles", // GSF_AIRPORTTILES
		"badge_columns_roadtypes", // GSF_ROADTYPES
		"badge_columns_tramtypes", // GSF_TRAMTYPES
		"badge_columns_roadstops", // GSF_ROADSTOPS
		{}, // GSF_BADGES
	};
};

/** Static instance of badge class configuration state. */
static BadgeClassConfig _badge_config;

/**
 * Get the badge user configuration for a feature.
 * @returns badge configuration for the given feature.
 */
static std::span<BadgeClassConfig::ConfigEntry> GetBadgeClassConfiguration(GrfSpecFeature feature)
{
	return _badge_config.features[to_underlying(feature)];
}

/**
 * Add new badge class to user configuration.
 * @param label label of class.
 */
void AddBadgeClassToConfiguration(std::string_view label)
{
	for (const GrfSpecFeature feature : SetBitIterator<GrfSpecFeature>(BadgeClassConfig::CONFIGURABLE_FEATURES)) {
		auto &config = _badge_config.features[feature];

		/* Search for this label in the vehicle badge configuration. */
		auto found = std::ranges::find(config, label, &BadgeClassConfig::ConfigEntry::label);
		if (found != std::end(config)) continue;

		/* Not found, insert it. */
		config.emplace_back(label, 0, true);
	}
}

/**
 * Apply configuration for a badge class.
 * @param feature Feature being used.
 * @param label Badge class label.
 * @param[out] visible Whether the badge class should be visible.
 * @param[out] column Which column the badge class should appear.
 * @param[out] sort_order Order within column for the badge class.
 * @returns true iff configuration was found and applied.
 */
bool ApplyBadgeClassConfiguration(GrfSpecFeature feature, std::string_view label, bool &visible, uint8_t &column, uint &sort_order)
{
	if (!HasBit(BadgeClassConfig::CONFIGURABLE_FEATURES, feature)) return false;

	auto config = GetBadgeClassConfiguration(feature);
	auto found = std::ranges::find(config, label, &BadgeClassConfig::ConfigEntry::label);
	if (found == std::end(config)) return false;

	column = found->column;
	visible = found->visible;
	sort_order = std::distance(std::begin(config), found);

	return true;
}

/**
 * Load badge column preferences.
 * @param ini IniFile to load to.
 * @param feature Feature to load.
 */
static void BadgeClassLoadConfigFeature(const IniFile &ini, GrfSpecFeature feature)
{
	assert(HasBit(BadgeClassConfig::CONFIGURABLE_FEATURES, feature));
	assert(!BadgeClassConfig::sections[feature].empty());

	_badge_config.features[to_underlying(feature)].clear();

	const IniGroup *group = ini.GetGroup(BadgeClassConfig::sections[feature]);
	if (group == nullptr) return;

	for (const IniItem &item : group->items) {
		int column = 0;
		bool visible = true;

		if (item.value.has_value() && !item.value.value().empty()) {
			const char *str = item.value.value().c_str();
			if (str[0] == '!') {
				/* Prefix of ! means the label is not visible. */
				visible = false;
				++str;
			}
			char *end;
			long val = std::strtol(str, &end, 0);
			if (end != str && *end == '\0') column = static_cast<int>(val);
		}

		_badge_config.features[to_underlying(feature)].emplace_back(item.name, column, visible);
	}
}

/**
 * Load badge column preferences.
 * @param ini IniFile to load to.
 */
void BadgeClassLoadConfig(const IniFile &ini)
{
	for (const GrfSpecFeature feature : SetBitIterator<GrfSpecFeature>(BadgeClassConfig::CONFIGURABLE_FEATURES)) {
		BadgeClassLoadConfigFeature(ini, feature);
	}
}

/**
 * Save badge column preferences.
 * @param ini IniFile to save to.
 * @param feature Feature to save.
 */
static void BadgeClassSaveConfigFeature(IniFile &ini, GrfSpecFeature feature)
{
	assert(HasBit(BadgeClassConfig::CONFIGURABLE_FEATURES, feature));
	assert(!BadgeClassConfig::sections[feature].empty());

	IniGroup &group = ini.GetOrCreateGroup(BadgeClassConfig::sections[feature]);
	group.Clear();

	for (const auto &item : _badge_config.features[to_underlying(feature)]) {
		group.CreateItem(item.label).SetValue(fmt::format("{}{}", item.visible ? "" : "!", item.column));
	}
}

/**
 * Save badge column preferences.
 * @param ini IniFile to save to.
 */
void BadgeClassSaveConfig(IniFile &ini)
{
	for (const GrfSpecFeature feature : SetBitIterator<GrfSpecFeature>(BadgeClassConfig::CONFIGURABLE_FEATURES)) {
		BadgeClassSaveConfigFeature(ini, feature);
	}
}

/**
 * Drop down component that shows extra 'buttons' to indicate that the item can be moved up or down.
 */
template <class TBase, bool TEnd = true, FontSize TFs = FS_NORMAL>
class DropDownMover : public TBase {
public:
	template <typename... Args>
	explicit DropDownMover(bool up, bool down, Args&&... args) : TBase(std::forward<Args>(args)...), up(up), down(down)
	{
		Dimension d = NWidgetScrollbar::GetVerticalDimension();
		this->dim = {d.width * 2, d.height};
	}

	uint Height() const override { return std::max<uint>(this->dim.height, this->TBase::Height()); }
	uint Width() const override { return this->dim.width + WidgetDimensions::scaled.hsep_wide + this->TBase::Width(); }

	void Draw(const Rect &full, const Rect &r, bool sel, Colours bg_colour) const override
	{
		bool rtl = (_current_text_dir == TD_RTL);

		Dimension d = NWidgetScrollbar::GetVerticalDimension();

		Rect br = r.WithWidth(this->dim.width, TEnd ^ rtl);
		if (this->up) DrawString(br.WithWidth(d.width, rtl), STR_JUST_UP_ARROW, this->GetColour(sel), SA_CENTER, false, TFs);
		if (this->down) DrawString(br.Indent(d.width, rtl).WithWidth(d.width, rtl), STR_JUST_DOWN_ARROW, this->GetColour(sel), SA_CENTER, false, TFs);

		this->TBase::Draw(full, r.Indent(this->dim.width + WidgetDimensions::scaled.hsep_wide, TEnd ^ rtl), sel, bg_colour);
	}

private:
	bool up; ///< Can be moved up.
	bool down; ///< Can be moved down.
	Dimension dim; ///< Dimension of both up/down symbols.
};

using DropDownListCheckedMoverItem = DropDownMover<DropDownCheck<DropDownString<DropDownListItem>>>;

DropDownList BuildBadgeClassConfigurationList(GrfSpecFeature feature, uint columns, std::span<const StringID> column_separators)
{
	DropDownList list;

	GUIBadgeClasses badge_classes(feature);
	if (badge_classes.GetClasses().empty()) return list;

	list.push_back(MakeDropDownListStringItem(STR_BADGE_CONFIG_RESET, INT_MAX));
	list.push_back(MakeDropDownListDividerItem());

	const BadgeClassID front = badge_classes.GetClasses().front().badge_class;
	const BadgeClassID back = badge_classes.GetClasses().back().badge_class;

	for (uint8_t i = 0; i < columns; ++i) {
		for (const GUIBadgeClasses::Element &badge_class : badge_classes.GetClasses()) {
			if (badge_class.column_group != i) continue;

			bool first = (i == 0 && badge_class.badge_class == front);
			bool last = (i == columns - 1 && badge_class.badge_class == back);
			list.push_back(std::make_unique<DropDownListCheckedMoverItem>(!first, !last, badge_class.visible, GetClassBadge(badge_class.badge_class)->name, badge_class.badge_class.base()));
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
 * @param class_index Badge class id.
 */
void BadgeClassToggleVisibility(GrfSpecFeature feature, BadgeClassID class_index)
{
	Badge *badge = GetClassBadge(class_index);
	if (badge == nullptr) return;

	auto config = GetBadgeClassConfiguration(feature);
	auto it = std::ranges::find(config, badge->label, &BadgeClassConfig::ConfigEntry::label);
	if (it == std::end(config)) return;

	it->visible = !it->visible;
}

void BadgeClassMovePrevious(GrfSpecFeature feature, BadgeClassID class_index)
{
	Badge *badge = GetClassBadge(class_index);
	if (badge == nullptr) return;

	auto config = GetBadgeClassConfiguration(feature);
	auto it = std::ranges::find(config, badge->label, &BadgeClassConfig::ConfigEntry::label);
	if (it == std::end(config)) return;

	GUIBadgeClasses badge_classes(feature);
	if (badge_classes.GetClasses().empty()) return;

	auto bccur = std::ranges::find(badge_classes.GetClasses(), class_index, &GUIBadgeClasses::Element::badge_class);
	if (bccur == std::begin(badge_classes.GetClasses())) {
		if (it->column > 0) --it->column;
		return;
	}

	auto itprev = std::ranges::find(config, std::prev(bccur)->label, &BadgeClassConfig::ConfigEntry::label);
	if (it->column > itprev->column) {
		--it->column;
	} else {
		/* Rotate elements right so that it is placed before itprev, maintaining order of non-visible elements. */
		std::rotate(itprev, it, std::next(it));
	}
}

void BadgeClassMoveNext(GrfSpecFeature feature, BadgeClassID class_index, uint columns)
{
	GUIBadgeClasses badge_classes(feature);
	if (badge_classes.GetClasses().empty()) return;

	Badge *badge = GetClassBadge(class_index);
	if (badge == nullptr) return;

	auto config = GetBadgeClassConfiguration(feature);
	auto it = std::ranges::find(config, badge->label, &BadgeClassConfig::ConfigEntry::label);
	if (it == std::end(config)) return;

	auto bccur = std::ranges::find(badge_classes.GetClasses(), class_index, &GUIBadgeClasses::Element::badge_class);
	if (std::next(bccur) == std::end(badge_classes.GetClasses())) {
		if (it->column < static_cast<int>(columns - 1)) ++it->column;
		return;
	}

	auto itnext = std::ranges::find(config, std::next(bccur)->label, &BadgeClassConfig::ConfigEntry::label);
	if (it->column < itnext->column) {
		++it->column;
	} else {
		/* Rotate elements left so that it is placed after itnext, maintaining order of non-visible elements. */
		std::rotate(it, std::next(it), std::next(itnext));
	}
}
