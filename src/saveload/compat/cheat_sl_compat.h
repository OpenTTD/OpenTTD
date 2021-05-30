/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cheat_sl_compat.h Loading for cheat chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_CHEAT_H
#define SAVELOAD_COMPAT_CHEAT_H

#include "../saveload.h"

/** Original field order for _cheats_desc. */
const SaveLoadCompat _cheats_sl_compat[] = {
	SLC_VAR("magic_bulldozer.been_used"),
	SLC_VAR("magic_bulldozer.value"),
	SLC_VAR("switch_company.been_used"),
	SLC_VAR("switch_company.value"),
	SLC_VAR("money.been_used"),
	SLC_VAR("money.value"),
	SLC_VAR("crossing_tunnels.been_used"),
	SLC_VAR("crossing_tunnels.value"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_TABLE_CHUNKS),
	SLC_NULL(1, SL_MIN_VERSION, SLV_TABLE_CHUNKS), // Need to be two NULL fields. See Load_CHTS().
	SLC_VAR("no_jetcrash.been_used"),
	SLC_VAR("no_jetcrash.value"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_TABLE_CHUNKS),
	SLC_NULL(1, SL_MIN_VERSION, SLV_TABLE_CHUNKS), // Need to be two NULL fields. See Load_CHTS().
	SLC_VAR("change_date.been_used"),
	SLC_VAR("change_date.value"),
	SLC_VAR("setup_prod.been_used"),
	SLC_VAR("setup_prod.value"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_TABLE_CHUNKS),
	SLC_NULL(1, SL_MIN_VERSION, SLV_TABLE_CHUNKS), // Need to be two NULL fields. See Load_CHTS().
	SLC_VAR("edit_max_hl.been_used"),
	SLC_VAR("edit_max_hl.value"),
};

#endif /* SAVELOAD_COMPAT_CHEAT_H */
