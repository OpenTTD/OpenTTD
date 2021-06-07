/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file town_sl_compat.h Loading of town chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_TOWN_H
#define SAVELOAD_COMPAT_TOWN_H

#include "../saveload.h"

/** Original field order for SlTownSupplied. */
const SaveLoadCompat _town_supplied_sl_compat[] = {
	SLC_VAR("old_max"),
	SLC_VAR("new_max"),
	SLC_VAR("old_act"),
	SLC_VAR("new_act"),
};

/** Original field order for SlTownReceived. */
const SaveLoadCompat _town_received_sl_compat[] = {
	SLC_VAR("old_max"),
	SLC_VAR("new_max"),
	SLC_VAR("old_act"),
	SLC_VAR("new_act"),
};

/** Original field order for SlTownAcceptanceMatrix. */
const SaveLoadCompat _town_acceptance_matrix_sl_compat[] = {
	SLC_VAR("area.tile"),
	SLC_VAR("area.w"),
	SLC_VAR("area.h"),
};

/** Original field order for town_desc. */
const SaveLoadCompat _town_sl_compat[] = {
	SLC_VAR("xy"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_3),
	SLC_NULL(4, SLV_3, SLV_85),
	SLC_NULL(2, SL_MIN_VERSION, SLV_92),
	SLC_VAR("townnamegrfid"),
	SLC_VAR("townnametype"),
	SLC_VAR("townnameparts"),
	SLC_VAR("name"),
	SLC_VAR("flags"),
	SLC_VAR("statues"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_2),
	SLC_VAR("have_ratings"),
	SLC_VAR("ratings"),
	SLC_VAR("unwanted"),
	SLC_VAR("supplied[CT_PASSENGERS].old_max"),
	SLC_VAR("supplied[CT_MAIL].old_max"),
	SLC_VAR("supplied[CT_PASSENGERS].new_max"),
	SLC_VAR("supplied[CT_MAIL].new_max"),
	SLC_VAR("supplied[CT_PASSENGERS].old_act"),
	SLC_VAR("supplied[CT_MAIL].old_act"),
	SLC_VAR("supplied[CT_PASSENGERS].new_act"),
	SLC_VAR("supplied[CT_MAIL].new_act"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_164),
	SLC_VAR("received[TE_FOOD].old_act"),
	SLC_VAR("received[TE_WATER].old_act"),
	SLC_VAR("received[TE_FOOD].new_act"),
	SLC_VAR("received[TE_WATER].new_act"),
	SLC_VAR("goal"),
	SLC_VAR("text"),
	SLC_VAR("time_until_rebuild"),
	SLC_VAR("grow_counter"),
	SLC_VAR("growth_rate"),
	SLC_VAR("fund_buildings_months"),
	SLC_VAR("road_build_months"),
	SLC_VAR("exclusivity"),
	SLC_VAR("exclusive_counter"),
	SLC_VAR("larger_town"),
	SLC_VAR("layout"),
	SLC_VAR("psa_list"),
	SLC_NULL(4, SLV_166, SLV_EXTEND_CARGOTYPES),
	SLC_NULL(8, SLV_EXTEND_CARGOTYPES, SLV_REMOVE_TOWN_CARGO_CACHE),
	SLC_NULL(30, SLV_2, SLV_REMOVE_TOWN_CARGO_CACHE),
	SLC_VAR("supplied"),
	SLC_VAR("received"),
	SLC_VAR("acceptance_matrix"),
};

#endif /* SAVELOAD_COMPAT_TOWN_H */
