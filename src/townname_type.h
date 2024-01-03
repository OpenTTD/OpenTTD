/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file townname_type.h
 * Definition of structures used for generating town names.
 */

#ifndef TOWNNAME_TYPE_H
#define TOWNNAME_TYPE_H

#include "newgrf_townname.h"
#include "town_type.h"
#include "string_type.h"

typedef std::set<std::string> TownNames;

static constexpr uint BUILTIN_TOWNNAME_GENERATOR_COUNT = SPECSTR_TOWNNAME_LAST - SPECSTR_TOWNNAME_START + 1; ///< Number of built-in town name generators.

/**
 * Struct holding parameters used to generate town name.
 * Speeds things up a bit because these values are computed only once per name generation.
 */
struct TownNameParams {
	uint32_t grfid; ///< newgrf ID (0 if not used)
	uint16_t type;  ///< town name style

	/**
	 * Initializes this struct from language ID
	 * @param town_name town name 'language' ID
	 */
	TownNameParams(byte town_name)
	{
		bool grf = town_name >= BUILTIN_TOWNNAME_GENERATOR_COUNT;
		this->grfid = grf ? GetGRFTownNameId(town_name - BUILTIN_TOWNNAME_GENERATOR_COUNT) : 0;
		this->type = grf ? GetGRFTownNameType(town_name - BUILTIN_TOWNNAME_GENERATOR_COUNT) : SPECSTR_TOWNNAME_START + town_name;
	}

	TownNameParams(const Town *t);
};

#endif /* TOWNNAME_TYPE_H */
