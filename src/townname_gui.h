/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file townname_gui.h GUI helpers for town name generators. */

#ifndef TOWNNAME_GUI_H
#define TOWNNAME_GUI_H

#include "dropdown_type.h"
#include "strings_type.h"

/**
 * Build a list of town name generators for a dropdown widget.
 * @return Dropdown list with NewGRF generators first, followed by built-in generators.
 */
DropDownList BuildTownNameDropDown();

/**
 * Get the display string for a town name generator.
 * @param gen Town name generator index.
 * @return String ID of the generator name.
 */
StringID GetTownNameGeneratorName(uint gen);

#endif /* TOWNNAME_GUI_H */
