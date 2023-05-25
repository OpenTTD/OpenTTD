/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_sl_compat.h Loading of industry chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_INDUSTRY_H
#define SAVELOAD_COMPAT_INDUSTRY_H

#include "../saveload.h"

const SaveLoadCompat _industry_accepts_sl_compat[] = {
	SLC_VAR("cargo"),
	SLC_VAR("waiting"),
	SLC_VAR("last_accepted"),
};

const SaveLoadCompat _industry_produced_history_sl_compat[] = {
	SLC_VAR("production"),
	SLC_VAR("transported"),
};

const SaveLoadCompat _industry_produced_sl_compat[] = {
	SLC_VAR("cargo"),
	SLC_VAR("waiting"),
	SLC_VAR("rate"),
};

/** Original field order for _industry_desc. */
const SaveLoadCompat _industry_sl_compat[] = {
	SLC_VAR("location.tile"),
	SLC_VAR("location.w"),
	SLC_VAR("location.h"),
	SLC_VAR("town"),
	SLC_VAR("neutral_station"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_61),
	SLC_VAR("produced_cargo"),
	SLC_VAR("incoming_cargo_waiting"),
	SLC_VAR("produced_cargo_waiting"),
	SLC_VAR("production_rate"),
	SLC_NULL(3, SL_MIN_VERSION, SLV_61),
	SLC_VAR("accepts_cargo"),
	SLC_VAR("prod_level"),
	SLC_VAR("this_month_production"),
	SLC_VAR("this_month_transported"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLC_NULL(16, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),
	SLC_VAR("last_month_production"),
	SLC_VAR("last_month_transported"),
	SLC_VAR("counter"),
	SLC_VAR("type"),
	SLC_VAR("owner"),
	SLC_VAR("random_colour"),
	SLC_VAR("last_prod_year"),
	SLC_VAR("was_cargo_delivered"),
	SLC_VAR("ctlflags"),
	SLC_VAR("founder"),
	SLC_VAR("construction_date"),
	SLC_VAR("construction_type"),
	SLC_VAR("last_cargo_accepted_at[0]"),
	SLC_VAR("last_cargo_accepted_at"),
	SLC_VAR("selected_layout"),
	SLC_VAR("exclusive_supplier"),
	SLC_VAR("exclusive_consumer"),
	SLC_VAR("storage"),
	SLC_VAR("psa"),
	SLC_NULL(1, SLV_82, SLV_197),
	SLC_VAR("random"),
	SLC_VAR("text"),
	SLC_NULL(32, SLV_2, SLV_144),
};

/** Original field order for _industry_builder_desc. */
const SaveLoadCompat _industry_builder_sl_compat[] = {
	SLC_VAR("wanted_inds"),
};

/** Original field order for _industrytype_builder_desc. */
const SaveLoadCompat _industrytype_builder_sl_compat[] = {
	SLC_VAR("probability"),
	SLC_VAR("min_number"),
	SLC_VAR("target_count"),
	SLC_VAR("max_wait"),
	SLC_VAR("wait_count"),
};

#endif /* SAVELOAD_COMPAT_INDUSTRY_H */
