/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file terraform_gui.h GUI stuff related to terraforming. */

#ifndef TERRAFORM_GUI_H
#define TERRAFORM_GUI_H

#include "window_type.h"

void CcTerraform(bool success, TileIndex tile, uint32 p1, uint32 p2);

Window *ShowTerraformToolbar(Window *link = NULL);
void ShowTerraformToolbarWithTool(uint16 key, uint16 keycode);
Window *ShowEditorTerraformToolbar();
void ShowEditorTerraformToolbarWithTool(uint16 key, uint16 keycode);

#endif /* GUI_H */
