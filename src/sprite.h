/* $Id$ */

#ifndef SPRITE_H
#define SPRITE_H


/* The following describes bunch of sprites to be drawn together in a single 3D
 * bounding box. Used especially for various multi-sprite buildings (like
 * depots or stations): */

struct DrawTileSeqStruct {
	int8 delta_x; // 0x80 is sequence terminator
	int8 delta_y;
	int8 delta_z;
	byte size_x;
	byte size_y;
	byte size_z;
	SpriteID image;
	SpriteID pal;
};

struct DrawTileSprites {
	SpriteID ground_sprite;
	SpriteID ground_pal;
	const DrawTileSeqStruct* seq;
};

/**
 * This structure is the same for both Industries and Houses.
 * Buildings here reference a general type of construction
 */
struct DrawBuildingsTileStruct {
	PalSpriteID ground;
	PalSpriteID building;
	byte subtile_x:4;
	byte subtile_y:4;
	byte width:4;
	byte height:4;
	byte dz;
	byte draw_proc;  /* this allows to specify a special drawing procedure.*/
};

// Iterate through all DrawTileSeqStructs in DrawTileSprites.
#define foreach_draw_tile_seq(idx, list) for (idx = list; ((byte) idx->delta_x) != 0x80; idx++)


#endif /* SPRITE_H */
