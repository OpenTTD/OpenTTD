/* $Id$ */

#ifndef NEWGRF_SPRITEGROUP_H
#define NEWGRF_SPRITEGROUP_H


typedef struct SpriteGroup SpriteGroup;

typedef struct RealSpriteGroup {
	byte sprites_per_set; // means number of directions - 4 or 8

	// Loaded = in motion, loading = not moving
	// Each group contains several spritesets, for various loading stages

	// XXX: For stations the meaning is different - loaded is for stations
	// with small amount of cargo whilst loading is for stations with a lot
	// of da stuff.

	byte loaded_count;     ///< Number of loaded groups
	SpriteGroup **loaded;  ///< List of loaded groups (can be SpriteIDs or Callback results)
	byte loading_count;    ///< Number of loading groups
	SpriteGroup **loading; ///< List of loading groups (can be SpriteIDs or Callback results)
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
	SGT_INVALID,
	SGT_REAL,
	SGT_DETERMINISTIC,
	SGT_RANDOMIZED,
	SGT_CALLBACK,
	SGT_RESULT,
} SpriteGroupType;

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

struct DeterministicSpriteGroupRange {
	SpriteGroup *group;
	byte low;
	byte high;
};


SpriteGroup *AllocateSpriteGroup(void);
void InitializeSpriteGroupPool(void);

#endif /* NEWGRF_SPRITEGROUP_H */
