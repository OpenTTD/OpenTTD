/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_spritegroup.h Action 2 handling. */

#ifndef NEWGRF_SPRITEGROUP_H
#define NEWGRF_SPRITEGROUP_H

#include "town_type.h"
#include "gfx_type.h"
#include "engine_type.h"
#include "core/pool_type.hpp"
#include "house_type.h"

#include "newgrf_callbacks.h"
#include "newgrf_generic.h"
#include "newgrf_storage.h"
#include "newgrf_commons.h"

/**
 * Gets the value of a so-called newgrf "register".
 * @param i index of the register
 * @pre i < 0x110
 * @return the value of the register
 */
static inline uint32 GetRegister(uint i)
{
	extern TemporaryStorageArray<int32, 0x110> _temp_store;
	return _temp_store.GetValue(i);
}

/**
 * Clears the value of a so-called newgrf "register".
 * @param i index of the register
 * @pre i < 0x110
 */
static inline void ClearRegister(uint i)
{
	extern TemporaryStorageArray<int32, 0x110> _temp_store;
	_temp_store.StoreValue(i, 0);
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

/* SPRITE_WIDTH is 24. ECS has roughly 30 sprite groups per real sprite.
 * Adding an 'extra' margin would be assuming 64 sprite groups per real
 * sprite. 64 = 2^6, so 2^30 should be enough (for now) */
typedef Pool<SpriteGroup, SpriteGroupID, 1024, 1 << 30, PT_DATA> SpriteGroupPool;
extern SpriteGroupPool _spritegroup_pool;

/* Common wrapper for all the different sprite group types */
struct SpriteGroup : SpriteGroupPool::PoolItem<&_spritegroup_pool> {
protected:
	SpriteGroup(SpriteGroupType type) : type(type) {}
	/** Base sprite group resolver */
	virtual const SpriteGroup *Resolve(struct ResolverObject *object) const { return this; };

public:
	virtual ~SpriteGroup() {}

	SpriteGroupType type;

	virtual SpriteID GetResult() const { return 0; }
	virtual byte GetNumResults() const { return 0; }
	virtual uint16 GetCallbackResult() const { return CALLBACK_FAILED; }

	/**
	 * ResolverObject (re)entry point.
	 * This cannot be made a call to a virtual function because virtual functions
	 * do not like NULL and checking for NULL *everywhere* is more cumbersome than
	 * this little helper function.
	 * @param group the group to resolve for
	 * @param object information needed to resolve the group
	 * @return the resolved group
	 */
	static const SpriteGroup *Resolve(const SpriteGroup *group, ResolverObject *object)
	{
		return group == NULL ? NULL : group->Resolve(object);
	}
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

protected:
	const SpriteGroup *Resolve(ResolverObject *object) const;
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
	DSGA_OP_SHL,  ///< a << b
	DSGA_OP_SHR,  ///< (unsigned) a >> b
	DSGA_OP_SAR,  ///< (signed) a >> b
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

protected:
	const SpriteGroup *Resolve(ResolverObject *object) const;
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

protected:
	const SpriteGroup *Resolve(ResolverObject *object) const;
};


/* This contains a callback result. A failed callback has a value of
 * CALLBACK_FAILED */
struct CallbackResultSpriteGroup : SpriteGroup {
	/**
	 * Creates a spritegroup representing a callback result
	 * @param value The value that was used to represent this callback result
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

/**
 * Action 2 sprite layout for houses, industry tiles, objects and airport tiles.
 */
struct TileLayoutSpriteGroup : SpriteGroup {
	TileLayoutSpriteGroup() : SpriteGroup(SGT_TILELAYOUT) {}
	~TileLayoutSpriteGroup() {}

	byte num_building_stages;    ///< Number of building stages to show for this house/industry tile
	NewGRFSpriteLayout dts;

	const DrawTileSprites *ProcessRegisters(uint8 *stage) const;
};

struct IndustryProductionSpriteGroup : SpriteGroup {
	IndustryProductionSpriteGroup() : SpriteGroup(SGT_INDUSTRY_PRODUCTION) {}

	uint8 version;
	int16 subtract_input[3];  // signed
	uint16 add_output[2];     // unsigned
	uint8 again;
};


struct ResolverObject {
	CallbackID callback;
	uint32 callback_param1;
	uint32 callback_param2;

	byte trigger;

	uint32 last_value;          ///< Result of most recent DeterministicSpriteGroup (including procedure calls)
	uint32 reseed;              ///< Collects bits to rerandomise while triggering triggers.

	VarSpriteGroupScope scope;  ///< Scope of currently resolved DeterministicSpriteGroup resp. RandomizedSpriteGroup
	byte count;                 ///< Additional scope for RandomizedSpriteGroup

	const GRFFile *grffile;     ///< GRFFile the resolved SpriteGroup belongs to

	union {
		struct {
			const struct Vehicle *self;
			const struct Vehicle *parent;
			EngineID self_type;
			bool info_view;                ///< Indicates if the item is being drawn in an info window
		} vehicle;
		struct {
			TileIndex tile;
		} canal;
		struct {
			TileIndex tile;
			struct BaseStation *st;
			const struct StationSpec *statspec;
			CargoID cargo_type;
		} station;
		struct {
			TileIndex tile;
			Town *town;                    ///< Town of this house
			HouseID house_id;
			uint16 initial_random_bits;    ///< Random bits during construction checks
			bool not_yet_constructed;      ///< True for construction check
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
			uint8 src_industry;            ///< Source industry substitute type. 0xFF for "town", 0xFE for "unknown".
			uint8 dst_industry;            ///< Destination industry substitute type. 0xFF for "town", 0xFE for "unknown".
			uint8 distance;
			AIConstructionEvent event;
			uint8 count;
			uint8 station_size;
		} generic;
		struct {
			TileIndex tile;                ///< Tracktile. For track on a bridge this is the southern bridgehead.
			TileContext context;           ///< Are we resolving sprites for the upper halftile, or on a bridge?
		} routes;
		struct {
			struct Station *st;            ///< Station of the airport for which the callback is run, or NULL for build gui.
			byte airport_id;               ///< Type of airport for which the callback is run
			byte layout;                   ///< Layout of the airport to build.
			TileIndex tile;                ///< Tile for the callback, only valid for airporttile callbacks.
		} airport;
		struct {
			struct Object *o;              ///< The object the callback is ran for.
			TileIndex tile;                ///< The tile related to the object.
			uint8 view;                    ///< The view of the object.
		} object;
	} u;

	uint32 (*GetRandomBits)(const struct ResolverObject*);
	uint32 (*GetTriggers)(const struct ResolverObject*);
	void (*SetTriggers)(const struct ResolverObject*, int);
	uint32 (*GetVariable)(const struct ResolverObject*, byte, byte, bool*);
	const SpriteGroup *(*ResolveReal)(const struct ResolverObject*, const RealSpriteGroup*);
	void (*StorePSA)(struct ResolverObject*, uint, int32);
};

#endif /* NEWGRF_SPRITEGROUP_H */
