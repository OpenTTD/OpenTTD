/* $Id$ */

/** @file waypoint_sl.cpp Code handling saving and loading of waypoints */

#include "../stdafx.h"
#include "../waypoint.h"
#include "../newgrf_station.h"
#include "../town.h"

#include "table/strings.h"

#include "saveload.h"

/**
 * Update waypoint graphics id against saved GRFID/localidx.
 * This is to ensure the chosen graphics are correct if GRF files are changed.
 */
void AfterLoadWaypoints()
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		uint i;

		if (wp->grfid == 0) continue;

		for (i = 0; i < GetNumCustomStations(STAT_CLASS_WAYP); i++) {
			const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, i);
			if (statspec != NULL && statspec->grffile->grfid == wp->grfid && statspec->localidx == wp->localidx) {
				wp->stat_id = i;
				break;
			}
		}
	}
}

/**
 * Fix savegames which stored waypoints in their old format
 */
void FixOldWaypoints()
{
	Waypoint *wp;

	/* Convert the old 'town_or_string', to 'string' / 'town' / 'town_cn' */
	FOR_ALL_WAYPOINTS(wp) {
		wp->town_index = ClosestTownFromTile(wp->xy, UINT_MAX)->index;
		wp->town_cn = 0;
		if (wp->string & 0xC000) {
			wp->town_cn = wp->string & 0x3F;
			wp->string = STR_NULL;
		}
	}
}

static const SaveLoad _waypoint_desc[] = {
	SLE_CONDVAR(Waypoint, xy,         SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Waypoint, xy,         SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, town_index, SLE_UINT16,                 12, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, town_cn,    SLE_FILE_U8 | SLE_VAR_U16,  12, 88),
	SLE_CONDVAR(Waypoint, town_cn,    SLE_UINT16,                 89, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, string,     SLE_STRINGID,                0, 83),
	SLE_CONDSTR(Waypoint, name,       SLE_STR, 0,                 84, SL_MAX_VERSION),
	    SLE_VAR(Waypoint, deleted,    SLE_UINT8),

	SLE_CONDVAR(Waypoint, build_date, SLE_FILE_U16 | SLE_VAR_I32,  3, 30),
	SLE_CONDVAR(Waypoint, build_date, SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, localidx,   SLE_UINT8,                   3, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, grfid,      SLE_UINT32,                 17, SL_MAX_VERSION),
	SLE_CONDVAR(Waypoint, owner,      SLE_UINT8,                 101, SL_MAX_VERSION),

	SLE_END()
};

static void Save_WAYP()
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		SlSetArrayIndex(wp->index);
		SlObject(wp, _waypoint_desc);
	}
}

static void Load_WAYP()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		Waypoint *wp = new (index) Waypoint();
		SlObject(wp, _waypoint_desc);
	}
}

extern const ChunkHandler _waypoint_chunk_handlers[] = {
	{ 'CHKP', Save_WAYP, Load_WAYP, CH_ARRAY | CH_LAST},
};
