/* $Id$ */

/** @file newgrf_spritegroup.h Action 2 handling. */

#ifndef NEWGRF_SPRITEGROUP_H
#define NEWGRF_SPRITEGROUP_H

#include "town_type.h"
#include "industry_type.h"
#include "core/bitmath_func.hpp"
#include "gfx_type.h"
#include "engine_type.h"
#include "tile_type.h"
#include "core/pool_type.hpp"

#include "newgrf_cargo.h"
#include "newgrf_callbacks.h"
#include "newgrf_generic.h"
#include "newgrf_storage.h"

/**
 * Gets the value of a so-called newgrf "register".
 * @param i index of the register
 * @pre i < 0x110
 * @return the value of the register
 */
static inline uint32 GetRegister(uint i)
{
	extern TemporaryStorageArray<uint32, 0x110> _temp_store;
	return _temp_store.Get(i);
}

/* List of different sprite group types */
enum SpriteGroupType {
	SGT_REAL,
	SGT_DETERMINISTIC,
	SGT_RANDOMIZED,
	SGT_CALLBACK,
	SGT_RESULT,
	SGT_TILELAYOUT,
	SGT_INDUSTRY_PRODUCTION,
};

struct SpriteGroup;
typedef uint32 SpriteGroupID;
typedef Pool<SpriteGroup, SpriteGroupID, 512, 64000> SpriteGroupPool;
extern SpriteGroupPool _spritegroup_pool;

/* Common wrapper for all the different sprite group types */
struct SpriteGroup : SpriteGroupPool::PoolItem<&_spritegroup_pool> {
protected:
	SpriteGroup(SpriteGroupType type) : type(type) {}

public:
	virtual ~SpriteGroup() {}

	SpriteGroupType type;

	virtual SpriteID GetResult() const { return 0; }
	virtual byte GetNumResults() const { return 0; }
	virtual uint16 GetCallbackResult() const { return CALLBACK_FAILED; }
};


/* 'Real' sprite groups contain a list of other result or callback sprite
 * groups. */
struct RealSpriteGroup : SpriteGroup {
	RealSpriteGroup() : SpriteGroup(SGT_REAL) {}
	~RealSpriteGroup();

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
	/* Any vehicle in the consist (vehicles only) */
	VSG_SCOPE_RELATIVE,
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
	DSGA_OP_STOP, ///< store a into persistent storage, indexed by b, return a
	DSGA_OP_ROR,  ///< rotate a b positions to the right
	DSGA_OP_SCMP, ///< (signed) comparision (a < b -> 0, a == b = 1, a > b = 2)
	DSGA_OP_UCMP, ///< (unsigned) comparision (a < b -> 0, a == b = 1, a > b = 2)
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


struct DeterministicSpriteGroup : SpriteGroup {
	DeterministicSpriteGroup() : SpriteGroup(SGT_DETERMINISTIC) {}
	~DeterministicSpriteGroup();

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

struct RandomizedSpriteGroup : SpriteGroup {
	RandomizedSpriteGroup() : SpriteGroup(SGT_RANDOMIZED) {}
	~RandomizedSpriteGroup();

	VarSpriteGroupScope var_scope;  ///< Take this object:

	RandomizedSpriteGroupCompareMode cmp_mode; ///< Check for these triggers:
	byte triggers;
	byte count;

	byte lowest_randbit; ///< Look for this in the per-object randomized bitmask:
	byte num_groups; ///< must be power of 2

	const SpriteGroup **groups; ///< Take the group with appropriate index:
};


/* This contains a callback result. A failed callback has a value of
 * CALLBACK_FAILED */
struct CallbackResultSpriteGroup : SpriteGroup {
	/**
	 * Creates a spritegroup representing a callback result
	 * @param result The value that was used to represent this callback result
	 */
	CallbackResultSpriteGroup(uint16 value) :
		SpriteGroup(SGT_CALLBACK),
		result(value)
	{
		/* Old style callback results have the highest byte 0xFF so signify it is a callback result
		 * New style ones only have the highest bit set (allows 15-bit results, instead of just 8) */
		if ((this->result >> 8) == 0xFF) {
			this->result &= ~0xFF00;
		} else {
			this->result &= ~0x8000;
		}
	}

	uint16 result;
	uint16 GetCallbackResult() const { return this->result; }
};


/* A result sprite group returns the first SpriteID and the number of
 * sprites in the set */
struct ResultSpriteGroup : SpriteGroup {
	/**
	 * Creates a spritegroup representing a sprite number result.
	 * @param sprite The sprite number.
	 * @param num_sprites The number of sprites per set.
	 * @return A spritegroup representing the sprite number result.
	 */
	ResultSpriteGroup(SpriteID sprite, byte num_sprites) :
		SpriteGroup(SGT_RESULT),
		sprite(sprite),
		num_sprites(num_sprites)
	{
	}

	SpriteID sprite;
	byte num_sprites;
	SpriteID GetResult() const { return this->sprite; }
	byte GetNumResults() const { return this->num_sprites; }
};

struct TileLayoutSpriteGroup : SpriteGroup {
	TileLayoutSpriteGroup() : SpriteGroup(SGT_TILELAYOUT) {}
	~TileLayoutSpriteGroup();

	byte num_sprites; ///< Number of sprites in the spriteset, used for loading stages
	struct DrawTileSprites *dts;
};

struct IndustryProductionSpriteGroup : SpriteGroup {
	IndustryProductionSpriteGroup() : SpriteGroup(SGT_INDUSTRY_PRODUCTION) {}

	uint8 version;
	uint16 substract_input[3];
	uint16 add_output[2];
	uint8 again;
};


struct ResolverObject {
	CallbackID callback;
	uint32 callback_param1;
	uint32 callback_param2;
	bool procedure_call; ///< true if we are currently resolving a var 0x7E procedure result.

	byte trigger;
	byte count;
	uint32 last_value;
	uint32 reseed;
	VarSpriteGroupScope scope;

	bool info_view; ///< Indicates if the item is being drawn in an info window

	BaseStorageArray *psa; ///< The persistent storage array of this resolved object.

	const GRFFile *grffile; ///< GRFFile the resolved SpriteGroup belongs to

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
			IndustryGfx gfx;
			IndustryType type;
		} industry;
		struct {
			const struct CargoSpec *cs;
		} cargo;
		struct {
			CargoID cargo_type;
			uint8 default_selection;
			IndustryType src_industry;
			IndustryType dst_industry;
			uint8 distance;
			AIConstructionEvent event;
			uint8 count;
			uint8 station_size;
		} generic;
	} u;

	uint32 (*GetRandomBits)(const struct ResolverObject*);
	uint32 (*GetTriggers)(const struct ResolverObject*);
	void (*SetTriggers)(const struct ResolverObject*, int);
	uint32 (*GetVariable)(const struct ResolverObject*, byte, byte, bool*);
	const SpriteGroup *(*ResolveReal)(const struct ResolverObject*, const RealSpriteGroup*);

	ResolverObject() : procedure_call(false) { }
};


/* Base sprite group resolver */
const SpriteGroup *Resolve(const SpriteGroup *group, ResolverObject *object);


#endif /* NEWGRF_SPRITEGROUP_H */
