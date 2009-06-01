/* $Id$ */

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
