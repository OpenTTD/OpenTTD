/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_badge_config.cpp Functionality for NewGRF badge configuration. */

#include "stdafx.h"
#include "core/string_consumer.hpp"
#include "ini_type.h"
#include "newgrf.h"
#include "newgrf_badge.h"
#include "newgrf_badge_config.h"
#include "newgrf_badge_type.h"

#include "table/strings.h"

#include "safeguards.h"

/** Global state for badge class configuration. */
class BadgeClassConfig {
public:
	static inline const BadgeClassConfigItem EMPTY_CONFIG_ITEM{};

	EnumIndexArray<std::vector<BadgeClassConfigItem>, GrfSpecFeature, GrfSpecFeature::End> features = {};

	static constexpr GrfSpecFeatures CONFIGURABLE_FEATURES = {
		GrfSpecFeature::Trains, GrfSpecFeature::RoadVehicles, GrfSpecFeature::Ships, GrfSpecFeature::Aircraft, GrfSpecFeature::Stations, GrfSpecFeature::Houses, GrfSpecFeature::Objects, GrfSpecFeature::RoadStops,
	};

	static inline const EnumIndexArray<std::string_view, GrfSpecFeature, GrfSpecFeature::End> sections = {
		"badges_trains", // GrfSpecFeature::Trains
		"badges_roadvehicles", // GrfSpecFeature::RoadVehicles
		"badges_ships", // GrfSpecFeature::Ships
		"badges_aircraft", // GrfSpecFeature::Aircraft
		"badges_stations", // GrfSpecFeature::Stations
		{}, // GrfSpecFeature::Canals
		{}, // GrfSpecFeature::Bridges
		"badges_houses", // GrfSpecFeature::Houses
		{}, // GrfSpecFeature::GlobalVar
		{}, // GrfSpecFeature::IndustryTiles
		{}, // GrfSpecFeature::Industries
		{}, // GrfSpecFeature::Cargoes
		{}, // GrfSpecFeature::SoundEffects
		{}, // GrfSpecFeature::Airports
		{}, // GrfSpecFeature::Signals
		"badges_objects", // GrfSpecFeature::Objects
		{}, // GrfSpecFeature::RailTypes
		{}, // GrfSpecFeature::AirportTiles
		{}, // GrfSpecFeature::RoadTypes
		{}, // GrfSpecFeature::TramTypes
		"badges_roadstops", // GrfSpecFeature::RoadStops
		{}, // GrfSpecFeature::Badges
	};
};

/** Static instance of badge class configuration state. */
static BadgeClassConfig _badge_config;

/**
 * Get the badge user configuration for a feature.
 * @param feature The feature to load the configuration for.
 * @returns badge configuration for the given feature.
 */
std::span<BadgeClassConfigItem> GetBadgeClassConfiguration(GrfSpecFeature feature)
{
	assert(BadgeClassConfig::CONFIGURABLE_FEATURES.Test(feature));
	assert(to_underlying(feature) < std::size(_badge_config.features));
	return _badge_config.features[feature];
}

/**
 * Add current badge classes to user configuration.
 */
void AddBadgeClassesToConfiguration()
{
	for (const GrfSpecFeature &feature : BadgeClassConfig::CONFIGURABLE_FEATURES) {
		auto &config = _badge_config.features[feature];

		for (const BadgeID &index : GetClassBadges()) {
			const Badge &badge = *GetBadge(index);
			if (badge.name == STR_NULL) continue;
			if (!badge.features.Test(feature)) continue;

			auto found = std::ranges::find(config, badge.label, &BadgeClassConfigItem::label);
			if (found != std::end(config)) continue;

			/* Not found, insert it. */
			config.emplace_back(badge.label, 0, true, false);
		}
	}
}

/**
 * Reset badge class configuration for a feature.
 * @param feature Feature to reset.
 */
void ResetBadgeClassConfiguration(GrfSpecFeature feature)
{
	assert(feature < GrfSpecFeature::End);

	auto &config = _badge_config.features[feature];
	config.clear();

	for (const BadgeID &index : GetClassBadges()) {
		const Badge &badge = *GetBadge(index);
		if (badge.name == STR_NULL) continue;
		config.emplace_back(badge.label, 0, true, false);
	}
}

/**
 * Get configuration for a badge class.
 * @param feature Feature being used.
 * @param label Badge class label.
 * @return badge class configuration item.
 */
std::pair<const BadgeClassConfigItem &, int> GetBadgeClassConfigItem(GrfSpecFeature feature, std::string_view label)
{
	if (BadgeClassConfig::CONFIGURABLE_FEATURES.Test(feature)) {
		auto config = GetBadgeClassConfiguration(feature);
		auto found = std::ranges::find(config, label, &BadgeClassConfigItem::label);
		if (found != std::end(config)) {
			/* Sort order is simply the position in the configuration list. */
			return {*found, static_cast<int>(std::distance(std::begin(config), found))};
		}
	}

	return {BadgeClassConfig::EMPTY_CONFIG_ITEM, 0};
}

/**
 * Load badge column preferences.
 * @param ini IniFile to load to.
 * @param feature Feature to load.
 */
static void BadgeClassLoadConfigFeature(const IniFile &ini, GrfSpecFeature feature)
{
	assert(BadgeClassConfig::CONFIGURABLE_FEATURES.Test(feature));
	assert(!BadgeClassConfig::sections[feature].empty());

	auto &config = _badge_config.features[feature];
	config.clear();

	const IniGroup *group = ini.GetGroup(BadgeClassConfig::sections[feature]);
	if (group == nullptr) return;

	for (const IniItem &item : group->items) {
		int column = 0;
		bool show_icon = true;
		bool show_filter = false;

		if (item.value.has_value() && !item.value.value().empty()) {
			StringConsumer consumer(item.value.value());
			if (consumer.ReadCharIf('?')) show_filter = true;
			if (consumer.ReadCharIf('!')) show_icon = false;
			if (auto value = consumer.TryReadIntegerBase<int>(10); value.has_value()) column = *value;
		}

		config.emplace_back(item.name, column, show_icon, show_filter);
	}
}

/**
 * Load badge column preferences.
 * @param ini IniFile to load to.
 */
void BadgeClassLoadConfig(const IniFile &ini)
{
	for (const GrfSpecFeature &feature : BadgeClassConfig::CONFIGURABLE_FEATURES) {
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
	assert(BadgeClassConfig::CONFIGURABLE_FEATURES.Test(feature));
	assert(!BadgeClassConfig::sections[feature].empty());

	IniGroup &group = ini.GetOrCreateGroup(BadgeClassConfig::sections[feature]);
	group.Clear();

	for (const auto &item : _badge_config.features[feature]) {
		group.CreateItem(item.label).SetValue(fmt::format("{}{}{}", item.show_filter ? "?" : "", item.show_icon ? "" : "!", item.column));
	}
}

/**
 * Save badge column preferences.
 * @param ini IniFile to save to.
 */
void BadgeClassSaveConfig(IniFile &ini)
{
	for (const GrfSpecFeature &feature : BadgeClassConfig::CONFIGURABLE_FEATURES) {
		BadgeClassSaveConfigFeature(ini, feature);
	}
}
