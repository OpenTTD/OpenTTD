/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_badge_config.h Functions related to NewGRF badge configuration. */

#ifndef NEWGRF_BADGE_CONFIG_H
#define NEWGRF_BADGE_CONFIG_H

#include "newgrf.h"
#include "newgrf_badge_type.h"

class BadgeClassConfigItem {
public:
	std::string label; ///< Class label.
	uint column = 0; ///< UI column, feature-dependent.
	bool show_icon = true; ///< Set if the badge icons should be displayed for this class.
	bool show_filter = false; ///< Set if a drop down filter should be added for this class.
};

void BadgeClassLoadConfig(const struct IniFile &ini);
void BadgeClassSaveConfig(struct IniFile &ini);

std::span<BadgeClassConfigItem> GetBadgeClassConfiguration(GrfSpecFeature feature);
void AddBadgeClassesToConfiguration();
void ResetBadgeClassConfiguration(GrfSpecFeature feature);
std::pair<const BadgeClassConfigItem &, int> GetBadgeClassConfigItem(GrfSpecFeature feature, std::string_view label);

#endif /* NEWGRF_BADGE_CONFIG_H */
