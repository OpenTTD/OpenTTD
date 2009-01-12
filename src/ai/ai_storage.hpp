/* $Id$ */

/** @file ai_storage.hpp Defines AIStorage and includes all files required for it. */

#ifndef AI_STORAGE_HPP
#define AI_STORAGE_HPP

#include "../command_func.h"
#include "../map_func.h"
#include "../network/network.h"
#include "../company_func.h"
#include "../signs_func.h"
#include "../tunnelbridge.h"
#include "../vehicle_func.h"
#include "../group.h"

#include <vector>

/**
 * The callback function for Mode-classes.
 */
typedef bool (AIModeProc)(TileIndex tile, uint32 p1, uint32 p2, uint procc, CommandCost costs);

/**
 * The storage for each AI. It keeps track of important information.
 */
class AIStorage {
friend class AIObject;
private:
	AIModeProc *mode;                //!< The current build mode we are int.
	class AIObject *mode_instance;   //!< The instance belonging to the current build mode.

	uint delay;                      //!< The ticks of delay each DoCommand has.
	bool allow_do_command;           //!< Is the usage of DoCommands restricted?

	CommandCost costs;               //!< The costs the AI is tracking.
	Money last_cost;                 //!< The last cost of the command.
	uint last_error;                 //!< The last error of the command.
	bool last_command_res;           //!< The last result of the command.

	VehicleID new_vehicle_id;        //!< The ID of the new Vehicle.
	SignID new_sign_id;              //!< The ID of the new Sign.
	TileIndex new_tunnel_endtile;    //!< The TileIndex of the new Tunnel.
	GroupID new_group_id;            //!< The ID of the new Group.

	std::vector<int> callback_value; //!< The values which need to survive a callback.

	RoadType road_type;              //!< The current roadtype we build.
	RailType rail_type;              //!< The current railtype we build.

	void *event_data;                //!< Pointer to the event data storage.
	void *log_data;                  //!< Pointer to the log data storage.

public:
	AIStorage() :
		mode              (NULL),
		mode_instance     (NULL),
		delay             (1),
		allow_do_command  (true),
		/* costs (can't be set) */
		last_cost         (0),
		last_error        (STR_NULL),
		last_command_res  (true),
		new_vehicle_id    (0),
		new_sign_id       (0),
		new_tunnel_endtile(INVALID_TILE),
		new_group_id      (0),
		/* calback_value (can't be set) */
		road_type         (INVALID_ROADTYPE),
		rail_type         (INVALID_RAILTYPE),
		event_data        (NULL),
		log_data          (NULL)
	{ }

	~AIStorage();
};

#endif /* AI_STORAGE_HPP */
