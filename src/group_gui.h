/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file group_gui.h Functions/definitions that have something to do with groups. */

#ifndef GROUP_GUI_H
#define GROUP_GUI_H

#include "company_type.h"
#include "group_type.h"
#include "sortlist_type.h"
#include "vehicle_type.h"

void ShowCompanyGroup(CompanyID company, VehicleType veh, GroupID group = GroupID::Invalid());
void ShowCompanyGroupForVehicle(const Vehicle *v);
void DeleteGroupHighlightOfVehicle(const Vehicle *v);

struct GUIGroupListItem {
	const Group *group;
	uint8_t indent; ///< Display indentation level.
	uint16_t level_mask; ///< Bitmask of indentation continuation.

	constexpr GUIGroupListItem(const Group *group, int8_t indent) : group(group), indent(indent), level_mask(0) {}
};

using GUIGroupList = GUIList<GUIGroupListItem>;

void BuildGuiGroupList(GUIGroupList &dst, bool fold, Owner owner, VehicleType veh_type);

#endif /* GROUP_GUI_H */
