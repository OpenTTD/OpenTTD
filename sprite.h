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
	DrawTileSeqStruct const *seq;
} DrawTileSprites;

// Iterate through all DrawTileSeqStructs in DrawTileSprites.
#define foreach_draw_tile_seq(idx, list) for (idx = list; ((byte) idx->delta_x) != 0x80; idx++)


/* This is for custom sprites: */


typedef struct SpriteGroup SpriteGroup;

typedef struct RealSpriteGroup {
	// XXX: Would anyone ever need more than 16 spritesets? Maybe we should
	// use even less, now we take whole 8kb for custom sprites table, oh my!
	byte sprites_per_set; // means number of directions - 4 or 8

	// Loaded = in motion, loading = not moving
	// Each group contains several spritesets, for various loading stages

	// XXX: For stations the meaning is different - loaded is for stations
	// with small amount of cargo whilst loading is for stations with a lot
	// of da stuff.

	byte loaded_count;
	SpriteGroup *loaded[16]; // sprite ids
	byte loading_count;
	SpriteGroup *loading[16]; // sprite ids
} RealSpriteGroup;

/* Shared by deterministic and random groups. */
typedef enum VarSpriteGroupScope {
	VSG_SCOPE_SELF,
	// Engine of consists for vehicles, city for stations.
	VSG_SCOPE_PARENT,
} VarSpriteGroupScope;

typedef struct DeterministicSpriteGroupRanges DeterministicSpriteGroupRanges;

typedef enum DeterministicSpriteGroupOperation {
	DSG_OP_NONE,
	DSG_OP_DIV,
	DSG_OP_MOD,
} DeterministicSpriteGroupOperation;

typedef struct DeterministicSpriteGroupRange DeterministicSpriteGroupRange;

typedef struct DeterministicSpriteGroup {
	// Take this variable:
	VarSpriteGroupScope var_scope;
	byte variable;
	byte parameter; ///< Used for variables between 0x60 and 0x7F inclusive.

	// Do this with it:
	byte shift_num;
	byte and_mask;

	// Then do this with it:
	DeterministicSpriteGroupOperation operation;
	byte add_val;
	byte divmod_val;

	// And apply it to this:
	byte num_ranges;
	DeterministicSpriteGroupRange *ranges; // Dynamically allocated

	// Dynamically allocated, this is the sole owner
	SpriteGroup *default_group;
} DeterministicSpriteGroup;

typedef enum RandomizedSpriteGroupCompareMode {
	RSG_CMP_ANY,
	RSG_CMP_ALL,
} RandomizedSpriteGroupCompareMode;

typedef struct RandomizedSpriteGroup {
	// Take this object:
	VarSpriteGroupScope var_scope;

	// Check for these triggers:
	RandomizedSpriteGroupCompareMode cmp_mode;
	byte triggers;

	// Look for this in the per-object randomized bitmask:
	byte lowest_randbit;
	byte num_groups; // must be power of 2

	// Take the group with appropriate index:
	SpriteGroup **groups;
} RandomizedSpriteGroup;

typedef struct CallbackResultSpriteGroup {
	uint16 result;
} CallbackResultSpriteGroup;

typedef struct ResultSpriteGroup {
	uint16 result;
	byte sprites;
} ResultSpriteGroup;

typedef enum SpriteGroupType {
	SGT_REAL,
	SGT_DETERMINISTIC,
	SGT_RANDOMIZED,
	SGT_CALLBACK,
	SGT_RESULT,
} SpriteGroupType;

struct SpriteGroup {
	SpriteGroupType type;
	byte ref_count;

	union {
		RealSpriteGroup real;
		DeterministicSpriteGroup determ;
		RandomizedSpriteGroup random;
		CallbackResultSpriteGroup callback;
		ResultSpriteGroup result;
	} g;
};

struct DeterministicSpriteGroupRange {
	SpriteGroup *group;
	byte low;
	byte high;
};

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
