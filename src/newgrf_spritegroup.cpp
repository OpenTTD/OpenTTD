/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_spritegroup.cpp Handling of primarily NewGRF action 2. */

#include "stdafx.h"
#include "newgrf.h"
#include "newgrf_spritegroup.h"
#include "sprite.h"
#include "core/pool_func.hpp"

SpriteGroupPool _spritegroup_pool("SpriteGroup");
INSTANTIATE_POOL_METHODS(SpriteGroup)

RealSpriteGroup::~RealSpriteGroup()
{
	free((void*)this->loaded);
	free((void*)this->loading);
}

DeterministicSpriteGroup::~DeterministicSpriteGroup()
{
	free(this->adjusts);
	free(this->ranges);
}

RandomizedSpriteGroup::~RandomizedSpriteGroup()
{
	free((void*)this->groups);
}

TileLayoutSpriteGroup::~TileLayoutSpriteGroup()
{
	free((void*)this->dts->seq);
	free(this->dts);
}

TemporaryStorageArray<int32, 0x110> _temp_store;


static inline uint32 GetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	/* First handle variables common with Action7/9/D */
	uint32 value;
	if (GetGlobalVariable(variable, &value)) return value;

	/* Non-common variable */
	switch (variable) {
		case 0x0C: return object->callback;
		case 0x10: return object->callback_param1;
		case 0x18: return object->callback_param2;
		case 0x1C: return object->last_value;

		case 0x5F: return (object->GetRandomBits(object) << 8) | object->GetTriggers(object);

		case 0x7D: return _temp_store.Get(parameter);

		case 0x7F:
			if (object == NULL || object->grffile == NULL) return 0;
			return object->grffile->GetParam(parameter);

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
		case DSGA_OP_STO:  _temp_store.Store((U)value, (S)last_value); return last_value;
		case DSGA_OP_RST:  return value;
		case DSGA_OP_STOP: if (object->psa != NULL) object->psa->Store((U)value, (S)last_value); return last_value;
		case DSGA_OP_ROR:  return RotateRight(last_value, value);
		case DSGA_OP_SCMP: return ((S)last_value == (S)value) ? 1 : ((S)last_value < (S)value ? 0 : 2);
		case DSGA_OP_UCMP: return ((U)last_value == (U)value) ? 1 : ((U)last_value < (U)value ? 0 : 2);
		case DSGA_OP_SHL:  return (U)last_value << ((U)value & 0x1F); // mask 'value' to 5 bits, which should behave the same on all architectures.
		case DSGA_OP_SHR:  return (U)last_value >> ((U)value & 0x1F);
		case DSGA_OP_SAR:  return (S)last_value >> ((U)value & 0x1F);
		default:           return value;
	}
}


const SpriteGroup *DeterministicSpriteGroup::Resolve(ResolverObject *object) const
{
	uint32 last_value = 0;
	uint32 value = 0;
	uint i;

	object->scope = this->var_scope;

	for (i = 0; i < this->num_adjusts; i++) {
		DeterministicSpriteGroupAdjust *adjust = &this->adjusts[i];

		/* Try to get the variable. We shall assume it is available, unless told otherwise. */
		bool available = true;
		if (adjust->variable == 0x7E) {
			const SpriteGroup *subgroup = SpriteGroup::Resolve(adjust->subroutine, object);
			if (subgroup == NULL) {
				value = CALLBACK_FAILED;
			} else {
				value = subgroup->GetCallbackResult();
			}

			/* Reset values to current scope.
			 * Note: 'last_value' and 'reseed' are shared between the main chain and the procedure */
			object->scope = this->var_scope;
		} else if (adjust->variable == 0x7B) {
			value = GetVariable(object, adjust->parameter, last_value, &available);
		} else {
			value = GetVariable(object, adjust->variable, adjust->parameter, &available);
		}

		if (!available) {
			/* Unsupported variable: skip further processing and return either
			 * the group from the first range or the default group. */
			return SpriteGroup::Resolve(this->num_ranges > 0 ? this->ranges[0].group : this->default_group, object);
		}

		switch (this->size) {
			case DSG_SIZE_BYTE:  value = EvalAdjustT<uint8,  int8> (adjust, object, last_value, value); break;
			case DSG_SIZE_WORD:  value = EvalAdjustT<uint16, int16>(adjust, object, last_value, value); break;
			case DSG_SIZE_DWORD: value = EvalAdjustT<uint32, int32>(adjust, object, last_value, value); break;
			default: NOT_REACHED();
		}
		last_value = value;
	}

	object->last_value = last_value;

	if (this->num_ranges == 0) {
		/* nvar == 0 is a special case -- we turn our value into a callback result */
		if (value != CALLBACK_FAILED) value = GB(value, 0, 15);
		static CallbackResultSpriteGroup nvarzero(0);
		nvarzero.result = value;
		return &nvarzero;
	}

	for (i = 0; i < this->num_ranges; i++) {
		if (this->ranges[i].low <= value && value <= this->ranges[i].high) {
			return SpriteGroup::Resolve(this->ranges[i].group, object);
		}
	}

	return SpriteGroup::Resolve(this->default_group, object);
}


const SpriteGroup *RandomizedSpriteGroup::Resolve(ResolverObject *object) const
{
	uint32 mask;
	byte index;

	object->scope = this->var_scope;
	object->count = this->count;

	if (object->trigger != 0) {
		/* Handle triggers */
		/* Magic code that may or may not do the right things... */
		byte waiting_triggers = object->GetTriggers(object);
		byte match = this->triggers & (waiting_triggers | object->trigger);
		bool res = (this->cmp_mode == RSG_CMP_ANY) ? (match != 0) : (match == this->triggers);

		if (res) {
			waiting_triggers &= ~match;
			object->reseed |= (this->num_groups - 1) << this->lowest_randbit;
		} else {
			waiting_triggers |= object->trigger;
		}

		object->SetTriggers(object, waiting_triggers);
	}

	mask  = (this->num_groups - 1) << this->lowest_randbit;
	index = (object->GetRandomBits(object) & mask) >> this->lowest_randbit;

	return SpriteGroup::Resolve(this->groups[index], object);
}


const SpriteGroup *RealSpriteGroup::Resolve(ResolverObject *object) const
{
	return object->ResolveReal(object, this);
}
