/* $Id$ */

#ifndef SPRITE_H
#define SPRITE_H


/* The following describes bunch of sprites to be drawn together in a single 3D
 * bounding box. Used especially for various multi-sprite buildings (like
 * depots or stations): */

typedef struct DrawTileSeqStruct {
	int8 delta_x; // 0x80 is sequence terminator
	int8 delta_y;
	int8 delta_z;
	byte width,height;
	byte unk; // 'depth', just z-size; TODO: rename
	uint32 image;
} DrawTileSeqStruct;

typedef struct DrawTileSprites {
	SpriteID ground_sprite;
	const DrawTileSeqStruct* seq;
} DrawTileSprites;

/**
 * This structure is the same for both Industries and Houses.
 * Buildings here reference a general type of construction
 */
typedef struct DrawBuildingsTileStruct {
	SpriteID ground;
	SpriteID building;
	byte subtile_x:4;
	byte subtile_y:4;
	byte width:4;
	byte height:4;
	byte dz;
	byte draw_proc;  /* this allows to specify a special drawing procedure.*/
} DrawBuildingsTileStruct;

// Iterate through all DrawTileSeqStructs in DrawTileSprites.
#define foreach_draw_tile_seq(idx, list) for (idx = list; ((byte) idx->delta_x) != 0x80; idx++)


// XXX Temporary include while juggling about
#include "newgrf_spritegroup.h"


/* This takes value (probably of the variable specified in the group) and
 * chooses corresponding SpriteGroup accordingly to the given
 * DeterministicSpriteGroup. */
SpriteGroup *EvalDeterministicSpriteGroup(const DeterministicSpriteGroup *dsg, int value);
/* Get value of a common deterministic SpriteGroup variable. */
int GetDeterministicSpriteValue(byte var);

/* This takes randomized bitmask (probably associated with
 * vehicle/station/whatever) and chooses corresponding SpriteGroup
 * accordingly to the given RandomizedSpriteGroup. */
SpriteGroup *EvalRandomizedSpriteGroup(const RandomizedSpriteGroup *rsg, byte random_bits);
/* Triggers given RandomizedSpriteGroup with given bitmask and returns and-mask
 * of random bits to be reseeded, or zero if there were no triggers matched
 * (then they are |ed to @waiting_triggers instead). */
byte RandomizedSpriteGroupTriggeredBits(const RandomizedSpriteGroup *rsg, byte triggers, byte *waiting_triggers);

#endif /* SPRITE_H */
