#ifndef SPRITE_H
#define SPRITE_H


/* The following describes bunch of sprites to be drawn together in a single 3D
 * bounding box. Used especially for various multi-sprite buildings (like
 * depots or stations): */

typedef struct DrawTileSeqStruct {
	int8 delta_x;
	int8 delta_y;
	int8 delta_z;
	byte width,height;
	byte unk; // 'depth', just z-size; TODO: rename
	uint32 image;
} DrawTileSeqStruct;

typedef struct DrawTileSprites {
	SpriteID ground_sprite;
	DrawTileSeqStruct const *seq;
} DrawTileSprites;

#define foreach_draw_tile_seq(idx, list) for (idx = list; ((byte) idx->delta_x) != 0x80; idx++)


/* This is for custom sprites: */

struct SpriteGroup {
	// XXX: Would anyone ever need more than 16 spritesets? Maybe we should
	// use even less, now we take whole 8kb for custom sprites table, oh my!
	byte sprites_per_set; // means number of directions - 4 or 8

	// Loaded = in motion, loading = not moving
	// Each group contains several spritesets, for various loading stages

	// XXX: For stations the meaning is different - loaded is for stations
	// with small amount of cargo whilst loading is for stations with a lot
	// of da stuff.

	byte loaded_count;
	uint16 loaded[16]; // sprite ids
	byte loading_count;
	uint16 loading[16]; // sprite ids
};

#endif
