/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_sl_compat.h Loading of vehicle chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_VEHICLE_H
#define SAVELOAD_COMPAT_VEHICLE_H

#include "../saveload.h"

/** Original field order for SlVehicleCommon. */
const SaveLoadCompat _vehicle_common_sl_compat[] = {
	SLC_VAR("subtype"),
	SLC_VAR("next"),
	SLC_VAR("name"),
	SLC_VAR("unitnumber"),
	SLC_VAR("owner"),
	SLC_VAR("tile"),
	SLC_VAR("dest_tile"),
	SLC_VAR("x_pos"),
	SLC_VAR("y_pos"),
	SLC_VAR("z_pos"),
	SLC_VAR("direction"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_58),
	SLC_VAR("spritenum"),
	SLC_NULL(5, SL_MIN_VERSION, SLV_58),
	SLC_VAR("engine_type"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_152),
	SLC_VAR("cur_speed"),
	SLC_VAR("subspeed"),
	SLC_VAR("acceleration"),
	SLC_VAR("motion_counter"),
	SLC_VAR("progress"),
	SLC_VAR("vehstatus"),
	SLC_VAR("last_station_visited"),
	SLC_VAR("last_loading_station"),
	SLC_VAR("cargo_type"),
	SLC_VAR("cargo_subtype"),
	SLC_VAR("cargo_days"),
	SLC_VAR("cargo_source"),
	SLC_VAR("cargo_source_xy"),
	SLC_VAR("cargo_cap"),
	SLC_VAR("refit_cap"),
	SLC_VAR("cargo_count"),
	SLC_VAR("cargo.packets"),
	SLC_VAR("cargo.action_counts"),
	SLC_VAR("cargo_age_counter"),
	SLC_VAR("day_counter"),
	SLC_VAR("tick_counter"),
	SLC_VAR("running_ticks"),
	SLC_VAR("cur_implicit_order_index"),
	SLC_VAR("cur_real_order_index"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_105),
	SLC_VAR("current_order.type"),
	SLC_VAR("current_order.flags"),
	SLC_VAR("current_order.dest"),
	SLC_VAR("current_order.refit_cargo"),
	SLC_NULL(1, SLV_36, SLV_182),
	SLC_VAR("current_order.wait_time"),
	SLC_VAR("current_order.travel_time"),
	SLC_VAR("current_order.max_speed"),
	SLC_VAR("timetable_start"),
	SLC_VAR("orders"),
	SLC_VAR("age"),
	SLC_VAR("max_age"),
	SLC_VAR("date_of_last_service"),
	SLC_VAR("service_interval"),
	SLC_VAR("reliability"),
	SLC_VAR("reliability_spd_dec"),
	SLC_VAR("breakdown_ctr"),
	SLC_VAR("breakdown_delay"),
	SLC_VAR("breakdowns_since_last_service"),
	SLC_VAR("breakdown_chance"),
	SLC_VAR("build_year"),
	SLC_VAR("load_unload_ticks"),
	SLC_VAR("cargo_paid_for"),
	SLC_VAR("vehicle_flags"),
	SLC_VAR("profit_this_year"),
	SLC_VAR("profit_last_year"),
	SLC_VAR("cargo_feeder_share"),
	SLC_NULL(4, SLV_51, SLV_68),
	SLC_VAR("value"),
	SLC_VAR("random_bits"),
	SLC_VAR("waiting_triggers"),
	SLC_VAR("next_shared"),
	SLC_NULL(2, SLV_2, SLV_69),
	SLC_NULL(4, SLV_69, SLV_101),
	SLC_VAR("group_id"),
	SLC_VAR("current_order_time"),
	SLC_VAR("lateness_counter"),
	SLC_NULL(10, SLV_2, SLV_144),
};

/** Original field order for SlVehicleTrain. */
const SaveLoadCompat _vehicle_train_sl_compat[] = {
	SLC_VAR("common"),
	SLC_VAR("crash_anim_pos"),
	SLC_VAR("force_proceed"),
	SLC_VAR("railtype"),
	SLC_VAR("track"),
	SLC_VAR("flags"),
	SLC_NULL(2, SLV_2, SLV_60),
	SLC_VAR("wait_counter"),
	SLC_NULL(2, SLV_2, SLV_20),
	SLC_VAR("gv_flags"),
	SLC_NULL(11, SLV_2, SLV_144),
};

/** Original field order for SlVehicleRoadVeh. */
const SaveLoadCompat _vehicle_roadveh_sl_compat[] = {
	SLC_VAR("common"),
	SLC_VAR("state"),
	SLC_VAR("frame"),
	SLC_VAR("blocked_ctr"),
	SLC_VAR("overtaking"),
	SLC_VAR("overtaking_ctr"),
	SLC_VAR("crashed_ctr"),
	SLC_VAR("reverse_ctr"),
	SLC_VAR("path.td"),
	SLC_VAR("path.tile"),
	SLC_NULL(2, SLV_6, SLV_69),
	SLC_VAR("gv_flags"),
	SLC_NULL(4, SLV_69, SLV_131),
	SLC_NULL(2, SLV_6, SLV_131),
	SLC_NULL(16, SLV_2, SLV_144),
};

/** Original field order for SlVehicleShip. */
const SaveLoadCompat _vehicle_ship_sl_compat[] = {
	SLC_VAR("common"),
	SLC_VAR("state"),
	SLC_VAR("path"),
	SLC_VAR("rotation"),
	SLC_NULL(16, SLV_2, SLV_144),
};

/** Original field order for SlVehicleAircraft. */
const SaveLoadCompat _vehicle_aircraft_sl_compat[] = {
	SLC_VAR("common"),
	SLC_VAR("crashed_counter"),
	SLC_VAR("pos"),
	SLC_VAR("targetairport"),
	SLC_VAR("state"),
	SLC_VAR("previous_pos"),
	SLC_VAR("last_direction"),
	SLC_VAR("number_consecutive_turns"),
	SLC_VAR("turn_counter"),
	SLC_VAR("flags"),
	SLC_NULL(13, SLV_2, SLV_144),
};

/** Original field order for SlVehicleEffect. */
const SaveLoadCompat _vehicle_effect_sl_compat[] = {
	SLC_VAR("subtype"),
	SLC_VAR("tile"),
	SLC_VAR("x_pos"),
	SLC_VAR("y_pos"),
	SLC_VAR("z_pos"),
	SLC_VAR("sprite_cache.sprite_seq.seq[0].sprite"),
	SLC_NULL(5, SL_MIN_VERSION, SLV_59),
	SLC_VAR("progress"),
	SLC_VAR("vehstatus"),
	SLC_VAR("animation_state"),
	SLC_VAR("animation_substate"),
	SLC_VAR("spritenum"),
	SLC_NULL(15, SLV_2, SLV_144),
};

/** Original field order for SlVehicleDisaster. */
const SaveLoadCompat _vehicle_disaster_sl_compat[] = {
	SLC_VAR("next"),
	SLC_VAR("subtype"),
	SLC_VAR("tile"),
	SLC_VAR("dest_tile"),
	SLC_VAR("x_pos"),
	SLC_VAR("y_pos"),
	SLC_VAR("z_pos"),
	SLC_VAR("direction"),
	SLC_NULL(5, SL_MIN_VERSION, SLV_58),
	SLC_VAR("owner"),
	SLC_VAR("vehstatus"),
	SLC_VAR("current_order.dest"),
	SLC_VAR("sprite_cache.sprite_seq.seq[0].sprite"),
	SLC_VAR("age"),
	SLC_VAR("tick_counter"),
	SLC_VAR("image_override"),
	SLC_VAR("big_ufo_destroyer_target"),
	SLC_VAR("flags"),
	SLC_NULL(16, SLV_2, SLV_144),
};

/** Original field order for vehicle_desc. */
const SaveLoadCompat _vehicle_sl_compat[] = {
	SLC_VAR("type"),
	SLC_VAR("train"),
	SLC_VAR("roadveh"),
	SLC_VAR("ship"),
	SLC_VAR("aircraft"),
	SLC_VAR("effect"),
	SLC_VAR("disaster"),
};

#endif /* SAVELOAD_COMPAT_VEHICLE_H */
