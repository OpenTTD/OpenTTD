/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_sl_compat.h Loading of order chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_ORDER_H
#define SAVELOAD_COMPAT_ORDER_H

#include "../saveload.h"

/** Original field order for _order_desc. */
const SaveLoadCompat _order_sl_compat[] = {
	SLC_VAR("type"),
	SLC_VAR("flags"),
	SLC_VAR("dest"),
	SLC_VAR("next"),
	SLC_VAR("refit_cargo"),
	SLC_NULL(1, SLV_36, SLV_182),
	SLC_VAR("wait_time"),
	SLC_VAR("travel_time"),
	SLC_VAR("max_speed"),
	SLC_NULL(10, SLV_5, SLV_36),
};

/** Original field order for _orderlist_desc. */
const SaveLoadCompat _orderlist_sl_compat[] = {
	SLC_VAR("first"),
};

/** Original field order for _order_backup_desc. */
const SaveLoadCompat _order_backup_sl_compat[] = {
	SLC_VAR("user"),
	SLC_VAR("tile"),
	SLC_VAR("group"),
	SLC_VAR("service_interval"),
	SLC_VAR("name"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_192),
	SLC_VAR("clone"),
	SLC_VAR("cur_real_order_index"),
	SLC_VAR("cur_implicit_order_index"),
	SLC_VAR("current_order_time"),
	SLC_VAR("lateness_counter"),
	SLC_VAR("timetable_start"),
	SLC_VAR("vehicle_flags"),
	SLC_VAR("orders"),
};

#endif /* SAVELOAD_COMPAT_ORDER_H */
