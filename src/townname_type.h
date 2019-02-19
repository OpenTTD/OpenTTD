/* $Id$ */

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
#include <set>
#include <string>

typedef std::set<std::string> TownNames;

/**
 * Struct holding parameters used to generate town name.
 * Speeds things up a bit because these values are computed only once per name generation.
 */
struct TownNameParams {
	uint32 grfid; ///< newgrf ID (0 if not used)
	uint16 type;  ///< town name style

	/**
	 * Initializes this struct from language ID
	 * @param town_name town name 'language' ID
	 */
	TownNameParams(byte town_name)
	{
		extern int _nb_orig_names;
		bool grf = town_name >= _nb_orig_names;
		this->grfid = grf ? GetGRFTownNameId(town_name - _nb_orig_names) : 0;
		this->type = grf ? GetGRFTownNameType(town_name - _nb_orig_names) : SPECSTR_TOWNNAME_START + town_name;
	}

	TownNameParams(const Town *t);
};

#endif /* TOWNNAME_TYPE_H */
