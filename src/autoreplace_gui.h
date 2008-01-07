/* $Id$ */

/** @file autoreplace_gui.h Functions related to the autoreplace GUIs*/

#ifndef AUTOREPLACE_GUI_H
#define AUTOREPLACE_GUI_H

#include "vehicle_type.h"

/**
 * When an engine is made buildable or is removed from being buildable, add/remove it from the build/autoreplace lists
 * @param type The type of engine
 */
void AddRemoveEngineFromAutoreplaceAndBuildWindows(VehicleType type);
void InvalidateAutoreplaceWindow(EngineID e, GroupID id_g);
void ShowReplaceVehicleWindow(VehicleType vehicletype);
void ShowReplaceGroupVehicleWindow(GroupID group, VehicleType veh);

#endif /* AUTOREPLACE_GUI_H */
