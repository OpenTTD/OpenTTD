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
void ResetSignalVariant(int32_t = 0);
void InitializeRailGUI();
DropDownList GetRailTypeDropDownList(bool for_replacement = false, bool all_option = false);

/** Settings for which signals are shown by the signal GUI. */
enum SignalGUISettings : uint8_t {
	SIGNAL_GUI_PATH = 0, ///< Show path signals only.
	SIGNAL_GUI_ALL = 1,  ///< Show all signals, including block and presignals.
};

/** Settings for which signals are cycled through by control-clicking on the signal with the signal tool. */
enum SignalCycleSettings : uint8_t {
	SIGNAL_CYCLE_PATH = 0, ///< Cycle through path signals only.
	SIGNAL_CYCLE_ALL = 1,  ///< Cycle through all signals visible.
};

#endif /* RAIL_GUI_H */
