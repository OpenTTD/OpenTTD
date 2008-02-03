/* $Id$ */

/** @file newgrf_spritegroup.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "landscape.h"
#include "oldpool.h"
#include "newgrf_callbacks.h"
#include "newgrf_spritegroup.h"
#include "sprite.h"
#include "date_func.h"
#include "settings_type.h"

static void SpriteGroupPoolCleanBlock(uint start_item, uint end_item);

static uint _spritegroup_count = 0;
STATIC_OLD_POOL(SpriteGroup, SpriteGroup, 9, 250, NULL, SpriteGroupPoolCleanBlock)

static void DestroySpriteGroup(SpriteGroup *group)
{
	/* Free dynamically allocated memory */
	/* XXX Cast away the consts due to MSVC being buggy... */
	switch (group->type) {
		case SGT_REAL:
			free((SpriteGroup**)group->g.real.loaded);
			free((SpriteGroup**)group->g.real.loading);
			break;

		case SGT_DETERMINISTIC:
			free(group->g.determ.adjusts);
			free(group->g.determ.ranges);
			break;

		case SGT_RANDOMIZED:
			free((SpriteGroup**)group->g.random.groups);
			break;

		case SGT_TILELAYOUT:
			free((void*)group->g.layout.dts->seq);
			free(group->g.layout.dts);
			break;

		default:
			break;
	}
}

static void SpriteGroupPoolCleanBlock(uint start_item, uint end_item)
{
	uint i;

	for (i = start_item; i <= end_item; i++) {
		DestroySpriteGroup(GetSpriteGroup(i));
	}
}


/* Allocate a new SpriteGroup */
SpriteGroup *AllocateSpriteGroup()
{
	/* This is totally different to the other pool allocators, as we never remove an item from the pool. */
	if (_spritegroup_count == GetSpriteGroupPoolSize()) {
		if (!_SpriteGroup_pool.AddBlockToPool()) return NULL;
	}

	return GetSpriteGroup(_spritegroup_count++);
}


void InitializeSpriteGroupPool()
{
	_SpriteGroup_pool.CleanPool();

	_spritegroup_count = 0;
}

TemporaryStorageArray<uint32, 0x110> _temp_store;


static inline uint32 GetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	/* Return common variables */
	switch (variable) {
		case 0x00: return max(_date - DAYS_TILL_ORIGINAL_BASE_YEAR, 0);
		case 0x01: return Clamp(_cur_year, ORIGINAL_BASE_YEAR, ORIGINAL_MAX_YEAR) - ORIGINAL_BASE_YEAR;
		case 0x02: return _cur_month;
		case 0x03: return _opt.landscape;
		case 0x09: return _date_fract;
		case 0x0A: return _tick_counter;
		case 0x0C: return object->callback;
		case 0x10: return object->callback_param1;
		case 0x11: return 0;
		case 0x12: return _game_mode;
		case 0x18: return object->callback_param2;
		case 0x1A: return UINT_MAX;
		case 0x1B: return GB(_display_opt, 0, 6);
		case 0x1C: return object->last_value;
		case 0x20: return _opt.landscape == LT_ARCTIC ? GetSnowLine() : 0xFF;

		case 0x7D: return _temp_store.Get(parameter);

		/* Not a common variable, so evalute the feature specific variables */
		default: return object->GetVariable(object, variable, parameter, available);
	}
}


/**
 * Rotate val rot times to the right
 * @param val the value to rotate
 * @param rot the amount of times to rotate
 * @return the rotated value
 */
static uint32 RotateRight(uint32 val, uint32 rot)
{
	/* Do not rotate more than necessary */
	rot %= 32;

	return (val >> rot) | (val << (32 - rot));
}


/* Evaluate an adjustment for a variable of the given size.
* U is the unsigned type and S is the signed type to use. */
template <typename U, typename S>
static U EvalAdjustT(const DeterministicSpriteGroupAdjust *adjust, ResolverObject *object, U last_value, uint32 value)
{
	value >>= adjust->shift_num;
	value  &= adjust->and_mask;

	if (adjust->type != DSGA_TYPE_NONE) value += (S)adjust->add_val;

	switch (adjust->type) {
		case DSGA_TYPE_DIV:  value /= (S)adjust->divmod_val; break;
		case DSGA_TYPE_MOD:  value %= (U)adjust->divmod_val; break;
		case DSGA_TYPE_NONE: break;
	}

	switch (adjust->operation) {
		case DSGA_OP_ADD:  return last_value + value;
		case DSGA_OP_SUB:  return last_value - value;
		case DSGA_OP_SMIN: return min((S)last_value, (S)value);
		case DSGA_OP_SMAX: return max((S)last_value, (S)value);
		case DSGA_OP_UMIN: return min((U)last_value, (U)value);
		case DSGA_OP_UMAX: return max((U)last_value, (U)value);
		case DSGA_OP_SDIV: return value == 0 ? (S)last_value : (S)last_value / (S)value;
		case DSGA_OP_SMOD: return value == 0 ? (S)last_value : (S)last_value % (S)value;
		case DSGA_OP_UDIV: return value == 0 ? (U)last_value : (U)last_value / (U)value;
		case DSGA_OP_UMOD: return value == 0 ? (U)last_value : (U)last_value % (U)value;
		case DSGA_OP_MUL:  return last_value * value;
		case DSGA_OP_AND:  return last_value & value;
		case DSGA_OP_OR:   return last_value | value;
		case DSGA_OP_XOR:  return last_value ^ value;
		case DSGA_OP_STO:  _temp_store.Store(value, last_value); return last_value;
		case DSGA_OP_RST:  return value;
		case DSGA_OP_STOP: if (object->psa != NULL) object->psa->Store(value, last_value); return last_value;
		case DSGA_OP_ROR:  return RotateRight(last_value, value);
		case DSGA_OP_SCMP: return ((S)last_value == (S)value) ? 1 : ((S)last_value < (S)value ? 0 : 2);
		case DSGA_OP_UCMP: return ((U)last_value == (U)value) ? 1 : ((U)last_value < (U)value ? 0 : 2);
		default:           return value;
	}
}


static inline const SpriteGroup *ResolveVariable(const SpriteGroup *group, ResolverObject *object)
{
	static SpriteGroup nvarzero;
	uint32 last_value = 0;
	uint32 value = 0;
	uint i;

	object->scope = group->g.determ.var_scope;

	for (i = 0; i < group->g.determ.num_adjusts; i++) {
		DeterministicSpriteGroupAdjust *adjust = &group->g.determ.adjusts[i];

		/* Try to get the variable. We shall assume it is available, unless told otherwise. */
		bool available = true;
		if (adjust->variable == 0x7E) {
			ResolverObject subobject = *object;
			const SpriteGroup *subgroup = Resolve(adjust->subroutine, &subobject);
			if (subgroup == NULL || subgroup->type != SGT_CALLBACK) {
				value = CALLBACK_FAILED;
			} else {
				value = subgroup->g.callback.result;
			}
		} else {
			value = GetVariable(object, adjust->variable, adjust->parameter, &available);
		}

		if (!available) {
			/* Unsupported property: skip further processing and return either
			 * the group from the first range or the default group. */
			return Resolve(group->g.determ.num_ranges > 0 ? group->g.determ.ranges[0].group : group->g.determ.default_group, object);
		}

		switch (group->g.determ.size) {
			case DSG_SIZE_BYTE:  value = EvalAdjustT<uint8,  int8> (adjust, object, last_value, value); break;
			case DSG_SIZE_WORD:  value = EvalAdjustT<uint16, int16>(adjust, object, last_value, value); break;
			case DSG_SIZE_DWORD: value = EvalAdjustT<uint32, int32>(adjust, object, last_value, value); break;
			default: NOT_REACHED(); break;
		}
		last_value = value;
	}

	object->last_value = last_value;

	if (group->g.determ.num_ranges == 0) {
		/* nvar == 0 is a special case -- we turn our value into a callback result */
		nvarzero.type = SGT_CALLBACK;
		switch (object->callback) {
			/* All these functions are 15 bit callbacks */
			case CBID_VEHICLE_REFIT_CAPACITY:
			case CBID_HOUSE_COLOUR:
			case CBID_HOUSE_CARGO_ACCEPTANCE:
			case CBID_INDUSTRY_LOCATION:
			case CBID_INDTILE_CARGO_ACCEPTANCE:
			case CBID_VEHICLE_COLOUR_MAPPING:
			case CBID_HOUSE_PRODUCE_CARGO:
			case CBID_VEHICLE_SOUND_EFFECT:
			case CBID_SOUNDS_AMBIENT_EFFECT:
				nvarzero.g.callback.result = GB(value, 0, 15);
				break;

			/* The rest is a 8 bit callback, which should be truncated properly */
			default:
				nvarzero.g.callback.result = GB(value, 0, 8);
				break;
		}
		return &nvarzero;
	}

	for (i = 0; i < group->g.determ.num_ranges; i++) {
		if (group->g.determ.ranges[i].low <= value && value <= group->g.determ.ranges[i].high) {
			return Resolve(group->g.determ.ranges[i].group, object);
		}
	}

	return Resolve(group->g.determ.default_group, object);
}


static inline const SpriteGroup *ResolveRandom(const SpriteGroup *group, ResolverObject *object)
{
	uint32 mask;
	byte index;

	object->scope = group->g.random.var_scope;

	if (object->trigger != 0) {
		/* Handle triggers */
		/* Magic code that may or may not do the right things... */
		byte waiting_triggers = object->GetTriggers(object);
		byte match = group->g.random.triggers & (waiting_triggers | object->trigger);
		bool res;

		res = (group->g.random.cmp_mode == RSG_CMP_ANY) ?
			(match != 0) : (match == group->g.random.triggers);

		if (res) {
			waiting_triggers &= ~match;
			object->reseed |= (group->g.random.num_groups - 1) << group->g.random.lowest_randbit;
		} else {
			waiting_triggers |= object->trigger;
		}

		object->SetTriggers(object, waiting_triggers);
	}

	mask  = (group->g.random.num_groups - 1) << group->g.random.lowest_randbit;
	index = (object->GetRandomBits(object) & mask) >> group->g.random.lowest_randbit;

	return Resolve(group->g.random.groups[index], object);
}


/* ResolverObject (re)entry point */
const SpriteGroup *Resolve(const SpriteGroup *group, ResolverObject *object)
{
	/* We're called even if there is no group, so quietly return nothing */
	if (group == NULL) return NULL;

	switch (group->type) {
		case SGT_REAL:          return object->ResolveReal(object, group);
		case SGT_DETERMINISTIC: return ResolveVariable(group, object);
		case SGT_RANDOMIZED:    return ResolveRandom(group, object);
		default:                return group;
	}
}
