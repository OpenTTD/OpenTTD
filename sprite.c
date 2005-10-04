/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "sprite.h"
#include "variables.h"
#include "debug.h"


SpriteGroup *EvalDeterministicSpriteGroup(const DeterministicSpriteGroup *dsg, int value)
{
	int i;

	value >>= dsg->shift_num; // This should bring us to the byte range.
	value &= dsg->and_mask;

	if (dsg->operation != DSG_OP_NONE)
		value += (signed char) dsg->add_val;

	switch (dsg->operation) {
		case DSG_OP_DIV:
			value /= (signed char) dsg->divmod_val;
			break;
		case DSG_OP_MOD:
			value %= (signed char) dsg->divmod_val;
			break;
		case DSG_OP_NONE:
			break;
	}

	for (i = 0; i < dsg->num_ranges; i++) {
		DeterministicSpriteGroupRange *range = &dsg->ranges[i];

		if (range->low <= value && value <= range->high)
			return range->group;
	}

	return dsg->default_group;
}

int GetDeterministicSpriteValue(byte var)
{
	switch (var) {
		case 0x00:
			return _date;
		case 0x01:
			return _cur_year;
		case 0x02:
			return _cur_month;
		case 0x03:
			return _opt.landscape;
		case 0x09:
			return _date_fract;
		case 0x0A:
			return _tick_counter;
		case 0x0C:
			/* If we got here, it means there was no callback or
			 * callbacks aren't supported on our callpath. */
			return 0;
		default:
			return -1;
	}
}

SpriteGroup *EvalRandomizedSpriteGroup(const RandomizedSpriteGroup *rsg, byte random_bits)
{
	byte mask;
	byte index;

	/* Noone likes mangling with bits, but you don't get around it here.
	 * Sorry. --pasky */
	// rsg->num_groups is always power of 2
	mask = (rsg->num_groups - 1) << rsg->lowest_randbit;
	index = (random_bits & mask) >> rsg->lowest_randbit;
	assert(index < rsg->num_groups);
	return rsg->groups[index];
}

byte RandomizedSpriteGroupTriggeredBits(const RandomizedSpriteGroup *rsg,
	byte triggers, byte *waiting_triggers)
{
	byte match = rsg->triggers & (*waiting_triggers | triggers);
	bool res;

	if (rsg->cmp_mode == RSG_CMP_ANY) {
		res = (match != 0);
	} else { /* RSG_CMP_ALL */
		res = (match == rsg->triggers);
	}

	if (!res) {
		*waiting_triggers |= triggers;
		return 0;
	}

	*waiting_triggers &= ~match;

	return (rsg->num_groups - 1) << rsg->lowest_randbit;
}

/**
 * Traverse a sprite group and release its and its child's memory.
 * A group is only released if its reference count is zero.
 * We pass a pointer to a pointer so that the original reference can be set to NULL.
 * @param group_ptr Pointer to sprite group reference.
 */
void UnloadSpriteGroup(SpriteGroup **group_ptr)
{
	SpriteGroup *group;
	int i;

	assert(group_ptr != NULL);
	assert(*group_ptr != NULL);

	group = *group_ptr;
	*group_ptr = NULL; // Remove this reference.

	group->ref_count--;
	if (group->ref_count > 0) {
		DEBUG(grf, 6)("UnloadSpriteGroup: Group at `%p' (type %d) has %d reference(s) left.", group, group->type, group->ref_count);
		return; // Still some references left, so don't clear up.
	}

	DEBUG(grf, 6)("UnloadSpriteGroup: Releasing group at `%p'.", group);
	switch (group->type) {
		case SGT_REAL:
		{
			RealSpriteGroup *rsg = &group->g.real;
			for (i = 0; i < rsg->loading_count; i++) {
				UnloadSpriteGroup(&rsg->loading[i]);
			}
			for (i = 0; i < rsg->loaded_count; i++) {
				UnloadSpriteGroup(&rsg->loaded[i]);
			}
			free(group);
			return;
		}

		case SGT_DETERMINISTIC:
		{
			DeterministicSpriteGroup *dsg = &group->g.determ;
			for (i = 0; i < group->g.determ.num_ranges; i++) {
				UnloadSpriteGroup(&dsg->ranges[i].group);
			}
			UnloadSpriteGroup(&dsg->default_group);
			free(group->g.determ.ranges);
			free(group);
			return;
		}

		case SGT_RANDOMIZED:
		{
			for (i = 0; i < group->g.random.num_groups; i++) {
				UnloadSpriteGroup(&group->g.random.groups[i]);
			}
			free(group->g.random.groups);
			free(group);
			return;
		}

		case SGT_CALLBACK:
		case SGT_RESULT:
			free(group);
			return;
	}

	DEBUG(grf, 1)("Unable to remove unknown sprite group type `0x%x'.", group->type);
}
