/* $Id$ */

/** @file newgrf.h */

#ifndef NEWGRF_H
#define NEWGRF_H

#include "station.h"
#include "town.h"
#include "newgrf_config.h"
#include "cargotype.h"
#include "industry.h"

enum GrfLoadingStage {
	GLS_FILESCAN,
	GLS_SAFETYSCAN,
	GLS_LABELSCAN,
	GLS_INIT,
	GLS_RESERVE,
	GLS_ACTIVATION,
	GLS_END,
};

DECLARE_POSTFIX_INCREMENT(GrfLoadingStage);

enum GrfMiscBit {
	GMB_DESERT_TREES_FIELDS    = 0, // Unsupported.
	GMB_DESERT_PAVED_ROADS     = 1,
	GMB_FIELD_BOUNDING_BOX     = 2, // Unsupported.
	GMB_TRAIN_WIDTH_32_PIXELS  = 3,
	GMB_AMBIENT_SOUND_CALLBACK = 4, // Unsupported.
	GMB_CATENARY_ON_3RD_TRACK  = 5, // Unsupported.
};

struct GRFLabel {
	byte label;
	uint32 nfo_line;
	uint32 pos;
	struct GRFLabel *next;
};

struct GRFFile {
	char *filename;
	uint32 grfid;
	uint16 sprite_offset;
	byte grf_version;
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

	SpriteID spriteset_start;
	int spriteset_numsets;
	int spriteset_numents;
	int spriteset_feature;

	int spritegroups_count;
	struct SpriteGroup **spritegroups;

	uint sound_offset;

	StationSpec **stations;
	HouseSpec **housespec;
	IndustrySpec **industryspec;
	IndustryTileSpec **indtspec;

	uint32 param[0x80];
	uint param_end;  ///< one more than the highest set parameter

	GRFLabel *label; ///< Pointer to the first label. This is a linked list, not an array.

	uint8 cargo_max;
	CargoLabel *cargo_list;
	uint8 cargo_map[NUM_CARGO];
};

extern GRFFile *_first_grffile;

extern SpriteID _coast_base;

struct GRFLoadedFeatures {
	bool has_2CC;             ///< Set if any vehicle is loaded which uses 2cc (two company colours).
	bool has_newhouses;       ///< Set if there are any newhouses loaded.
	bool has_newindustries;   ///< Set if there are any newindustries loaded.
	bool has_newwater;        ///< Set it there are any newwater grf loaded
};

/* Indicates which are the newgrf features currently loaded ingame */
extern GRFLoadedFeatures _loaded_newgrf_features;

void LoadNewGRFFile(GRFConfig *config, uint file_index, GrfLoadingStage stage);
void LoadNewGRF(uint load_index, uint file_index);
void ReloadNewGRFData(); // in openttd.cpp

void CDECL grfmsg(int severity, const char *str, ...);

bool HasGrfMiscBit(GrfMiscBit bit);

#endif /* NEWGRF_H */
