/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file newgrf_townname.h
 * Header of Action 0F "universal holder" structure and functions
 */

#ifndef NEWGRF_TOWNNAME_H
#define NEWGRF_TOWNNAME_H

#include "strings_type.h"

struct NamePart {
	std::string text; ///< If probability bit 7 is clear
	byte id;          ///< If probability bit 7 is set
	byte prob;        ///< The relative probability of the following name to appear in the bottom 7 bits.
};

struct NamePartList {
	byte bitstart;  ///< Start of random seed bits to use.
	byte bitcount;  ///< Number of bits of random seed to use.
	uint16_t maxprob; ///< Total probability of all parts.
	std::vector<NamePart> parts; ///< List of parts to choose from.
};

struct TownNameStyle {
	StringID name; ///< String ID of this town name style.
	byte id;       ///< Index within partlist for this town name style.

	TownNameStyle(StringID name, byte id) : name(name), id(id) { }
};

struct GRFTownName {
	static const uint MAX_LISTS = 128; ///< Maximum number of town name lists that can be defined per GRF.

	uint32_t grfid;                                   ///< GRF ID of NewGRF.
	std::vector<TownNameStyle> styles;              ///< Style names defined by the Town Name NewGRF.
	std::vector<NamePartList> partlists[MAX_LISTS]; ///< Lists of town name parts.
};

GRFTownName *AddGRFTownName(uint32_t grfid);
GRFTownName *GetGRFTownName(uint32_t grfid);
void DelGRFTownName(uint32_t grfid);
void CleanUpGRFTownNames();
uint32_t GetGRFTownNameId(uint16_t gen);
uint16_t GetGRFTownNameType(uint16_t gen);
StringID GetGRFTownNameName(uint16_t gen);

const std::vector<StringID> &GetGRFTownNameList();

#endif /* NEWGRF_TOWNNAME_H */
