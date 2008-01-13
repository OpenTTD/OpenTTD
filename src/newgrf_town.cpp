/* $Id$ */

/** @file newgrf_town.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "settings_type.h"
#include "debug.h"
#include "core/bitmath_func.hpp"
#include "town.h"

/** This function implements the town variables that newGRF defines.
 * @param variable that is queried
 * @param parameter unused
 * @param available will return false if ever the variable asked for does not exist
 * @param t is of course the town we are inquiring
 * @return the value stored in the corresponding variable*/
uint32 TownGetVariable(byte variable, byte parameter, bool *available, const Town *t)
{
	switch (variable) {
		/* Larger towns */
		case 0x40:
			if (_patches.larger_towns == 0) return 2;
			if (t->larger_town) return 1;
			return 0;

		/* Town index */
		case 0x41: return t->index;

		/* Town properties */
		case 0x80: return t->xy;
		case 0x81: return GB(t->xy, 8, 8);
		case 0x82: return t->population;
		case 0x83: return GB(t->population, 8, 8);
		case 0x8A: return t->grow_counter;
		case 0x92: return t->flags12;  // In original game, 0x92 and 0x93 are really one word. Since flags12 is a byte, this is to adjust
		case 0x93: return 0;
		case 0x94: return t->radius[0];
		case 0x95: return GB(t->radius[0], 8, 8);
		case 0x96: return t->radius[1];
		case 0x97: return GB(t->radius[1], 8, 8);
		case 0x98: return t->radius[2];
		case 0x99: return GB(t->radius[2], 8, 8);
		case 0x9A: return t->radius[3];
		case 0x9B: return GB(t->radius[3], 8, 8);
		case 0x9C: return t->radius[4];
		case 0x9D: return GB(t->radius[4], 8, 8);
		case 0x9E: return t->ratings[0];
		case 0x9F: return t->ratings[1];
		case 0xA0: return t->ratings[2];
		case 0xA1: return t->ratings[3];
		case 0xA2: return t->ratings[4];
		case 0xA3: return t->ratings[5];
		case 0xA4: return t->ratings[6];
		case 0xA5: return t->ratings[7];
		case 0xAE: return t->have_ratings;
		case 0xB2: return t->statues;
		case 0xB6: return t->num_houses;
		case 0xB9: return t->growth_rate;
		case 0xBA: return t->new_max_pass;
		case 0xBB: return GB(t->new_max_pass, 8, 8);
		case 0xBC: return t->new_max_mail;
		case 0xBD: return GB(t->new_max_mail, 8, 8);
		case 0xBE: return t->new_act_pass;
		case 0xBF: return GB(t->new_act_pass, 8, 8);
		case 0xC0: return t->new_act_mail;
		case 0xC1: return GB(t->new_act_mail, 8, 8);
		case 0xC2: return t->max_pass;
		case 0xC3: return GB(t->max_pass, 8, 8);
		case 0xC4: return t->max_mail;
		case 0xC5: return GB(t->max_mail, 8, 8);
		case 0xC6: return t->act_pass;
		case 0xC7: return GB(t->act_pass, 8, 8);
		case 0xC8: return t->act_mail;
		case 0xC9: return GB(t->act_mail, 8, 8);
		case 0xCA: return t->pct_pass_transported;
		case 0xCB: return t->pct_mail_transported;
		case 0xCC: return t->new_act_food;
		case 0xCD: return GB(t->new_act_food, 8, 8);
		case 0xCE: return t->new_act_water;
		case 0xCF: return GB(t->new_act_water, 8, 8);
		case 0xD0: return t->act_food;
		case 0xD1: return GB(t->act_food, 8, 8);
		case 0xD2: return t->act_water;
		case 0xD3: return GB(t->act_water, 8, 8);
		case 0xD4: return t->road_build_months;
		case 0xD5: return t->fund_buildings_months;
	}

	DEBUG(grf, 1, "Unhandled town property 0x%X", variable);

	*available = false;
	return (uint32)-1;
}
