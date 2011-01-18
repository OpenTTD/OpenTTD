/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_gui.h Functions related to the autoreplace GUIs*/

#ifndef AUTOREPLACE_GUI_H
#define AUTOREPLACE_GUI_H

#include "engine_type.h"
#include "group_type.h"
#include "vehicle_type.h"

void AddRemoveEngineFromAutoreplaceAndBuildWindows(VehicleType type);
void InvalidateAutoreplaceWindow(EngineID e, GroupID id_g);
void ShowReplaceGroupVehicleWindow(GroupID group, VehicleType veh);

#endif /* AUTOREPLACE_GUI_H */
