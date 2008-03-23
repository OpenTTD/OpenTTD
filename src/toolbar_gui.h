/* $Id$ */

/** @file toolbar_gui.h Stuff related to the (main) toolbar. */

#ifndef TOOLBAR_GUI_H
#define TOOLBAR_GUI_H

#include "window_type.h"

Point GetToolbarDropdownPos(uint16 parent_button, int width, int height);
Window *AllocateToolbar();

#endif /*TOOLBAR_GUI_H*/
