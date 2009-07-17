/* $Id$ */

/** @file waypoint_sl.cpp Code handling saving and loading of waypoints */

#include "../stdafx.h"
#include "../waypoint.h"
#include "../newgrf_station.h"
#include "../town.h"

#include "table/strings.h"

#include "saveload.h"
#include "saveload_internal.h"

/**
 * Update waypoint graphics id against saved GRFID/localidx.
 * This is to ensure the chosen graphics are correct if GRF files are changed.
 */
void AfterLoadWaypoints()
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		if (wp->num_specs == 0) continue;

		for (uint i = 0; i < GetNumCustomStations(STAT_CLASS_WAYP); i++) {
			const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, i);
			if (statspec != NULL && statspec->grffile->grfid == wp->speclist->grfid && statspec->localidx == wp->speclist->localidx) {
				wp->speclist->spec = statspec;
				break;
			}
		}
	}
}

static uint16 _waypoint_town_index;
static StringID _waypoint_string_id;
static StationSpecList _waypoint_spec;

static const SaveLoad _waypoint_desc[] = {
	 SLE_CONDVAR(Waypoint, xy,            SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	 SLE_CONDVAR(Waypoint, xy,            SLE_UINT32,                  6, SL_MAX_VERSION),
	SLEG_CONDVAR(_waypoint_town_index,    SLE_UINT16,                 12, 121),
	 SLE_CONDREF(Waypoint, town,          REF_TOWN,                  122, SL_MAX_VERSION),
	 SLE_CONDVAR(Waypoint, town_cn,       SLE_FILE_U8 | SLE_VAR_U16,  12, 88),
	 SLE_CONDVAR(Waypoint, town_cn,       SLE_UINT16,                 89, SL_MAX_VERSION),
	SLEG_CONDVAR(_waypoint_string_id,     SLE_STRINGID,                0, 83),
	 SLE_CONDSTR(Waypoint, name,          SLE_STR, 0,                 84, SL_MAX_VERSION),
	     SLE_VAR(Waypoint, delete_ctr,    SLE_UINT8),

	 SLE_CONDVAR(Waypoint, build_date,    SLE_FILE_U16 | SLE_VAR_I32,  3, 30),
	 SLE_CONDVAR(Waypoint, build_date,    SLE_INT32,                  31, SL_MAX_VERSION),
	SLEG_CONDVAR(_waypoint_spec.localidx, SLE_UINT8,                   3, SL_MAX_VERSION),
	SLEG_CONDVAR(_waypoint_spec.grfid,    SLE_UINT32,                 17, SL_MAX_VERSION),
	 SLE_CONDVAR(Waypoint, owner,         SLE_UINT8,                 101, SL_MAX_VERSION),

	SLE_END()
};

static void Save_WAYP()
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		if (wp->num_specs == 0) {
			_waypoint_spec.grfid = 0;
		} else {
			_waypoint_spec = *wp->speclist;
		}

		SlSetArrayIndex(wp->index);
		SlObject(wp, _waypoint_desc);
	}
}

static void Load_WAYP()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		_waypoint_string_id = 0;
		_waypoint_town_index = 0;
		_waypoint_spec.grfid = 0;

		Waypoint *wp = new (index) Waypoint();
		SlObject(wp, _waypoint_desc);

		if (_waypoint_spec.grfid != 0) {
			wp->num_specs = 1;
			wp->speclist = MallocT<StationSpecList>(1);
			*wp->speclist = _waypoint_spec;
		}

		if (CheckSavegameVersion(84)) wp->name = (char *)(size_t)_waypoint_string_id;
		if (CheckSavegameVersion(122)) wp->town = (Town *)(size_t)_waypoint_town_index;
	}
}

static void Ptrs_WAYP()
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		SlObject(wp, _waypoint_desc);

		StringID sid = (StringID)(size_t)wp->name;
		if (CheckSavegameVersion(12)) {
			wp->town_cn = (sid & 0xC000) == 0xC000 ? (sid >> 8) & 0x3F : 0;
			wp->town = ClosestTownFromTile(wp->xy, UINT_MAX);
		} else if (CheckSavegameVersion(122)) {
			/* Only for versions 12 .. 122 */
			wp->town = Town::Get((size_t)wp->town);
		}
		if (CheckSavegameVersion(84)) {
			wp->name = CopyFromOldName(sid);
		}
	}
}

extern const ChunkHandler _waypoint_chunk_handlers[] = {
	{ 'CHKP', Save_WAYP, Load_WAYP, Ptrs_WAYP, CH_ARRAY | CH_LAST},
};
