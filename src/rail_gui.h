/* $Id$ */

/** @file rail_gui.h Functions/types etc. related to the rail GUI. */

#ifndef RAIL_GUI_H
#define RAIL_GUI_H

#include "rail_type.h"

void ShowBuildRailToolbar(RailType railtype, int button);
void ReinitGuiAfterToggleElrail(bool disable);
int32 ResetSignalVariant(int32 = 0);

#endif /* RAIL_GUI_H */
