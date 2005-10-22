/* $Id$ */

#ifndef NEWGRF_H
#define NEWGRF_H

#include "sprite.h"
#include "station.h"

typedef struct GRFFile GRFFile;
struct GRFFile {
	char *filename;
	uint32 grfid;
	uint16 flags;
	uint16 sprite_offset;
	GRFFile *next;

	/* A sprite group contains all sprites of a given vehicle (or multiple
	 * vehicles) when carrying given cargo. It consists of several sprite
	 * sets.  Group ids are refered as "cargo id"s by TTDPatch
	 * documentation, contributing to the global confusion.
	 *
	 * A sprite set contains all sprites of a given vehicle carrying given
	 * cargo at a given *stage* - that is usually its load stage. Ie. you
	 * can have a spriteset for an empty wagon, wagon full of coal,
	 * half-filled wagon etc.  Each spriteset contains eight sprites (one
	 * per direction) or four sprites if the vehicle is symmetric. */

	int spriteset_start;
	int spriteset_numsets;
	int spriteset_numents;
	int spriteset_feature;

	int spritegroups_count;
	SpriteGroup **spritegroups;

	StationSpec stations[256];

	uint32 param[0x80];
	uint param_end; /// one more than the highest set parameter
};

extern int _grffile_count;
extern GRFFile *_first_grffile;


void LoadNewGRF(uint load_index, uint file_index);

#endif /* NEWGRF_H */
