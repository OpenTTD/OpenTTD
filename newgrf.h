/* $Id$ */

#ifndef NEWGRF_H
#define NEWGRF_H

#include "station.h"

typedef struct GRFLabel {
	byte label;
	uint32 nfo_line;
	uint32 pos;
	struct GRFLabel *next;
} GRFLabel;

typedef struct GRFFile {
	char *filename;
	uint32 grfid;
	uint16 flags;
	uint16 sprite_offset;
	byte grf_version;
	struct GRFFile *next;

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

	SpriteID spriteset_start;
	int spriteset_numsets;
	int spriteset_numents;
	int spriteset_feature;

	int spritegroups_count;
	struct SpriteGroup **spritegroups;

	uint sound_offset;

	StationSpec **stations;

	uint32 param[0x80];
	uint param_end; /// one more than the highest set parameter

	GRFLabel *label; ///< Pointer to the first label. This is a linked list, not an array.
} GRFFile;

extern GRFFile *_first_grffile;

typedef struct GRFConfig {
	const char *filename;
	uint32 param[0x80];
	byte num_params;

	struct GRFConfig *next;
} GRFConfig;

extern GRFConfig *_first_grfconfig;
extern SpriteID _signal_base;
extern SpriteID _coast_base;
extern bool _have_2cc;

void LoadNewGRF(uint load_index, uint file_index);

#endif /* NEWGRF_H */
