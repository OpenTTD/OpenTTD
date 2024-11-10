/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_badge_config.h Functions related to NewGRF badge configuration. */

#ifndef NEWGRF_BADGE_CONFIG_H
#define NEWGRF_BADGE_CONFIG_H

#include "dropdown_type.h"
#include "newgrf.h"
#include "newgrf_badge_type.h"
#include "strings_type.h"

void BadgeClassLoadConfig(const struct IniFile &ini);
void BadgeClassSaveConfig(struct IniFile &ini);

void AddBadgeClassToConfiguration(std::string_view label);
bool ApplyBadgeClassConfiguration(GrfSpecFeature feature, std::string_view label, bool &visible, uint8_t &column, uint &sort_order);

DropDownList BuildBadgeClassConfigurationList(GrfSpecFeature feature, uint columns, std::span<const StringID> column_separators);

void BadgeClassToggleVisibility(GrfSpecFeature feature, BadgeClassID class_index);
void BadgeClassMovePrevious(GrfSpecFeature feature, BadgeClassID class_index);
void BadgeClassMoveNext(GrfSpecFeature feature, BadgeClassID class_index, uint columns);

#endif /* NEWGRF_BADGE_CONFIG_H */
