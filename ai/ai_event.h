#ifndef AI_EVENT
#define AI_EVENT

/* Make the ai_event macro set correctly */
#ifdef GPMI
#	include <gpmi.h>
#	include "ai.h"

/* This is how we call events (with safety-check) to GPMI */
/* XXX -- This macro works only for some compilers (all GCCs for example).
 *   Some compilers on the other hand (MSCV!!) doesn't support variadic macros
 *   causing this to fail. There is no known solution. If you know any, please
 *   tell us ASAP! */
#	define ai_event(player, event, ...) \
		if ((player) < MAX_PLAYERS && _ai_player[(player)].module != NULL) \
			gpmi_event(_ai_player[(player)].module, (event), _ai_current_uid, ##__VA_ARGS__)

#else /* GPMI */

/* XXX -- Some compilers (like MSVC :() doesn't support variadic macros,
 *   which means we have to go to a lot of trouble to get the ai_event() ignored
 *   in case GPMI is disabled... KILL KILL KILL!
 */
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
DEF_EVENTS int ottd_Event_CommandFailed								INITIAL_SET; // (tile, unique_id)
DEF_EVENTS int ottd_Event_CommandSucceeded						INITIAL_SET; // (tile, unique_id)

DEF_EVENTS int ottd_Event_BuildStation								INITIAL_SET; // (station_index, station_tile)
DEF_EVENTS int ottd_Event_BuildRoadStation						INITIAL_SET; // (station_index, station_tile)

DEF_EVENTS int ottd_Event_BuildDepot									INITIAL_SET; // (depot_index, depot_tile)
DEF_EVENTS int ottd_Event_BuildRoadDepot							INITIAL_SET; // (depot_index, depot_tile)

DEF_EVENTS int ottd_Event_BuildVehicle								INITIAL_SET; // (vehicle_index, depot_tile)
DEF_EVENTS int ottd_Event_BuildRoadVehicle						INITIAL_SET; // (vehicle_index, depot_tile)

DEF_EVENTS int ottd_Event_VehicleEnterDepot						INITIAL_SET; // (vehicle_index, depot_tile)
DEF_EVENTS int ottd_Event_RoadVehicleEnterDepot				INITIAL_SET; // (vehicle_index, depot_tile)

DEF_EVENTS int ottd_Event_GiveOrder										INITIAL_SET; // (vehicle_index)

DEF_EVENTS int ottd_Event_BuildRoad										INITIAL_SET; // (road_tile, road_pieces)
/* ----------------- End of list ------------------ */

#endif /* AI_EVENT */
