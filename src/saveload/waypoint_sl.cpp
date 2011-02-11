/* $Id$ */

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
	OwnerByte owner;

	size_t new_index;
};

/** Temporary array with old waypoints. */
static SmallVector<OldWaypoint, 16> _old_waypoints;

/**
 * Update the waypoint orders to get the new waypoint ID.
 * @param o the order 'list' to check.
 */
static void UpdateWaypointOrder(Order *o)
{
	if (!o->IsType(OT_GOTO_WAYPOINT)) return;

	for (OldWaypoint *wp = _old_waypoints.Begin(); wp != _old_waypoints.End(); wp++) {
		if (wp->index != o->GetDestination()) continue;

		o->SetDestination((DestinationID)wp->new_index);
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
	if (IsSavegameVersionBefore(17)) {
		for (OldWaypoint *wp = _old_waypoints.Begin(); wp != _old_waypoints.End(); wp++) {
			if (wp->delete_ctr != 0) continue; // The waypoint was deleted

			/* Waypoint indices were not added to the map prior to this. */
			_m[wp->xy].m2 = (StationID)wp->index;

			if (HasBit(_m[wp->xy].m3, 4)) {
				wp->spec = StationClass::Get(STAT_CLASS_WAYP, _m[wp->xy].m4 + 1);
			}
		}
	} else {
		/* As of version 17, we recalculate the custom graphic ID of waypoints
		 * from the GRF ID / station index. */
		for (OldWaypoint *wp = _old_waypoints.Begin(); wp != _old_waypoints.End(); wp++) {
			for (uint i = 0; i < StationClass::GetCount(STAT_CLASS_WAYP); i++) {
				const StationSpec *statspec =  StationClass::Get(STAT_CLASS_WAYP, i);
				if (statspec != NULL && statspec->grf_prop.grffile->grfid == wp->grfid && statspec->grf_prop.local_id == wp->localidx) {
					wp->spec = statspec;
					break;
				}
			}
		}
	}

	if (!Waypoint::CanAllocateItem(_old_waypoints.Length())) SlError(STR_ERROR_TOO_MANY_STATIONS_LOADING);

	/* All saveload conversions have been done. Create the new waypoints! */
	for (OldWaypoint *wp = _old_waypoints.Begin(); wp != _old_waypoints.End(); wp++) {
		Waypoint *new_wp = new Waypoint(wp->xy);
		new_wp->town       = wp->town;
		new_wp->town_cn    = wp->town_cn;
		new_wp->name       = wp->name;
		new_wp->delete_ctr = 0; // Just reset delete counter for once.
		new_wp->build_date = wp->build_date;
		new_wp->owner      = wp->owner;

		new_wp->string_id = STR_SV_STNAME_WAYPOINT;

		TileIndex t = wp->xy;
		if (IsTileType(t, MP_RAILWAY) && GetRailTileType(t) == 2 /* RAIL_TILE_WAYPOINT */ && _m[t].m2 == wp->index) {
			/* The tile might've been reserved! */
			bool reserved = !IsSavegameVersionBefore(100) && HasBit(_m[t].m5, 4);

			/* The tile really has our waypoint, so reassign the map array */
			MakeRailWaypoint(t, GetTileOwner(t), new_wp->index, (Axis)GB(_m[t].m5, 0, 1), 0, GetRailType(t));
			new_wp->facilities |= FACIL_TRAIN;
			new_wp->owner = GetTileOwner(t);

			SetRailStationReservation(t, reserved);

			if (wp->spec != NULL) {
				SetCustomStationSpecIndex(t, AllocateSpecToStation(wp->spec, new_wp, true));
			}
			new_wp->rect.BeforeAddTile(t, StationRect::ADD_FORCE);
		}

		wp->new_index = new_wp->index;
	}

	/* Update the orders of vehicles */
	OrderList *ol;
	FOR_ALL_ORDER_LISTS(ol) {
		if (ol->GetFirstSharedVehicle()->type != VEH_TRAIN) continue;

		for (Order *o = ol->GetFirstOrder(); o != NULL; o = o->next) UpdateWaypointOrder(o);
	}

	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type != VEH_TRAIN) continue;

		UpdateWaypointOrder(&v->current_order);
	}

	_old_waypoints.Reset();
}

static const SaveLoad _old_waypoint_desc[] = {
	SLE_CONDVAR(OldWaypoint, xy,         SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(OldWaypoint, xy,         SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, town_index, SLE_UINT16,                 12, 121),
	SLE_CONDREF(OldWaypoint, town,       REF_TOWN,                  122, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, town_cn,    SLE_FILE_U8 | SLE_VAR_U16,  12, 88),
	SLE_CONDVAR(OldWaypoint, town_cn,    SLE_UINT16,                 89, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, string_id,  SLE_STRINGID,                0, 83),
	SLE_CONDSTR(OldWaypoint, name,       SLE_STR, 0,                 84, SL_MAX_VERSION),
	    SLE_VAR(OldWaypoint, delete_ctr, SLE_UINT8),

	SLE_CONDVAR(OldWaypoint, build_date, SLE_FILE_U16 | SLE_VAR_I32,  3, 30),
	SLE_CONDVAR(OldWaypoint, build_date, SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, localidx,   SLE_UINT8,                   3, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, grfid,      SLE_UINT32,                 17, SL_MAX_VERSION),
	SLE_CONDVAR(OldWaypoint, owner,      SLE_UINT8,                 101, SL_MAX_VERSION),

	SLE_END()
};

static void Load_WAYP()
{
	/* Precaution for when loading failed and it didn't get cleared */
	_old_waypoints.Clear();

	int index;

	while ((index = SlIterateArray()) != -1) {
		OldWaypoint *wp = _old_waypoints.Append();
		memset(wp, 0, sizeof(*wp));

		wp->index = index;
		SlObject(wp, _old_waypoint_desc);
	}
}

static void Ptrs_WAYP()
{
	for (OldWaypoint *wp = _old_waypoints.Begin(); wp != _old_waypoints.End(); wp++) {
		SlObject(wp, _old_waypoint_desc);

		if (IsSavegameVersionBefore(12)) {
			wp->town_cn = (wp->string_id & 0xC000) == 0xC000 ? (wp->string_id >> 8) & 0x3F : 0;
			wp->town = ClosestTownFromTile(wp->xy, UINT_MAX);
		} else if (IsSavegameVersionBefore(122)) {
			/* Only for versions 12 .. 122 */
			if (!Town::IsValidID(wp->town_index)) {
				/* Upon a corrupted waypoint we'll likely get here. The next step will be to
				 * loop over all Ptrs procs to NULL the pointers. However, we don't know
				 * whether we're in the NULL or "normal" Ptrs proc. So just clear the list
				 * of old waypoints we constructed and then this waypoint (and the other
				 * possibly corrupt ones) will not be queried in the NULL Ptrs proc run. */
				_old_waypoints.Clear();
				SlErrorCorrupt("Referencing invalid Town");
			}
			wp->town = Town::Get(wp->town_index);
		}
		if (IsSavegameVersionBefore(84)) {
			wp->name = CopyFromOldName(wp->string_id);
		}
	}
}

extern const ChunkHandler _waypoint_chunk_handlers[] = {
	{ 'CHKP', NULL, Load_WAYP, Ptrs_WAYP, NULL, CH_ARRAY | CH_LAST},
};
