/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "macros.h"
#include "oldpool.h"
#include "newgrf_spritegroup.h"
#include "date.h"

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
SpriteGroup *AllocateSpriteGroup(void)
{
	/* This is totally different to the other pool allocators, as we never remove an item from the pool. */
	if (_spritegroup_count == GetSpriteGroupPoolSize()) {
		if (!AddBlockToPool(&_SpriteGroup_pool)) return NULL;
	}

	return GetSpriteGroup(_spritegroup_count++);
}


void InitializeSpriteGroupPool(void)
{
	CleanPool(&_SpriteGroup_pool);

	_spritegroup_count = 0;
}


static inline uint32 GetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	/* Return common variables */
	switch (variable) {
		case 0x00: return max(_date - DAYS_TILL_ORIGINAL_BASE_YEAR, 0);
		case 0x01: return clamp(_cur_year, ORIGINAL_BASE_YEAR, ORIGINAL_MAX_YEAR) - ORIGINAL_BASE_YEAR;
		case 0x02: return _cur_month;
		case 0x03: return _opt.landscape;
		case 0x09: return _date_fract;
		case 0x0A: return _tick_counter;
		case 0x0C: return object->callback;
		case 0x10: return object->callback_param1;
		case 0x11: return 0;
		case 0x18: return object->callback_param2;
		case 0x1A: return UINT_MAX;
		case 0x1B: return GB(_display_opt, 0, 6);
		case 0x1C: return object->last_value;
		case 0x20: return _opt.landscape == LT_HILLY ? _opt.snow_line : 0xFF;

		/* Not a common variable, so evalute the feature specific variables */
		default: return object->GetVariable(object, variable, parameter, available);
	}
}


/* Evaluate an adjustment for a variable of the given size.
* U is the unsigned type and S is the signed type to use. */
template <typename U, typename S>
static U EvalAdjustT(const DeterministicSpriteGroupAdjust *adjust, U last_value, int32 value)
{
	value >>= adjust->shift_num;
	value  &= adjust->and_mask;

	if (adjust->type != DSGA_TYPE_NONE) value += (S)adjust->add_val;

	switch (adjust->type) {
		case DSGA_TYPE_DIV:  value /= (S)adjust->divmod_val; break;
		case DSGA_TYPE_MOD:  value %= (U)adjust->divmod_val; break;
		case DSGA_TYPE_NONE: break;
	}

	/* Get our value to the correct range */
	value = (U)value;

	switch (adjust->operation) {
		case DSGA_OP_ADD:  return last_value + value;
		case DSGA_OP_SUB:  return last_value - value;
		case DSGA_OP_SMIN: return min((S)last_value, (S)value);
		case DSGA_OP_SMAX: return max((S)last_value, (S)value);
		case DSGA_OP_UMIN: return min((U)last_value, (U)value);
		case DSGA_OP_UMAX: return max((U)last_value, (U)value);
		case DSGA_OP_SDIV: return last_value / value;
		case DSGA_OP_SMOD: return last_value % value;
		case DSGA_OP_UDIV: return (U)last_value / (U)value;
		case DSGA_OP_UMOD: return (U)last_value % (U)value;
		case DSGA_OP_MUL:  return last_value * value;
		case DSGA_OP_AND:  return last_value & value;
		case DSGA_OP_OR:   return last_value | value;
		case DSGA_OP_XOR:  return last_value ^ value;
		default:           return value;
	}
}


static inline const SpriteGroup *ResolveVariable(const SpriteGroup *group, ResolverObject *object)
{
	static SpriteGroup nvarzero;
	int32 last_value = object->last_value;
	int32 value = -1;
	uint i;

	object->scope = group->g.determ.var_scope;

	for (i = 0; i < group->g.determ.num_adjusts; i++) {
		DeterministicSpriteGroupAdjust *adjust = &group->g.determ.adjusts[i];

		/* Try to get the variable. We shall assume it is available, unless told otherwise. */
		bool available = true;
		value = GetVariable(object, adjust->variable, adjust->parameter, &available);

		if (!available) {
			/* Unsupported property: skip further processing and return either
			 * the group from the first range or the default group. */
			return Resolve(group->g.determ.num_ranges > 0 ? group->g.determ.ranges[0].group : group->g.determ.default_group, object);
		}

		switch (group->g.determ.size) {
			case DSG_SIZE_BYTE:  value = EvalAdjustT<uint8, int8>(adjust, last_value, value); break;
			case DSG_SIZE_WORD:  value = EvalAdjustT<uint16, int16>(adjust, last_value, value); break;
			case DSG_SIZE_DWORD: value = EvalAdjustT<uint32, int32>(adjust, last_value, value); break;
			default: NOT_REACHED(); break;
		}
		last_value = value;
	}

	if (group->g.determ.num_ranges == 0) {
		/* nvar == 0 is a special case -- we turn our value into a callback result */
		nvarzero.type = SGT_CALLBACK;
		nvarzero.g.callback.result = GB(value, 0, 15);
		return &nvarzero;
	}

	for (i = 0; i < group->g.determ.num_ranges; i++) {
		if (group->g.determ.ranges[i].low <= (uint32)value && (uint32)value <= group->g.determ.ranges[i].high) {
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
