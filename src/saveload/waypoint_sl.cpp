/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file waypoint_sl.cpp Code handling saving and loading of waypoints */

#include "../stdafx.h"
#include "../waypoint_base.h"
#include "../newgrf_station.h"
#include "../vehicle_base.h"
#include "../town.h"
#include "../newgrf.h"

#include "table/strings.h"

#include "saveload_internal.h"

#include "../safeguards.h"

/** Helper structure to convert from the old waypoint system. */
struct OldWaypoint {
	size_t index;
	TileIndex xy;
	TownID town_index;
	Town *town;
	uint16 town_cn;
	StringID string_id;
	char *name;
	uint8 delete_ctr;
	Date build_date;
	uint8 localidx;
	uint32 grfid;
	const StationSpec *spec;
	Owner owner;

	size_t new_index;
};

/** Temporary array with old waypoints. */
static std::vector<OldWaypoint> _old_waypoints;

/**
 * Update the waypoint orders to get the new waypoint ID.
 * @param o the order 'list' to check.
 */
static void UpdateWaypointOrder(Order *o)
{
	if (!o->IsType(OT_GOTO_WAYPOINT)) return;

	for (OldWaypoint &wp : _old_waypoints) {
		if (wp.index != o->GetDestination()) continue;

		o->SetDestination((DestinationID)wp.new_index);
		return;
	}
}

/**
 * Perform all steps to upgrade from the old waypoints to the new version
 * that uses station. This includes some old saveload mechanics.
 */
void MoveWaypointsToBaseStations()
{
	/* In version 17, ground type is moved from m2 to m4 for depots and
	 * waypoints to make way for storing the index in m2. The custom graphics
	 * id which was stored in m4 is now saved as a grf/id reference in the
	 * waypoint struct. */
	if (IsSavegameVersionBefore(SLV_17)) {
		for (OldWaypoint &wp : _old_waypoints) {
			if (wp.delete_ctr != 0) continue; // The waypoint was deleted

			/* Waypoint indices were not added to the map prior to this. */
			_m[wp.xy].m2 = (StationID)wp.index;

			if (HasBit(_m[wp.xy].m3, 4)) {
				wp.spec = StationClass::Get(STAT_CLASS_WAYP)->GetSpec(_m[wp.xy].m4 + 1);
			}
		}
	} else {
		/* As of version 17, we recalculate the custom graphic ID of waypoints
		 * from the GRF ID / station index. */
		for (OldWaypoint &wp : _old_waypoints) {
			StationClass* stclass = StationClass::Get(STAT_CLASS_WAYP);
			for (uint i = 0; i < stclass->GetSpecCount(); i++) {
				const StationSpec *statspec = stclass->GetSpec(i);
				if (statspec != nullptr && statspec->grf_prop.grffile->grfid == wp.grfid && statspec->grf_prop.local_id == wp.localidx) {
					wp.spec = statspec;
					break;
				}
			}
		}
	}

	if (!Waypoint::CanAllocateItem(_old_waypoints.size())) SlError(STR_ERROR_TOO_MANY_STATIONS_LOADING);

	/* All saveload conversions have been done. Create the new waypoints! */
	for (OldWaypoint &wp : _old_waypoints) {
		Waypoint *new_wp = new Waypoint(wp.xy);
		new_wp->town       = wp.town;
		new_wp->town_cn    = wp.town_cn;
		new_wp->name       = wp.name;
		new_wp->delete_ctr = 0; // Just reset delete counter for once.
		new_wp->build_date = wp.build_date;
		new_wp->owner      = wp.owner;

		new_wp->string_id = STR_SV_STNAME_WAYPOINT;

		TileIndex t = wp.xy;
		if (IsTileType(t, MP_RAILWAY) && GetRailTileType(t) == 2 /* RAIL_TILE_WAYPOINT */ && _m[t].m2 == wp.index) {
			/* The tile might've been reserved! */
			bool reserved = !IsSavegameVersionBefore(SLV_100) && HasBit(_m[t].m5, 4);

			/* The tile really has our waypoint, so reassign the map array */
			MakeRailWaypoint(t, GetTileOwner(t), new_wp->index, (Axis)GB(_m[t].m5, 0, 1), 0, GetRailType(t));
			new_wp->facilities |= FACIL_TRAIN;
			new_wp->owner = GetTileOwner(t);

			SetRailStationReservation(t, reserved);

			if (wp.spec != nullptr) {
				SetCustomStationSpecIndex(t, AllocateSpecToStation(wp.spec, new_wp, true));
			}
			new_wp->rect.BeforeAddTile(t, StationRect::ADD_FORCE);
		}

		wp.new_index = new_wp->index;
	}

	/* Update the orders of vehicles */
	for (OrderList *ol : OrderList::Iterate()) {
		if (ol->GetFirstSharedVehicle()->type != VEH_TRAIN) continue;

		for (Order *o = ol->GetFirstOrder(); o != nullptr; o = o->next) UpdateWaypointOrder(o);
	}

	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type != VEH_TRAIN) continue;

		UpdateWaypointOrder(&v->current_order);
	}

	_old_waypoints.clear();
	_old_waypoints.shrink_to_fit();
}

static const SaveLoad _old_waypoint_desc[] = {
	SLE_CONDVAR(OldWaypoint, xy,         SLE_FILE_U16 | SLE_VAR_U32,  SL_MIN_VERSION, SLV_6),
	SLE_CONDVAR(OldWaypoint, xy,         SLE_UINT32,                  SLV_6, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, town_index, SLE_UINT16,                 SLV_12, SLV_122),
	SLE_CONDREF(OldWaypoint, town,       REF_TOWN,                  SLV_122, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, town_cn,    SLE_FILE_U8 | SLE_VAR_U16,  SLV_12, SLV_89),
	SLE_CONDVAR(OldWaypoint, town_cn,    SLE_UINT16,                 SLV_89, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, string_id,  SLE_STRINGID,                SL_MIN_VERSION, SLV_84),
	SLE_CONDSTR(OldWaypoint, name,       SLE_STR, 0,                 SLV_84, SL_MAX_VERSION),
	    SLE_VAR(OldWaypoint, delete_ctr, SLE_UINT8),

	SLE_CONDVAR(OldWaypoint, build_date, SLE_FILE_U16 | SLE_VAR_I32,  SLV_3, SLV_31),
	SLE_CONDVAR(OldWaypoint, build_date, SLE_INT32,                  SLV_31, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, localidx,   SLE_UINT8,                   SLV_3, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, grfid,      SLE_UINT32,                 SLV_17, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, owner,      SLE_UINT8,                 SLV_101, SL_MAX_VERSION),

	SLE_END()
};

static void Load_WAYP()
{
	/* Precaution for when loading failed and it didn't get cleared */
	_old_waypoints.clear();

	int index;

	while ((index = SlIterateArray()) != -1) {
		/*C++17: OldWaypoint *wp = &*/ _old_waypoints.emplace_back();
		OldWaypoint *wp = &_old_waypoints.back();
		memset(wp, 0, sizeof(*wp));

		wp->index = index;
		SlObject(wp, _old_waypoint_desc);
	}
}

static void Ptrs_WAYP()
{
	for (OldWaypoint &wp : _old_waypoints) {
		SlObject(&wp, _old_waypoint_desc);

		if (IsSavegameVersionBefore(SLV_12)) {
			wp.town_cn = (wp.string_id & 0xC000) == 0xC000 ? (wp.string_id >> 8) & 0x3F : 0;
			wp.town = ClosestTownFromTile(wp.xy, UINT_MAX);
		} else if (IsSavegameVersionBefore(SLV_122)) {
			/* Only for versions 12 .. 122 */
			if (!Town::IsValidID(wp.town_index)) {
				/* Upon a corrupted waypoint we'll likely get here. The next step will be to
				 * loop over all Ptrs procs to nullptr the pointers. However, we don't know
				 * whether we're in the nullptr or "normal" Ptrs proc. So just clear the list
				 * of old waypoints we constructed and then this waypoint (and the other
				 * possibly corrupt ones) will not be queried in the nullptr Ptrs proc run. */
				_old_waypoints.clear();
				SlErrorCorrupt("Referencing invalid Town");
			}
			wp.town = Town::Get(wp.town_index);
		}
		if (IsSavegameVersionBefore(SLV_84)) {
			wp.name = CopyFromOldName(wp.string_id);
		}
	}
}

extern const ChunkHandler _waypoint_chunk_handlers[] = {
	{ 'CHKP', nullptr, Load_WAYP, Ptrs_WAYP, nullptr, CH_ARRAY | CH_LAST},
};
