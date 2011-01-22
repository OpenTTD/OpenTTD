/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_object.hpp Main object, on which all objects depend. */

#ifndef AI_OBJECT_HPP
#define AI_OBJECT_HPP

#include "../../misc/countedptr.hpp"
#include "../../road_type.h"
#include "../../rail_type.h"

#include "ai_types.hpp"

/**
 * The callback function when an AI suspends.
 */
typedef void (AISuspendCallbackProc)(class AIInstance *instance);

/**
 * The callback function for Mode-classes.
 */
typedef bool (AIModeProc)();

/**
 * Uper-parent object of all API classes. You should never use this class in
 *   your AI, as it doesn't publish any public functions. It is used
 *   internally to have a common place to handle general things, like internal
 *   command processing, and command-validation checks.
 */
class AIObject : public SimpleCountedObject {
friend void CcAI(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2);
friend class AIInstance;
protected:
	/**
	 * Executes a raw DoCommand for the AI.
	 */
	static bool DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint cmd, const char *text = NULL, AISuspendCallbackProc *callback = NULL);

	/**
	 * Sets the DoCommand costs counter to a value.
	 */
	static void SetDoCommandCosts(Money value);

	/**
	 * Increase the current value of the DoCommand costs counter.
	 */
	static void IncreaseDoCommandCosts(Money value);

	/**
	 * Get the current DoCommand costs counter.
	 */
	static Money GetDoCommandCosts();

	/**
	 * Set the DoCommand last error.
	 */
	static void SetLastError(AIErrorType last_error);

	/**
	 * Get the DoCommand last error.
	 */
	static AIErrorType GetLastError();

	/**
	 * Set the road type.
	 */
	static void SetRoadType(RoadType road_type);

	/**
	 * Get the road type.
	 */
	static RoadType GetRoadType();

	/**
	 * Set the rail type.
	 */
	static void SetRailType(RailType rail_type);

	/**
	 * Get the rail type.
	 */
	static RailType GetRailType();

	/**
	 * Set the current mode of your AI to this proc.
	 */
	static void SetDoCommandMode(AIModeProc *proc, AIObject *instance);

	/**
	 * Get the current mode your AI is currently under.
	 */
	static AIModeProc *GetDoCommandMode();

	/**
	 * Get the instance of the current mode your AI is currently under.
	 */
	static AIObject *GetDoCommandModeInstance();

	/**
	 * Set the delay of the DoCommand.
	 */
	static void SetDoCommandDelay(uint ticks);

	/**
	 * Get the delay of the DoCommand.
	 */
	static uint GetDoCommandDelay();

	/**
	 * Get the latest result of a DoCommand.
	 */
	static bool GetLastCommandRes();

	/**
	 * Get the latest stored new_vehicle_id.
	 */
	static VehicleID GetNewVehicleID();

	/**
	 * Get the latest stored new_sign_id.
	 */
	static SignID GetNewSignID();

	/**
	 * Get the latest stored new_tunnel_endtile.
	 */
	static TileIndex GetNewTunnelEndtile();

	/**
	 * Get the latest stored new_group_id.
	 */
	static GroupID GetNewGroupID();

	/**
	 * Can we suspend the AI at this moment?
	 */
	static bool CanSuspend();

	/**
	 * Get the internal value of allow_do_command. This can differ
	 * from CanSuspend() if the reason we are not allowed
	 * to execute a DoCommand is in squirrel and not the API.
	 * In that case use this function to restore the previous value.
	 */
	static bool GetAllowDoCommand();

	/**
	 * Get the pointer to store event data in.
	 */
	static void *&GetEventPointer();

	/**
	 * Set the cost of the last command.
	 */
	static void SetLastCost(Money last_cost);

	/**
	 * Get the cost of the last command.
	 */
	static Money GetLastCost();

	/**
	 * Set a variable that can be used by callback functions to pass information.
	 */
	static void SetCallbackVariable(int index, int value);

	/**
	 * Get the variable that is used by callback functions to pass information.
	 */
	static int GetCallbackVariable(int index);

public:
	/**
	 * Store the latest result of a DoCommand per company.
	 * @note NEVER use this yourself in your AI!
	 * @param res The result of the last command.
	 */
	static void SetLastCommandRes(bool res);

	/**
	 * Store a new_vehicle_id per company.
	 * @note NEVER use this yourself in your AI!
	 * @param vehicle_id The new VehicleID.
	 */
	static void SetNewVehicleID(VehicleID vehicle_id);

	/**
	 * Store a new_sign_id per company.
	 * @note NEVER use this yourself in your AI!
	 * @param sign_id The new SignID.
	 */
	static void SetNewSignID(SignID sign_id);

	/**
	 * Store a new_tunnel_endtile per company.
	 * @note NEVER use this yourself in your AI!
	 * @param tile The new TileIndex.
	 */
	static void SetNewTunnelEndtile(TileIndex tile);

	/**
	 * Store a new_group_id per company.
	 * @note NEVER use this yourself in your AI!
	 * @param group_id The new GroupID.
	 */
	static void SetNewGroupID(GroupID group_id);

	/**
	 * Store a allow_do_command per company.
	 * @note NEVER use this yourself in your AI!
	 * @param allow The new allow.
	 */
	static void SetAllowDoCommand(bool allow);

	/**
	 * Get the pointer to store log message in.
	 * @note NEVER use this yourself in your AI!
	 */
	static void *&GetLogPointer();
};

#endif /* AI_OBJECT_HPP */
