/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file townname_gui.cpp GUI helpers for town name generators. */

#include "stdafx.h"
#include "townname_gui.h"

#include "dropdown_common_type.h"
#include "dropdown_func.h"
#include "newgrf_townname.h"

#include "table/strings.h"

#include "safeguards.h"

DropDownList BuildTownNameDropDown()
{
	DropDownList list;

	/* Add and sort NewGRF townname generators. */
	const auto &grf_names = GetGRFTownNameList();
	for (uint i = 0; i < grf_names.size(); i++) {
		list.push_back(MakeDropDownListStringItem(grf_names[i], BUILTIN_TOWNNAME_GENERATOR_COUNT + i));
	}
	std::sort(list.begin(), list.end(), DropDownListStringItem::NatSortFunc);

	size_t newgrf_size = list.size();
	/* Insert a separator after NewGRF town names. */
	if (newgrf_size > 0) {
		list.push_back(MakeDropDownListDividerItem());
		newgrf_size++;
	}

	/* Add and sort built-in townname generators. */
	for (uint i = 0; i < BUILTIN_TOWNNAME_GENERATOR_COUNT; i++) {
		list.push_back(MakeDropDownListStringItem(STR_MAPGEN_TOWN_NAME_ORIGINAL_ENGLISH + i, i));
	}
	std::sort(list.begin() + newgrf_size, list.end(), DropDownListStringItem::NatSortFunc);

	return list;
}

StringID GetTownNameGeneratorName(uint gen)
{
	return gen < BUILTIN_TOWNNAME_GENERATOR_COUNT ?
			STR_MAPGEN_TOWN_NAME_ORIGINAL_ENGLISH + gen :
			GetGRFTownNameName(gen - BUILTIN_TOWNNAME_GENERATOR_COUNT);
}
