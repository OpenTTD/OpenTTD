/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misc_sl_compat.h Loading for misc chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_MISC_H
#define SAVELOAD_COMPAT_MISC_H

#include "../saveload.h"

/** Original field order for _date_desc. */
const SaveLoadCompat _date_sl_compat[] = {
	SLC_VAR("date"),
	SLC_VAR("date_fract"),
	SLC_VAR("tick_counter"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_157),
	SLC_VAR("age_cargo_skip_counter"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_46),
	SLC_VAR("cur_tileloop_tile"),
	SLC_VAR("next_disaster_start"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_120),
	SLC_VAR("random_state[0]"),
	SLC_VAR("random_state[1]"),
	SLC_NULL(1, SL_MIN_VERSION,  SLV_10),
	SLC_NULL(4, SLV_10, SLV_120),
	SLC_VAR("company_tick_counter"),
	SLC_VAR("next_competitor_start"),
	SLC_VAR("trees_tick_counter"),
	SLC_VAR("pause_mode"),
	SLC_NULL(4, SLV_11, SLV_120),
};

/** Original field order for _date_check_desc. */
const SaveLoadCompat _date_check_sl_compat[] = {
	SLC_VAR("date"),
	SLC_NULL(2, SL_MIN_VERSION, SL_MAX_VERSION), // date_fract
	SLC_NULL(2, SL_MIN_VERSION, SL_MAX_VERSION), // tick_counter
	SLC_NULL(2, SL_MIN_VERSION, SLV_157),
	SLC_NULL(1, SL_MIN_VERSION, SLV_162),        // age_cargo_skip_counter
	SLC_NULL(1, SL_MIN_VERSION, SLV_46),
	SLC_NULL(2, SL_MIN_VERSION, SLV_6),          // cur_tileloop_tile
	SLC_NULL(4, SLV_6, SL_MAX_VERSION),          // cur_tileloop_tile
	SLC_NULL(2, SL_MIN_VERSION, SL_MAX_VERSION), // disaster_delay
	SLC_NULL(2, SL_MIN_VERSION, SLV_120),
	SLC_NULL(4, SL_MIN_VERSION, SL_MAX_VERSION), // random.state[0]
	SLC_NULL(4, SL_MIN_VERSION, SL_MAX_VERSION), // random.state[1]
	SLC_NULL(1,  SL_MIN_VERSION,  SLV_10),
	SLC_NULL(4, SLV_10, SLV_120),
	SLC_NULL(1, SL_MIN_VERSION, SL_MAX_VERSION), // cur_company_tick_index
	SLC_NULL(2, SL_MIN_VERSION, SLV_109),        // next_competitor_start
	SLC_NULL(4, SLV_109, SL_MAX_VERSION),        // next_competitor_start
	SLC_NULL(1, SL_MIN_VERSION, SL_MAX_VERSION), // trees_tick_ctr
	SLC_NULL(1, SLV_4, SL_MAX_VERSION),          // pause_mode
	SLC_NULL(4, SLV_11, SLV_120),
};

/** Original field order for _view_desc. */
const SaveLoadCompat _view_sl_compat[] = {
	SLC_VAR("x"),
	SLC_VAR("y"),
	SLC_VAR("zoom"),
};

#endif /* SAVELOAD_COMPAT_MISC_H */
