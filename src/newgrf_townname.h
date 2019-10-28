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
	byte prob;     ///< The relative probability of the following name to appear in the bottom 7 bits.
	union {
		char *text;    ///< If probability bit 7 is clear
		byte id;       ///< If probability bit 7 is set
	} data;
};

struct NamePartList {
	byte partcount;
	byte bitstart;
	byte bitcount;
	uint16 maxprob;
	NamePart *parts;
};

struct GRFTownName {
	uint32 grfid;
	byte nb_gen;
	byte id[128];
	StringID name[128];
	byte nbparts[128];
	NamePartList *partlist[128];
	GRFTownName *next;
};

GRFTownName *AddGRFTownName(uint32 grfid);
GRFTownName *GetGRFTownName(uint32 grfid);
void DelGRFTownName(uint32 grfid);
void CleanUpGRFTownNames();
StringID *GetGRFTownNameList();
char *GRFTownNameGenerate(char *buf, uint32 grfid, uint16 gen, uint32 seed, const char *last);
uint32 GetGRFTownNameId(int gen);
uint16 GetGRFTownNameType(int gen);

#endif /* NEWGRF_TOWNNAME_H */
