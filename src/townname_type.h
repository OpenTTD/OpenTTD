/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file townname_type.h Definition of structures used for generating town names. */

#ifndef TOWNNAME_TYPE_H
#define TOWNNAME_TYPE_H

#include "newgrf_townname.h"
#include "town_type.h"
#include "string_type.h"
#include "strings_internal.h"

typedef std::set<std::string> TownNames;

/**
 * Struct holding parameters used to generate town name.
 * Speeds things up a bit because these values are computed only once per name generation.
 */
struct TownNameParams {
	uint32_t grfid; ///< newgrf ID (0 if not used)
	uint16_t type; ///< town name style
	bool use_original_generator; ///< flag to maintain gamesave load compatibility

	/**
	 * Initializes this struct from language ID
	 * @param town_name town name 'language' ID
	 */
	TownNameParams(uint8_t town_name)
	{
		bool grf = town_name >= BUILTIN_TOWNNAME_GENERATOR_COUNT;
		this->grfid = grf ? GetGRFTownNameId(town_name - BUILTIN_TOWNNAME_GENERATOR_COUNT) : 0;
		this->type = grf ? GetGRFTownNameType(town_name - BUILTIN_TOWNNAME_GENERATOR_COUNT) : SPECSTR_TOWNNAME_START + town_name;
		this->use_original_generator = false;
	}

	TownNameParams(const Town *t);
};


/**
 * Type for all town name generator functions.
 * @param builder The builder to write the name to.
 * @param seed The seed of the town name.
 */
typedef void TownNameGenerator(StringBuilder &builder, uint32_t seed);

#endif /* TOWNNAME_TYPE_H */
