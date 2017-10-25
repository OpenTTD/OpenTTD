/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_spritegroup.cpp Handling of primarily NewGRF action 2. */

#include "stdafx.h"
#include "debug.h"
#include "newgrf_spritegroup.h"
#include "core/pool_func.hpp"

#include "safeguards.h"

SpriteGroupPool _spritegroup_pool("SpriteGroup");
INSTANTIATE_POOL_METHODS(SpriteGroup)

TemporaryStorageArray<int32, 0x110> _temp_store;


/**
 * ResolverObject (re)entry point.
 * This cannot be made a call to a virtual function because virtual functions
 * do not like NULL and checking for NULL *everywhere* is more cumbersome than
 * this little helper function.
 * @param group the group to resolve for
 * @param object information needed to resolve the group
 * @param top_level true if this is a top-level SpriteGroup, false if used nested in another SpriteGroup.
 * @return the resolved group
 */
/* static */ const SpriteGroup *SpriteGroup::Resolve(const SpriteGroup *group, ResolverObject &object, bool top_level)
{
	if (group == NULL) return NULL;
	if (top_level) {
		_temp_store.ClearChanges();
	}
	return group->Resolve(object);
}

RealSpriteGroup::~RealSpriteGroup()
{
	free(this->loaded);
	free(this->loading);
}

DeterministicSpriteGroup::~DeterministicSpriteGroup()
{
	free(this->adjusts);
	free(this->ranges);
}

RandomizedSpriteGroup::~RandomizedSpriteGroup()
{
	free(this->groups);
}

static inline uint32 GetVariable(const ResolverObject &object, ScopeResolver *scope, byte variable, uint32 parameter, bool *available)
{
	/* First handle variables common with Action7/9/D */
	uint32 value;
	if (GetGlobalVariable(variable, &value, object.grffile)) return value;

	/* Non-common variable */
	switch (variable) {
		case 0x0C: return object.callback;
		case 0x10: return object.callback_param1;
		case 0x18: return object.callback_param2;
		case 0x1C: return object.last_value;

		case 0x5F: return (scope->GetRandomBits() << 8) | scope->GetTriggers();

		case 0x7D: return _temp_store.GetValue(parameter);

		case 0x7F:
			if (object.grffile == NULL) return 0;
			return object.grffile->GetParam(parameter);

		/* Not a common variable, so evaluate the feature specific variables */
		default: return scope->GetVariable(variable, parameter, available);
	}
}

ScopeResolver::ScopeResolver(ResolverObject &ro)
		: ro(ro)
{
}

ScopeResolver::~ScopeResolver() {}

/**
 * Get a few random bits. Default implementation has no random bits.
 * @return Random bits.
 */
/* virtual */ uint32 ScopeResolver::GetRandomBits() const
{
	return 0;
}

/**
 * Get the triggers. Base class returns \c 0 to prevent trouble.
 * @return The triggers.
 */
/* virtual */ uint32 ScopeResolver::GetTriggers() const
{
	return 0;
}

/**
 * Get a variable value. Default implementation has no available variables.
 * @param variable Variable to read
 * @param parameter Parameter for 60+x variables
 * @param[out] available Set to false, in case the variable does not exist.
 * @return Value
 */
/* virtual */ uint32 ScopeResolver::GetVariable(byte variable, uint32 parameter, bool *available) const
{
	DEBUG(grf, 1, "Unhandled scope variable 0x%X", variable);
	*available = false;
	return UINT_MAX;
}

/**
 * Store a value into the persistent storage area (PSA). Default implementation does nothing (for newgrf classes without storage).
 * @param pos Position to store into.
 * @param value Value to store.
 */
/* virtual */ void ScopeResolver::StorePSA(uint reg, int32 value) {}

/**
 * Resolver constructor.
 * @param grffile NewGRF file associated with the object (or \c NULL if none).
 * @param callback Callback code being resolved (default value is #CBID_NO_CALLBACK).
 * @param callback_param1 First parameter (var 10) of the callback (only used when \a callback is also set).
 * @param callback_param2 Second parameter (var 18) of the callback (only used when \a callback is also set).
 */
ResolverObject::ResolverObject(const GRFFile *grffile, CallbackID callback, uint32 callback_param1, uint32 callback_param2)
		: default_scope(*this)
{
	this->callback = callback;
	this->callback_param1 = callback_param1;
	this->callback_param2 = callback_param2;
	this->ResetState();

	this->grffile = grffile;
	this->root_spritegroup = NULL;
}

ResolverObject::~ResolverObject() {}

/**
 * Get the real sprites of the grf.
 * @param group Group to get.
 * @return The available sprite group.
 */
/* virtual */ const SpriteGroup *ResolverObject::ResolveReal(const RealSpriteGroup *group) const
{
	return NULL;
}

/**
 * Get a resolver for the \a scope.
 * @param scope Scope to return.
 * @param relative Additional parameter for #VSG_SCOPE_RELATIVE.
 * @return The resolver for the requested scope.
 */
/* virtual */ ScopeResolver *ResolverObject::GetScope(VarSpriteGroupScope scope, byte relative)
{
	return &this->default_scope;
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
static U EvalAdjustT(const DeterministicSpriteGroupAdjust *adjust, ScopeResolver *scope, U last_value, uint32 value)
{
	value >>= adjust->shift_num;
	value  &= adjust->and_mask;

	if (adjust->type != DSGA_TYPE_NONE) value += (S)adjust->add_val;

	switch (adjust->type) {
		case DSGA_TYPE_DIV:  value = (S)value / (S)adjust->divmod_val; break;
		case DSGA_TYPE_MOD:  value = (S)value % (S)adjust->divmod_val; break;
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
		case DSGA_OP_STO:  _temp_store.StoreValue((U)value, (S)last_value); return last_value;
		case DSGA_OP_RST:  return value;
		case DSGA_OP_STOP: scope->StorePSA((U)value, (S)last_value); return last_value;
		case DSGA_OP_ROR:  return RotateRight(last_value, value);
		case DSGA_OP_SCMP: return ((S)last_value == (S)value) ? 1 : ((S)last_value < (S)value ? 0 : 2);
		case DSGA_OP_UCMP: return ((U)last_value == (U)value) ? 1 : ((U)last_value < (U)value ? 0 : 2);
		case DSGA_OP_SHL:  return (uint32)(U)last_value << ((U)value & 0x1F); // Same behaviour as in ParamSet, mask 'value' to 5 bits, which should behave the same on all architectures.
		case DSGA_OP_SHR:  return (uint32)(U)last_value >> ((U)value & 0x1F);
		case DSGA_OP_SAR:  return (int32)(S)last_value >> ((U)value & 0x1F);
		default:           return value;
	}
}


const SpriteGroup *DeterministicSpriteGroup::Resolve(ResolverObject &object) const
{
	uint32 last_value = 0;
	uint32 value = 0;
	uint i;

	ScopeResolver *scope = object.GetScope(this->var_scope);

	for (i = 0; i < this->num_adjusts; i++) {
		DeterministicSpriteGroupAdjust *adjust = &this->adjusts[i];

		/* Try to get the variable. We shall assume it is available, unless told otherwise. */
		bool available = true;
		if (adjust->variable == 0x7E) {
			const SpriteGroup *subgroup = SpriteGroup::Resolve(adjust->subroutine, object, false);
			if (subgroup == NULL) {
				value = CALLBACK_FAILED;
			} else {
				value = subgroup->GetCallbackResult();
			}

			/* Note: 'last_value' and 'reseed' are shared between the main chain and the procedure */
		} else if (adjust->variable == 0x7B) {
			value = GetVariable(object, scope, adjust->parameter, last_value, &available);
		} else {
			value = GetVariable(object, scope, adjust->variable, adjust->parameter, &available);
		}

		if (!available) {
			/* Unsupported variable: skip further processing and return either
			 * the group from the first range or the default group. */
			return SpriteGroup::Resolve(this->num_ranges > 0 ? this->ranges[0].group : this->default_group, object, false);
		}

		switch (this->size) {
			case DSG_SIZE_BYTE:  value = EvalAdjustT<uint8,  int8> (adjust, scope, last_value, value); break;
			case DSG_SIZE_WORD:  value = EvalAdjustT<uint16, int16>(adjust, scope, last_value, value); break;
			case DSG_SIZE_DWORD: value = EvalAdjustT<uint32, int32>(adjust, scope, last_value, value); break;
			default: NOT_REACHED();
		}
		last_value = value;
	}

	object.last_value = last_value;

	if (this->num_ranges == 0) {
		/* nvar == 0 is a special case -- we turn our value into a callback result */
		if (value != CALLBACK_FAILED) value = GB(value, 0, 15);
		static CallbackResultSpriteGroup nvarzero(0, true);
		nvarzero.result = value;
		return &nvarzero;
	}

	for (i = 0; i < this->num_ranges; i++) {
		if (this->ranges[i].low <= value && value <= this->ranges[i].high) {
			return SpriteGroup::Resolve(this->ranges[i].group, object, false);
		}
	}

	return SpriteGroup::Resolve(this->default_group, object, false);
}


const SpriteGroup *RandomizedSpriteGroup::Resolve(ResolverObject &object) const
{
	ScopeResolver *scope = object.GetScope(this->var_scope, this->count);
	if (object.callback == CBID_RANDOM_TRIGGER) {
		/* Handle triggers */
		byte match = this->triggers & object.waiting_triggers;
		bool res = (this->cmp_mode == RSG_CMP_ANY) ? (match != 0) : (match == this->triggers);

		if (res) {
			object.used_triggers |= match;
			object.reseed[this->var_scope] |= (this->num_groups - 1) << this->lowest_randbit;
		}
	}

	uint32 mask  = (this->num_groups - 1) << this->lowest_randbit;
	byte index = (scope->GetRandomBits() & mask) >> this->lowest_randbit;

	return SpriteGroup::Resolve(this->groups[index], object, false);
}


const SpriteGroup *RealSpriteGroup::Resolve(ResolverObject &object) const
{
	return object.ResolveReal(this);
}

/**
 * Process registers and the construction stage into the sprite layout.
 * The passed construction stage might get reset to zero, if it gets incorporated into the layout
 * during the preprocessing.
 * @param [in, out] stage Construction stage (0-3), or NULL if not applicable.
 * @return sprite layout to draw.
 */
const DrawTileSprites *TileLayoutSpriteGroup::ProcessRegisters(uint8 *stage) const
{
	if (!this->dts.NeedsPreprocessing()) {
		if (stage != NULL && this->dts.consistent_max_offset > 0) *stage = GetConstructionStageOffset(*stage, this->dts.consistent_max_offset);
		return &this->dts;
	}

	static DrawTileSprites result;
	uint8 actual_stage = stage != NULL ? *stage : 0;
	this->dts.PrepareLayout(0, 0, 0, actual_stage, false);
	this->dts.ProcessRegisters(0, 0, false);
	result.seq = this->dts.GetLayout(&result.ground);

	/* Stage has been processed by PrepareLayout(), set it to zero. */
	if (stage != NULL) *stage = 0;

	return &result;
}
