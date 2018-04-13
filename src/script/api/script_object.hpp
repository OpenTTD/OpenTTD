/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_object.hpp Main object, on which all objects depend. */

#ifndef SCRIPT_OBJECT_HPP
#define SCRIPT_OBJECT_HPP

#include "../../misc/countedptr.hpp"
#include "../../road_type.h"
#include "../../rail_type.h"

#include "script_types.hpp"
#include "../script_suspend.hpp"

/**
 * The callback function for Mode-classes.
 */
typedef bool (ScriptModeProc)();

/**
 * Uper-parent object of all API classes. You should never use this class in
 *   your script, as it doesn't publish any public functions. It is used
 *   internally to have a common place to handle general things, like internal
 *   command processing, and command-validation checks.
 * @api none
 */
class ScriptObject : public SimpleCountedObject {
friend class ScriptInstance;
friend class ScriptController;
protected:
	/**
	 * A class that handles the current active instance. By instantiating it at
	 *  the beginning of a function with the current active instance, it remains
	 *  active till the scope of the variable closes. It then automatically
	 *  reverts to the active instance it was before instantiating.
	 */
	class ActiveInstance {
	friend class ScriptObject;
	public:
		ActiveInstance(ScriptInstance *instance);
		~ActiveInstance();
	private:
		ScriptInstance *last_active;    ///< The active instance before we go instantiated.

		static ScriptInstance *active;  ///< The global current active instance.
	};

public:
	/**
	 * Store the latest result of a DoCommand per company.
	 * @param res The result of the last command.
	 */
	static void SetLastCommandRes(bool res);

	/**
	 * Get the currently active instance.
	 * @return The instance.
	 */
	static class ScriptInstance *GetActiveInstance();

protected:
	/**
	 * Executes a raw DoCommand for the script.
	 */
	static bool DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint cmd, const char *text = NULL, Script_SuspendCallbackProc *callback = NULL);

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
	static void SetLastError(ScriptErrorType last_error);

	/**
	 * Get the DoCommand last error.
	 */
	static ScriptErrorType GetLastError();

	/**
	 * Set the road type identifier.
	 */
	static void SetRoadType(RoadTypeIdentifier rtid);

	/**
	 * Get the road type identifier.
	 */
	static RoadTypeIdentifier GetRoadType();

	/**
	 * Set the rail type.
	 */
	static void SetRailType(RailType rail_type);

	/**
	 * Get the rail type.
	 */
	static RailType GetRailType();

	/**
	 * Set the current mode of your script to this proc.
	 */
	static void SetDoCommandMode(ScriptModeProc *proc, ScriptObject *instance);

	/**
	 * Get the current mode your script is currently under.
	 */
	static ScriptModeProc *GetDoCommandMode();

	/**
	 * Get the instance of the current mode your script is currently under.
	 */
	static ScriptObject *GetDoCommandModeInstance();

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
	 * Get the latest stored new_group_id.
	 */
	static GroupID GetNewGroupID();

	/**
	 * Get the latest stored new_goal_id.
	 */
	static GoalID GetNewGoalID();

	/**
	 * Get the latest stored new_story_page_id.
	 */
	static StoryPageID GetNewStoryPageID();

	/**
	 * Get the latest stored new_story_page_id.
	 */
	static StoryPageID GetNewStoryPageElementID();

	/**
	 * Store a allow_do_command per company.
	 * @param allow The new allow.
	 */
	static void SetAllowDoCommand(bool allow);

	/**
	 * Get the internal value of allow_do_command. This can differ
	 * from CanSuspend() if the reason we are not allowed
	 * to execute a DoCommand is in squirrel and not the API.
	 * In that case use this function to restore the previous value.
	 * @return True iff DoCommands are allowed in the current scope.
	 */
	static bool GetAllowDoCommand();

	/**
	 * Set the current company to execute commands for or request
	 *  information about.
	 * @param company The new company.
	 */
	static void SetCompany(CompanyID company);

	/**
	 * Get the current company we are executing commands for or
	 *  requesting information about.
	 * @return The current company.
	 */
	static CompanyID GetCompany();

	/**
	 * Get the root company, the company that the script really
	 *  runs under / for.
	 * @return The root company.
	 */
	static CompanyID GetRootCompany();

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

	/**
	 * Can we suspend the script at this moment?
	 */
	static bool CanSuspend();

	/**
	 * Get the pointer to store event data in.
	 */
	static void *&GetEventPointer();

	/**
	 * Get the pointer to store log message in.
	 */
	static void *&GetLogPointer();

	/**
	 * Get an allocated string with all control codes stripped off.
	 */
	static char *GetString(StringID string);

private:
	/**
	 * Store a new_vehicle_id per company.
	 * @param vehicle_id The new VehicleID.
	 */
	static void SetNewVehicleID(VehicleID vehicle_id);

	/**
	 * Store a new_sign_id per company.
	 * @param sign_id The new SignID.
	 */
	static void SetNewSignID(SignID sign_id);

	/**
	 * Store a new_group_id per company.
	 * @param group_id The new GroupID.
	 */
	static void SetNewGroupID(GroupID group_id);

	/**
	 * Store a new_goal_id per company.
	 * @param goal_id The new GoalID.
	 */
	static void SetNewGoalID(GoalID goal_id);

	/**
	 * Store a new_story_page_id per company.
	 * @param story_page_id The new StoryPageID.
	 */
	static void SetNewStoryPageID(StoryPageID story_page_id);

	/**
	 * Store a new_story_page_id per company.
	 * @param story_page_id The new StoryPageID.
	 */
	static void SetNewStoryPageElementID(StoryPageElementID story_page_element_id);
};

#endif /* SCRIPT_OBJECT_HPP */
