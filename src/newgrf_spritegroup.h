/* $Id$ */

#ifndef NEWGRF_SPRITEGROUP_H
#define NEWGRF_SPRITEGROUP_H


typedef struct SpriteGroup SpriteGroup;


/* 'Real' sprite groups contain a list of other result or callback sprite
 * groups. */
typedef struct RealSpriteGroup {
	// Loaded = in motion, loading = not moving
	// Each group contains several spritesets, for various loading stages

	// XXX: For stations the meaning is different - loaded is for stations
	// with small amount of cargo whilst loading is for stations with a lot
	// of da stuff.

	byte num_loaded;       ///< Number of loaded groups
	byte num_loading;      ///< Number of loading groups
	const SpriteGroup **loaded;  ///< List of loaded groups (can be SpriteIDs or Callback results)
	const SpriteGroup **loading; ///< List of loading groups (can be SpriteIDs or Callback results)
} RealSpriteGroup;

/* Shared by deterministic and random groups. */
typedef enum VarSpriteGroupScopes {
	VSG_SCOPE_SELF,
	// Engine of consists for vehicles, city for stations.
	VSG_SCOPE_PARENT,
} VarSpriteGroupScope;

typedef enum DeterministicSpriteGroupSizes {
	DSG_SIZE_BYTE,
	DSG_SIZE_WORD,
	DSG_SIZE_DWORD,
} DeterministicSpriteGroupSize;

typedef enum DeterministicSpriteGroupAdjustTypes {
	DSGA_TYPE_NONE,
	DSGA_TYPE_DIV,
	DSGA_TYPE_MOD,
} DeterministicSpriteGroupAdjustType;

typedef enum DeterministicSpriteGroupAdjustOperations {
	DSGA_OP_ADD,  // a + b
	DSGA_OP_SUB,  // a - b
	DSGA_OP_SMIN, // (signed) min(a, b)
	DSGA_OP_SMAX, // (signed) max(a, b)
	DSGA_OP_UMIN, // (unsigned) min(a, b)
	DSGA_OP_UMAX, // (unsigned) max(a, b)
	DSGA_OP_SDIV, // (signed) a / b
	DSGA_OP_SMOD, // (signed) a % b
	DSGA_OP_UDIV, // (unsigned) a / b
	DSGA_OP_UMOD, // (unsigned) a & b
	DSGA_OP_MUL,  // a * b
	DSGA_OP_AND,  // a & b
	DSGA_OP_OR,   // a | b
	DSGA_OP_XOR,  // a ^ b
} DeterministicSpriteGroupAdjustOperation;


typedef struct DeterministicSpriteGroupAdjust {
	DeterministicSpriteGroupAdjustOperation operation;
	DeterministicSpriteGroupAdjustType type;
	byte variable;
	byte parameter; ///< Used for variables between 0x60 and 0x7F inclusive.
	byte shift_num;
	uint32 and_mask;
	uint32 add_val;
	uint32 divmod_val;
	const SpriteGroup *subroutine;
} DeterministicSpriteGroupAdjust;


typedef struct DeterministicSpriteGroupRange {
	const SpriteGroup *group;
	uint32 low;
	uint32 high;
} DeterministicSpriteGroupRange;


typedef struct DeterministicSpriteGroup {
	VarSpriteGroupScope var_scope;
	DeterministicSpriteGroupSize size;
	byte num_adjusts;
	byte num_ranges;
	DeterministicSpriteGroupAdjust *adjusts;
	DeterministicSpriteGroupRange *ranges; // Dynamically allocated

	// Dynamically allocated, this is the sole owner
	const SpriteGroup *default_group;
} DeterministicSpriteGroup;

typedef enum RandomizedSpriteGroupCompareModes {
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
	const SpriteGroup **groups;
} RandomizedSpriteGroup;


/* This contains a callback result. A failed callback has a value of
 * CALLBACK_FAILED */
typedef struct CallbackResultSpriteGroup {
	uint16 result;
} CallbackResultSpriteGroup;


/* A result sprite group returns the first SpriteID and the number of
 * sprites in the set */
typedef struct ResultSpriteGroup {
	SpriteID sprite;
	byte num_sprites;
} ResultSpriteGroup;

/* List of different sprite group types */
typedef enum SpriteGroupType {
	SGT_INVALID,
	SGT_REAL,
	SGT_DETERMINISTIC,
	SGT_RANDOMIZED,
	SGT_CALLBACK,
	SGT_RESULT,
} SpriteGroupType;

/* Common wrapper for all the different sprite group types */
struct SpriteGroup {
	SpriteGroupType type;

	union {
		RealSpriteGroup real;
		DeterministicSpriteGroup determ;
		RandomizedSpriteGroup random;
		CallbackResultSpriteGroup callback;
		ResultSpriteGroup result;
	} g;
};


SpriteGroup *AllocateSpriteGroup(void);
void InitializeSpriteGroupPool(void);


typedef struct ResolverObject {
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
			const struct Station *st;
			const struct StationSpec *statspec;
			CargoID cargo_type;
		} station;
	} u;

	uint32 (*GetRandomBits)(const struct ResolverObject*);
	uint32 (*GetTriggers)(const struct ResolverObject*);
	void (*SetTriggers)(const struct ResolverObject*, int);
	uint32 (*GetVariable)(const struct ResolverObject*, byte, byte, bool*);
	const SpriteGroup *(*ResolveReal)(const struct ResolverObject*, const SpriteGroup*);
} ResolverObject;


/* Base sprite group resolver */
const SpriteGroup *Resolve(const SpriteGroup *group, ResolverObject *object);


#endif /* NEWGRF_SPRITEGROUP_H */
