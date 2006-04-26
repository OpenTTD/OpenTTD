/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "sprite.h"
#include "variables.h"
#include "debug.h"


SpriteGroup *EvalDeterministicSpriteGroup(const DeterministicSpriteGroup *dsg, int value)
{
	int i;

	value >>= dsg->adjusts[0].shift_num; // This should bring us to the byte range.
	value &= dsg->adjusts[0].and_mask;

	if (dsg->adjusts[0].operation != DSGA_TYPE_NONE)
		value += (signed char) dsg->adjusts[0].add_val;

	switch (dsg->adjusts[0].type) {
		case DSGA_TYPE_DIV:
			value /= (signed char) dsg->adjusts[0].divmod_val;
			break;
		case DSGA_TYPE_MOD:
			value %= (signed char) dsg->adjusts[0].divmod_val;
			break;
		case DSGA_TYPE_NONE:
			break;
	}

	for (i = 0; i < dsg->num_ranges; i++) {
		DeterministicSpriteGroupRange *range = &dsg->ranges[i];

		if (range->low <= (uint32)value && (uint32)value <= range->high)
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
