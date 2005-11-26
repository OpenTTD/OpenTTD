#ifndef AI_EVENT
#define AI_EVENT

/* Make the ai_event macro set correctly */
#ifdef GPMI
#	include <gpmi.h>
#	include "ai.h"

/* This is how we call events (with safety-check) to GPMI */
/* XXX -- This macro (vararg macro) isn't supported on old GCCs, but GPMI
 *         is using them too, so it isn't a real problem, yet */
#	define ai_event(player, params...) \
		if (player < MAX_PLAYERS && _ai_player[player].module != NULL) \
			gpmi_event(_ai_player[player].module, params)

#else
/* If GPMI isn't loaded, don't do a thing with the events (for now at least)
 *  Because old GCCs don't support vararg macros, and OTTD does support those
 *  GCCs, we need something tricky to ignore the events... and this is the solution :)
 *  Ugly, I know, but it works! */

#	ifdef DEF_EVENTS
	void CDECL empty_function(PlayerID player, int event, ...) {}
#	else
	extern void CDECL empty_function(PlayerID player, int event, ...);
#	endif
#	define ai_event empty_function

#endif /* GPMI */

/* To make our life a bit easier; you now only have to define new
 *  events here, and automaticly they work in OpenTTD without including
 *  the ottd_event package. Just because of some lovely macro-shit ;) */
#ifdef DEF_EVENTS
#	define DEF_EVENTS
#	define INITIAL_SET = -1
#else
#	define DEF_EVENTS extern
#	define INITIAL_SET
#endif /* DEF_EVENTS */

/* ------------ All available events -------------- */
DEF_EVENTS int ottd_Event_BuildStation								INITIAL_SET; // (station_index, station_tile)
DEF_EVENTS int ottd_Event_BuildRoadStation						INITIAL_SET; // (station_index, station_tile)

DEF_EVENTS int ottd_Event_BuildDepot									INITIAL_SET; // (depot_index, depot_tile)
DEF_EVENTS int ottd_Event_BuildRoadDepot							INITIAL_SET; // (depot_index, depot_tile)

DEF_EVENTS int ottd_Event_BuildVehicle								INITIAL_SET; // (vehicle_index, depot_tile)
DEF_EVENTS int ottd_Event_BuildRoadVehicle						INITIAL_SET; // (vehicle_index, depot_tile)

DEF_EVENTS int ottd_Event_VehicleEnterDepot						INITIAL_SET; // (vehicle_index, depot_tile)
DEF_EVENTS int ottd_Event_RoadVehicleEnterDepot				INITIAL_SET; // (vehicle_index, depot_tile)

DEF_EVENTS int ottd_Event_GiveOrder										INITIAL_SET; // (vehicle_index)
/* ----------------- End of list ------------------ */

#endif /* AI_EVENT */
