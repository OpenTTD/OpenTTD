/* $Id$ */

/** @file newgrf_spritegroup.h */

#ifndef NEWGRF_SPRITEGROUP_H
#define NEWGRF_SPRITEGROUP_H

#include "town.h"
#include "industry.h"

struct SpriteGroup;


/* 'Real' sprite groups contain a list of other result or callback sprite
 * groups. */
struct RealSpriteGroup {
	/* Loaded = in motion, loading = not moving
	 * Each group contains several spritesets, for various loading stages */

	/* XXX: For stations the meaning is different - loaded is for stations
	 * with small amount of cargo whilst loading is for stations with a lot
	 * of da stuff. */

	byte num_loaded;       ///< Number of loaded groups
	byte num_loading;      ///< Number of loading groups
	const SpriteGroup **loaded;  ///< List of loaded groups (can be SpriteIDs or Callback results)
	const SpriteGroup **loading; ///< List of loading groups (can be SpriteIDs or Callback results)
};

/* Shared by deterministic and random groups. */
enum VarSpriteGroupScope {
	VSG_SCOPE_SELF,
	/* Engine of consists for vehicles, city for stations. */
	VSG_SCOPE_PARENT,
};

enum DeterministicSpriteGroupSize {
	DSG_SIZE_BYTE,
	DSG_SIZE_WORD,
	DSG_SIZE_DWORD,
};

enum DeterministicSpriteGroupAdjustType {
	DSGA_TYPE_NONE,
	DSGA_TYPE_DIV,
	DSGA_TYPE_MOD,
};

enum DeterministicSpriteGroupAdjustOperation {
	DSGA_OP_ADD,  ///< a + b
	DSGA_OP_SUB,  ///< a - b
	DSGA_OP_SMIN, ///< (signed) min(a, b)
	DSGA_OP_SMAX, ///< (signed) max(a, b)
	DSGA_OP_UMIN, ///< (unsigned) min(a, b)
	DSGA_OP_UMAX, ///< (unsigned) max(a, b)
	DSGA_OP_SDIV, ///< (signed) a / b
	DSGA_OP_SMOD, ///< (signed) a % b
	DSGA_OP_UDIV, ///< (unsigned) a / b
	DSGA_OP_UMOD, ///< (unsigned) a & b
	DSGA_OP_MUL,  ///< a * b
	DSGA_OP_AND,  ///< a & b
	DSGA_OP_OR,   ///< a | b
	DSGA_OP_XOR,  ///< a ^ b
	DSGA_OP_STO,  ///< store a into temporary storage, indexed by b. return a
	DSGA_OP_RST,  ///< return b
};


struct DeterministicSpriteGroupAdjust {
	DeterministicSpriteGroupAdjustOperation operation;
	DeterministicSpriteGroupAdjustType type;
	byte variable;
	byte parameter; ///< Used for variables between 0x60 and 0x7F inclusive.
	byte shift_num;
	uint32 and_mask;
	uint32 add_val;
	uint32 divmod_val;
	const SpriteGroup *subroutine;
};


struct DeterministicSpriteGroupRange {
	const SpriteGroup *group;
	uint32 low;
	uint32 high;
};


struct DeterministicSpriteGroup {
	VarSpriteGroupScope var_scope;
	DeterministicSpriteGroupSize size;
	byte num_adjusts;
	byte num_ranges;
	DeterministicSpriteGroupAdjust *adjusts;
	DeterministicSpriteGroupRange *ranges; // Dynamically allocated

	/* Dynamically allocated, this is the sole owner */
	const SpriteGroup *default_group;
};

enum RandomizedSpriteGroupCompareMode {
	RSG_CMP_ANY,
	RSG_CMP_ALL,
};

struct RandomizedSpriteGroup {
	VarSpriteGroupScope var_scope;  ///< Take this object:

	RandomizedSpriteGroupCompareMode cmp_mode; ///< Check for these triggers:
	byte triggers;

	byte lowest_randbit; ///< Look for this in the per-object randomized bitmask:
	byte num_groups; ///< must be power of 2

	const SpriteGroup **groups; ///< Take the group with appropriate index:
};


/* This contains a callback result. A failed callback has a value of
 * CALLBACK_FAILED */
struct CallbackResultSpriteGroup {
	uint16 result;
};


/* A result sprite group returns the first SpriteID and the number of
 * sprites in the set */
struct ResultSpriteGroup {
	SpriteID sprite;
	byte num_sprites;
};

struct TileLayoutSpriteGroup {
	byte num_sprites; ///< Number of sprites in the spriteset, used for loading stages
	struct DrawTileSprites *dts;
};

/* List of different sprite group types */
enum SpriteGroupType {
	SGT_INVALID,
	SGT_REAL,
	SGT_DETERMINISTIC,
	SGT_RANDOMIZED,
	SGT_CALLBACK,
	SGT_RESULT,
	SGT_TILELAYOUT,
};

/* Common wrapper for all the different sprite group types */
struct SpriteGroup {
	SpriteGroupType type;

	union {
		RealSpriteGroup real;
		DeterministicSpriteGroup determ;
		RandomizedSpriteGroup random;
		CallbackResultSpriteGroup callback;
		ResultSpriteGroup result;
		TileLayoutSpriteGroup layout;
	} g;
};


SpriteGroup *AllocateSpriteGroup();
void InitializeSpriteGroupPool();


struct ResolverObject {
	uint16 callback;
	uint32 callback_param1;
	uint32 callback_param2;

	byte trigger;
	uint32 last_value;
	uint32 reseed;
	VarSpriteGroupScope scope;

	bool info_view; ///< Indicates if the item is being drawn in an info window

	union {
		struct {
			const struct Vehicle *self;
			const struct Vehicle *parent;
			EngineID self_type;
		} vehicle;
		struct {
			TileIndex tile;
		} canal;
		struct {
			TileIndex tile;
			const struct Station *st;
			const struct StationSpec *statspec;
			CargoID cargo_type;
		} station;
		struct {
			TileIndex tile;
			Town *town;
			HouseID house_id;
		} house;
		struct {
			TileIndex tile;
			Industry *ind;
		} industry;
		struct {
			const struct CargoSpec *cs;
		} cargo;
	} u;

	uint32 (*GetRandomBits)(const struct ResolverObject*);
	uint32 (*GetTriggers)(const struct ResolverObject*);
	void (*SetTriggers)(const struct ResolverObject*, int);
	uint32 (*GetVariable)(const struct ResolverObject*, byte, byte, bool*);
	const SpriteGroup *(*ResolveReal)(const struct ResolverObject*, const SpriteGroup*);
};


/* Base sprite group resolver */
const SpriteGroup *Resolve(const SpriteGroup *group, ResolverObject *object);


#endif /* NEWGRF_SPRITEGROUP_H */
