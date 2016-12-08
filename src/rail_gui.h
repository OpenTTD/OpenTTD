/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file rail_gui.h Functions/types etc. related to the rail GUI. */

#ifndef RAIL_GUI_H
#define RAIL_GUI_H

#include "rail_type.h"
#include "widgets/dropdown_type.h"

struct Window *ShowBuildRailToolbar(RailType railtype);
void ReinitGuiAfterToggleElrail(bool disable);
bool ResetSignalVariant(int32 = 0);
void InitializeRailGUI();
DropDownList *GetRailTypeDropDownList(bool for_replacement = false, bool all_option = false);

#endif /* RAIL_GUI_H */
