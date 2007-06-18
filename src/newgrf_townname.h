/* $Id$ */
#ifndef NEWGRF_TOWNNAME_H
#define NEWGRF_TOWNNAME_H

/** @file newgrf_townname.h
 * Header of Action 0F "universal holder" structure and functions
 */

struct NamePart {
	byte prob;     ///< The relative probablity of the following name to appear in the bottom 7 bits.
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
