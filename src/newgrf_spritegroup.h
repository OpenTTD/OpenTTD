/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_spritegroup.h Action 2 handling. */

#ifndef NEWGRF_SPRITEGROUP_H
#define NEWGRF_SPRITEGROUP_H

#include "core/pool_type.hpp"
#include "town_type.h"
#include "engine_type.h"
#include "house_type.h"
#include "industry_type.h"

#include "newgrf_callbacks.h"
#include "newgrf_generic.h"
#include "newgrf_storage.h"
#include "newgrf_commons.h"

struct SpriteGroup;
struct ResultSpriteGroup;
struct TileLayoutSpriteGroup;
struct IndustryProductionSpriteGroup;
struct ResolverObject;
using CallbackResult = uint16_t;

/**
 * Result of resolving sprite groups:
 * - std::monostate: Failure.
 * - CallbackResult: Callback result.
 * - SpriteGroup: ResultSpriteGroup, TileLayoutSpriteGroup, IndustryProductionSpriteGroup
 */
using ResolverResult = std::variant<std::monostate, CallbackResult, const ResultSpriteGroup *, const TileLayoutSpriteGroup *, const IndustryProductionSpriteGroup *>;

/* SPRITE_WIDTH is 24. ECS has roughly 30 sprite groups per real sprite.
 * Adding an 'extra' margin would be assuming 64 sprite groups per real
 * sprite. 64 = 2^6, so 2^30 should be enough (for now) */
using SpriteGroupID = PoolID<uint32_t, struct SpriteGroupIDTag, 1U << 30, 0xFFFFFFFF>;
using SpriteGroupPool = Pool<SpriteGroup, SpriteGroupID, 1024, PoolType::Data>;
extern SpriteGroupPool _spritegroup_pool;

/* Common wrapper for all the different sprite group types */
struct SpriteGroup : SpriteGroupPool::PoolItem<&_spritegroup_pool> {
protected:
	SpriteGroup() {} // Not `= default` as that resets PoolItem->index.
	/** Base sprite group resolver */
	virtual ResolverResult Resolve(ResolverObject &object) const = 0;

public:
	virtual ~SpriteGroup() = default;

	uint32_t nfo_line = 0;

	static ResolverResult Resolve(const SpriteGroup *group, ResolverObject &object, bool top_level = true);
};


/* 'Real' sprite groups contain a list of other result or callback sprite
 * groups. */
struct RealSpriteGroup : SpriteGroup {
	RealSpriteGroup() : SpriteGroup() {}

	/* Loaded = in motion, loading = not moving
	 * Each group contains several spritesets, for various loading stages */

	/* XXX: For stations the meaning is different - loaded is for stations
	 * with small amount of cargo whilst loading is for stations with a lot
	 * of da stuff. */

	std::vector<const SpriteGroup *> loaded{};  ///< List of loaded groups (can be SpriteIDs or Callback results)
	std::vector<const SpriteGroup *> loading{}; ///< List of loading groups (can be SpriteIDs or Callback results)

protected:
	ResolverResult Resolve(ResolverObject &object) const override;
};

/* Shared by deterministic and random groups. */
enum VarSpriteGroupScope : uint8_t {
	VSG_BEGIN,

	VSG_SCOPE_SELF = VSG_BEGIN, ///< Resolved object itself
	VSG_SCOPE_PARENT,           ///< Related object of the resolved one
	VSG_SCOPE_RELATIVE,         ///< Relative position (vehicles only)

	VSG_END
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(VarSpriteGroupScope)

enum DeterministicSpriteGroupSize : uint8_t {
	DSG_SIZE_BYTE,
	DSG_SIZE_WORD,
	DSG_SIZE_DWORD,
};

enum DeterministicSpriteGroupAdjustType : uint8_t {
	DSGA_TYPE_NONE,
	DSGA_TYPE_DIV,
	DSGA_TYPE_MOD,
};

enum DeterministicSpriteGroupAdjustOperation : uint8_t {
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
	DSGA_OP_SCMP, ///< (signed) comparison (a < b -> 0, a == b = 1, a > b = 2)
	DSGA_OP_UCMP, ///< (unsigned) comparison (a < b -> 0, a == b = 1, a > b = 2)
	DSGA_OP_SHL,  ///< a << b
	DSGA_OP_SHR,  ///< (unsigned) a >> b
	DSGA_OP_SAR,  ///< (signed) a >> b
};


struct DeterministicSpriteGroupAdjust {
	DeterministicSpriteGroupAdjustOperation operation{};
	DeterministicSpriteGroupAdjustType type{};
	uint8_t variable = 0;
	uint8_t parameter = 0; ///< Used for variables between 0x60 and 0x7F inclusive.
	uint8_t shift_num = 0;
	uint32_t and_mask = 0;
	uint32_t add_val = 0;
	uint32_t divmod_val = 0;
	const SpriteGroup *subroutine = nullptr;
};


struct DeterministicSpriteGroupResult {
	bool calculated_result = false;
	const SpriteGroup *group = nullptr;

	bool operator==(const DeterministicSpriteGroupResult &) const = default;
};

struct DeterministicSpriteGroupRange {
	DeterministicSpriteGroupResult result;
	uint32_t low = 0;
	uint32_t high = 0;
};


struct DeterministicSpriteGroup : SpriteGroup {
	DeterministicSpriteGroup() : SpriteGroup() {}

	VarSpriteGroupScope var_scope{};
	DeterministicSpriteGroupSize size{};
	std::vector<DeterministicSpriteGroupAdjust> adjusts{};
	std::vector<DeterministicSpriteGroupRange> ranges{}; // Dynamically allocated

	/* Dynamically allocated, this is the sole owner */
	DeterministicSpriteGroupResult default_result;

	const SpriteGroup *error_group = nullptr; // was first range, before sorting ranges

protected:
	ResolverResult Resolve(ResolverObject &object) const override;
};

enum RandomizedSpriteGroupCompareMode : uint8_t {
	RSG_CMP_ANY,
	RSG_CMP_ALL,
};

struct RandomizedSpriteGroup : SpriteGroup {
	RandomizedSpriteGroup() : SpriteGroup() {}

	VarSpriteGroupScope var_scope{};  ///< Take this object:

	RandomizedSpriteGroupCompareMode cmp_mode{}; ///< Check for these triggers:
	uint8_t triggers = 0;
	uint8_t count = 0;

	uint8_t lowest_randbit = 0; ///< Look for this in the per-object randomized bitmask:

	std::vector<const SpriteGroup *> groups{}; ///< Take the group with appropriate index:

protected:
	ResolverResult Resolve(ResolverObject &object) const override;
};


/* This contains a callback result. A failed callback has a value of
 * CALLBACK_FAILED */
struct CallbackResultSpriteGroup : SpriteGroup {
	/**
	 * Creates a spritegroup representing a callback result
	 * @param value The value that was used to represent this callback result
	 */
	explicit CallbackResultSpriteGroup(CallbackResult value) : SpriteGroup(), result(value) {}

	CallbackResult result = 0;

protected:
	ResolverResult Resolve(ResolverObject &object) const override;
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
	ResultSpriteGroup(SpriteID sprite, uint8_t num_sprites) : SpriteGroup(), num_sprites(num_sprites), sprite(sprite) {}

	uint8_t num_sprites = 0;
	SpriteID sprite = 0;

protected:
	ResolverResult Resolve(ResolverObject &) const override { return this; }
};

/**
 * Action 2 sprite layout for houses, industry tiles, objects and airport tiles.
 */
struct TileLayoutSpriteGroup : SpriteGroup {
	TileLayoutSpriteGroup() : SpriteGroup() {}

	NewGRFSpriteLayout dts{};

	SpriteLayoutProcessor ProcessRegisters(const ResolverObject &object, uint8_t *stage) const;

protected:
	ResolverResult Resolve(ResolverObject &) const override { return this; }
};

struct IndustryProductionSpriteGroup : SpriteGroup {
	IndustryProductionSpriteGroup() : SpriteGroup() {}

	uint8_t version = 0; ///< Production callback version used, or 0xFF if marked invalid
	uint8_t num_input = 0; ///< How many subtract_input values are valid
	std::array<int16_t, INDUSTRY_NUM_INPUTS> subtract_input{}; ///< Take this much of the input cargo (can be negative, is indirect in cb version 1+)
	std::array<CargoType, INDUSTRY_NUM_INPUTS> cargo_input{}; ///< Which input cargoes to take from (only cb version 2)
	uint8_t num_output = 0; ///< How many add_output values are valid
	std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> add_output{}; ///< Add this much output cargo when successful (unsigned, is indirect in cb version 1+)
	std::array<CargoType, INDUSTRY_NUM_OUTPUTS> cargo_output{}; ///< Which output cargoes to add to (only cb version 2)
	uint8_t again = 0;

protected:
	ResolverResult Resolve(ResolverObject &) const override { return this; }
};

/**
 * Interface to query and set values specific to a single #VarSpriteGroupScope (action 2 scope).
 *
 * Multiple of these interfaces are combined into a #ResolverObject to allow access
 * to different game entities from a #SpriteGroup-chain (action 1-2-3 chain).
 */
struct ScopeResolver {
	ResolverObject &ro; ///< Surrounding resolver object.

	ScopeResolver(ResolverObject &ro) : ro(ro) {}
	virtual ~ScopeResolver() = default;

	virtual uint32_t GetRandomBits() const;
	virtual uint32_t GetRandomTriggers() const;

	virtual uint32_t GetVariable(uint8_t variable, [[maybe_unused]] uint32_t parameter, bool &available) const;
	virtual void StorePSA(uint reg, int32_t value);
};

/**
 * Interface for #SpriteGroup-s to access the gamestate.
 *
 * Using this interface #SpriteGroup-chains (action 1-2-3 chains) can be resolved,
 * to get the results of callbacks, rerandomisations or normal sprite lookups.
 */
struct ResolverObject {
private:
	static TemporaryStorageArray<int32_t, 0x110> temp_store;

public:
	/**
	 * Resolver constructor.
	 * @param grffile NewGRF file associated with the object (or \c nullptr if none).
	 * @param callback Callback code being resolved (default value is #CBID_NO_CALLBACK).
	 * @param callback_param1 First parameter (var 10) of the callback (only used when \a callback is also set).
	 * @param callback_param2 Second parameter (var 18) of the callback (only used when \a callback is also set).
	 */
	ResolverObject(const GRFFile *grffile, CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0)
		: default_scope(*this), callback(callback), callback_param1(callback_param1), callback_param2(callback_param2), grffile(grffile), root_spritegroup(nullptr)
	{
	}

	virtual ~ResolverObject() = default;

	ResolverResult DoResolve()
	{
		temp_store.ClearChanges();
		this->last_value = 0;
		this->used_random_triggers = 0;
		this->reseed.fill(0);
		return SpriteGroup::Resolve(this->root_spritegroup, *this);
	}

	ScopeResolver default_scope; ///< Default implementation of the grf scope.

	/**
	 * Gets the value of a so-called newgrf "register".
	 * @param i index of the register
	 * @return the value of the register
	 * @pre i < 0x110
	 */
	inline int32_t GetRegister(uint i) const
	{
		return temp_store.GetValue(i);
	}

	/**
	 * Sets the value of a so-called newgrf "register".
	 * @param i index of the register
	 * @param value the value of the register
	 * @pre i < 0x110
	 */
	inline void SetRegister(uint i, int32_t value)
	{
		temp_store.StoreValue(i, value);
	}

	CallbackID callback{}; ///< Callback being resolved.
	uint32_t callback_param1 = 0; ///< First parameter (var 10) of the callback.
	uint32_t callback_param2 = 0; ///< Second parameter (var 18) of the callback.

	uint32_t last_value = 0; ///< Result of most recent DeterministicSpriteGroup (including procedure calls)

protected:
	uint32_t waiting_random_triggers = 0; ///< Waiting triggers to be used by any rerandomisation. (scope independent)
	uint32_t used_random_triggers = 0; ///< Subset of cur_triggers, which actually triggered some rerandomisation. (scope independent)
public:
	std::array<uint32_t, VSG_END> reseed; ///< Collects bits to rerandomise while triggering triggers.

	const GRFFile *grffile = nullptr; ///< GRFFile the resolved SpriteGroup belongs to
	const SpriteGroup *root_spritegroup = nullptr; ///< Root SpriteGroup to use for resolving

	/**
	 * Resolve SpriteGroup.
	 * @return Result spritegroup.
	 * @tparam TSpriteGroup Sprite group type
	 */
	template <class TSpriteGroup>
	inline const TSpriteGroup *Resolve()
	{
		auto result = this->DoResolve();
		const auto *group = std::get_if<const TSpriteGroup *>(&result);
		if (group == nullptr) return nullptr;
		return *group;
	}

	/**
	 * Resolve bits to be rerandomised.
	 * Access results via:
	 * - reseed: Bits to rerandomise per scope, for features with proper PARENT rerandomisation. (only industry tiles)
	 * - GetReseedSum: Bits to rerandomise for SELF scope, for features with broken-by-design PARENT randomisation. (all but industry tiles)
	 * - GetUsedRandomTriggers: Consumed random triggers to be reset.
	 */
	inline void ResolveRerandomisation()
	{
		/* The Resolve result has no meaning.
		 * It can be a SpriteSet, a callback result, or even an invalid SpriteGroup reference (nullptr). */
		this->DoResolve();
	}

	/**
	 * Resolve callback.
	 * @param[out] regs100 Additional result values from registers 100+
	 * @return Callback result.
	 */
	inline CallbackResult ResolveCallback(std::span<int32_t> regs100)
	{
		auto result = this->DoResolve();
		const auto *value = std::get_if<CallbackResult>(&result);
		if (value == nullptr) return CALLBACK_FAILED;
		for (uint i = 0; i < regs100.size(); ++i) {
			regs100[i] = this->GetRegister(0x100 + i);
		}
		return *value;
	}

	virtual const SpriteGroup *ResolveReal(const RealSpriteGroup &group) const;

	virtual ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, uint8_t relative = 0);

	/**
	 * Used by RandomizedSpriteGroup: Triggers for rerandomisation
	 */
	uint32_t GetWaitingRandomTriggers() const
	{
		return this->waiting_random_triggers;
	}

	/**
	 * Used by RandomizedSpriteGroup: Consume triggers.
	 */
	void AddUsedRandomTriggers(uint32_t triggers)
	{
		this->used_random_triggers |= triggers;
	}

	/**
	 * Returns the OR-sum of all bits that need reseeding
	 * independent of the scope they were accessed with.
	 * @return OR-sum of the bits.
	 */
	uint32_t GetReseedSum() const
	{
		uint32_t sum = 0;
		for (VarSpriteGroupScope vsg = VSG_BEGIN; vsg < VSG_END; vsg++) {
			sum |= this->reseed[vsg];
		}
		return sum;
	}

	/**
	 * Get the feature number being resolved for.
	 * This function is mainly intended for the callback profiling feature.
	 */
	virtual GrfSpecFeature GetFeature() const { return GSF_INVALID; }
	/**
	 * Get an identifier for the item being resolved.
	 * This function is mainly intended for the callback profiling feature,
	 * and should return an identifier recognisable by the NewGRF developer.
	 */
	virtual uint32_t GetDebugID() const { return 0; }
};

/**
 * Specialization of ResolverObject with type-safe access to RandomTriggers.
 */
template <class RandomTriggers>
struct SpecializedResolverObject : public ResolverObject {
	using ResolverObject::ResolverObject;

	/**
	 * Set waiting triggers for rerandomisation.
	 * This is scope independent, even though this is broken-by-design in most cases.
	 */
	void SetWaitingRandomTriggers(RandomTriggers triggers)
	{
		this->waiting_random_triggers = triggers.base();
	}

	/**
	 * Get the triggers, which were "consumed" by some rerandomisation.
	 * This is scope independent, even though this is broken-by-design in most cases.
	 */
	RandomTriggers GetUsedRandomTriggers() const
	{
		return static_cast<RandomTriggers>(this->used_random_triggers);
	}
};

#endif /* NEWGRF_SPRITEGROUP_H */
