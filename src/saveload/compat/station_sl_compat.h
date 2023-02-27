/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file station_sl_compat.h Loading of station chunks before table headers were added. */

#ifndef SAVELOAD_COMPAT_STATION_H
#define SAVELOAD_COMPAT_STATION_H

#include "../saveload.h"

/** Original field order for _roadstop_desc. */
const SaveLoadCompat _roadstop_sl_compat[] = {
	SLC_VAR("xy"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_45),
	SLC_VAR("status"),
	SLC_NULL(4, SL_MIN_VERSION, SLV_9),
	SLC_NULL(2, SL_MIN_VERSION, SLV_45),
	SLC_NULL(1, SL_MIN_VERSION, SLV_26),
	SLC_VAR("next"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_45),
	SLC_NULL(4, SL_MIN_VERSION, SLV_25),
	SLC_NULL(1, SLV_25, SLV_26),
};

/** Original field order for SlStationSpecList. */
const SaveLoadCompat _station_spec_list_sl_compat[] = {
	SLC_VAR("grfid"),
	SLC_VAR("localidx"),
};

/** Nominal field order for SlRoadStopSpecList. */
const SaveLoadCompat _station_road_stop_spec_list_sl_compat[] = {
	SLC_VAR("grfid"),
	SLC_VAR("localidx"),
};

/** Original field order for SlStationCargo. */
const SaveLoadCompat _station_cargo_sl_compat[] = {
	SLC_VAR("first"),
	SLC_VAR("second"),
};

/** Original field order for SlStationFlow. */
const SaveLoadCompat _station_flow_sl_compat[] = {
	SLC_VAR("source"),
	SLC_VAR("via"),
	SLC_VAR("share"),
	SLC_VAR("restricted"),
};

/** Original field order for SlStationGoods. */
const SaveLoadCompat _station_goods_sl_compat[] = {
	SLC_VAR("waiting_acceptance"),
	SLC_VAR("status"),
	SLC_NULL(2, SLV_51, SLV_68),
	SLC_VAR("time_since_pickup"),
	SLC_VAR("rating"),
	SLC_VAR("cargo_source"),
	SLC_VAR("cargo_source_xy"),
	SLC_VAR("cargo_days"),
	SLC_VAR("last_speed"),
	SLC_VAR("last_age"),
	SLC_VAR("cargo_feeder_share"),
	SLC_VAR("amount_fract"),
	SLC_VAR("packets"),
	SLC_VAR("old_num_dests"),
	SLC_VAR("cargo.reserved_count"),
	SLC_VAR("link_graph"),
	SLC_VAR("node"),
	SLC_VAR("old_num_flows"),
	SLC_VAR("max_waiting_cargo"),
	SLC_VAR("flow"),
	SLC_VAR("cargo"),
};

/** Original field order for SlStationBase. */
const SaveLoadCompat _station_base_sl_compat[] = {
	SLC_VAR("xy"),
	SLC_VAR("town"),
	SLC_VAR("string_id"),
	SLC_VAR("name"),
	SLC_VAR("delete_ctr"),
	SLC_VAR("owner"),
	SLC_VAR("facilities"),
	SLC_VAR("build_date"),
	SLC_VAR("random_bits"),
	SLC_VAR("waiting_triggers"),
	SLC_VAR("num_specs"),
};

/** Original field order for SlStationNormal. */
const SaveLoadCompat _station_normal_sl_compat[] = {
	SLC_VAR("base"),
	SLC_VAR("train_station.tile"),
	SLC_VAR("train_station.w"),
	SLC_VAR("train_station.h"),
	SLC_VAR("bus_stops"),
	SLC_VAR("truck_stops"),
	SLC_NULL(4, SL_MIN_VERSION, SLV_MULTITILE_DOCKS),
	SLC_VAR("ship_station.tile"),
	SLC_VAR("ship_station.w"),
	SLC_VAR("ship_station.h"),
	SLC_VAR("docking_station.tile"),
	SLC_VAR("docking_station.w"),
	SLC_VAR("docking_station.h"),
	SLC_VAR("airport.tile"),
	SLC_VAR("airport.w"),
	SLC_VAR("airport.h"),
	SLC_VAR("airport.type"),
	SLC_VAR("airport.layout"),
	SLC_VAR("airport.flags"),
	SLC_VAR("airport.rotation"),
	SLC_VAR("storage"),
	SLC_VAR("airport.psa"),
	SLC_VAR("indtype"),
	SLC_VAR("time_since_load"),
	SLC_VAR("time_since_unload"),
	SLC_VAR("last_vehicle_type"),
	SLC_VAR("had_vehicle_of_type"),
	SLC_VAR("loading_vehicles"),
	SLC_VAR("always_accepted"),
	SLC_VAR("goods"),
};

/** Original field order for SlStationWaypoint. */
const SaveLoadCompat _station_waypoint_sl_compat[] = {
	SLC_VAR("base"),
	SLC_VAR("town_cn"),
	SLC_VAR("train_station.tile"),
	SLC_VAR("train_station.w"),
	SLC_VAR("train_station.h"),
};

/** Original field order for _station_desc. */
const SaveLoadCompat _station_sl_compat[] = {
	SLC_VAR("facilities"),
	SLC_VAR("normal"),
	SLC_VAR("waypoint"),
	SLC_VAR("speclist"),
};

/** Original field order for _old_station_desc. */
const SaveLoadCompat _old_station_sl_compat[] = {
	SLC_VAR("xy"),
	SLC_NULL(4, SL_MIN_VERSION, SLV_6),
	SLC_VAR("train_station.tile"),
	SLC_VAR("airport.tile"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_6),
	SLC_NULL(4, SLV_6, SLV_MULTITILE_DOCKS),
	SLC_VAR("town"),
	SLC_VAR("train_station.w"),
	SLC_VAR("train_station.h"),
	SLC_NULL(1, SL_MIN_VERSION, SLV_4),
	SLC_VAR("string_id"),
	SLC_VAR("name"),
	SLC_VAR("indtype"),
	SLC_VAR("had_vehicle_of_type"),
	SLC_VAR("time_since_load"),
	SLC_VAR("time_since_unload"),
	SLC_VAR("delete_ctr"),
	SLC_VAR("owner"),
	SLC_VAR("facilities"),
	SLC_VAR("airport.type"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_6),
	SLC_NULL(1, SL_MIN_VERSION, SLV_5),
	SLC_VAR("airport.flags"),
	SLC_NULL(2, SL_MIN_VERSION, SLV_26),
	SLC_VAR("last_vehicle_type"),
	SLC_NULL(2, SLV_3, SLV_26),
	SLC_VAR("build_date"),
	SLC_VAR("bus_stops"),
	SLC_VAR("truck_stops"),
	SLC_VAR("random_bits"),
	SLC_VAR("waiting_triggers"),
	SLC_VAR("num_specs"),
	SLC_VAR("loading_vehicles"),
	SLC_NULL(32, SLV_2, SL_MAX_VERSION),
	SLC_VAR("goods"),
	SLC_VAR("speclist"),

};

#endif /* SAVELOAD_COMPAT_STATION_H */
