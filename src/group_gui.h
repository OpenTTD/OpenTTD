/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group_gui.h Functions/definitions that have something to do with groups. */

#ifndef GROUP_GUI_H
#define GROUP_GUI_H

#include "company_type.h"
#include "vehicle_type.h"

void ShowCompanyGroup(CompanyID company, VehicleType veh, GroupID group = INVALID_GROUP, bool need_existing_window = false);
void ShowCompanyGroupForVehicle(const Vehicle *v);
void DeleteGroupHighlightOfVehicle(const Vehicle *v);

#endif /* GROUP_GUI_H */
