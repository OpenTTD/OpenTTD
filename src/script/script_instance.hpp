/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_instance.hpp The ScriptInstance tracks a script. */

#ifndef SCRIPT_INSTANCE_HPP
#define SCRIPT_INSTANCE_HPP

#include <squirrel.h>
#include "script_suspend.hpp"

#include "../command_type.h"
#include "../company_type.h"
#include "../fileio_type.h"

static const uint SQUIRREL_MAX_DEPTH = 25; ///< The maximum recursive depth for items stored in the savegame.

/** Runtime information about a script like a pointer to the squirrel vm and the current state. */
class ScriptInstance {
public:
	friend class ScriptObject;
	friend class ScriptController;

	/**
	 * Create a new script.
	 */
	ScriptInstance(const char *APIName);
	virtual ~ScriptInstance();

	/**
	 * Initialize the script and prepare it for its first run.
	 * @param main_script The full path of the script to load.
	 * @param instance_name The name of the instance out of the script to load.
	 * @param company Which company this script is serving.
	 */
	void Initialize(const char *main_script, const char *instance_name, CompanyID company);

	/**
	 * Get the value of a setting of the current instance.
	 * @param name The name of the setting.
	 * @return the value for the setting, or -1 if the setting is not known.
	 */
	virtual int GetSetting(const char *name) = 0;

	/**
	 * Find a library.
	 * @param library The library name to find.
	 * @param version The version the library should have.
	 * @return The library if found, nullptr otherwise.
	 */
	virtual class ScriptInfo *FindLibrary(const char *library, int version) = 0;

	/**
	 * A script in multiplayer waits for the server to handle his DoCommand.
	 *  It keeps waiting for this until this function is called.
	 */
	void Continue();

	/**
	 * Run the GameLoop of a script.
	 */
	void GameLoop();

	/**
	 * Let the VM collect any garbage.
	 */
	void CollectGarbage() const;

	/**
	 * Get the storage of this script.
	 */
	class ScriptStorage *GetStorage();

	/**
	 * Get the log pointer of this script.
	 */
	void *GetLogPointer();

	/**
	 * Return a true/false reply for a DoCommand.
	 */
	static void DoCommandReturn(ScriptInstance *instance);

	/**
	 * Return a VehicleID reply for a DoCommand.
	 */
	static void DoCommandReturnVehicleID(ScriptInstance *instance);

	/**
	 * Return a SignID reply for a DoCommand.
	 */
	static void DoCommandReturnSignID(ScriptInstance *instance);

	/**
	 * Return a GroupID reply for a DoCommand.
	 */
	static void DoCommandReturnGroupID(ScriptInstance *instance);

	/**
	 * Return a GoalID reply for a DoCommand.
	 */
	static void DoCommandReturnGoalID(ScriptInstance *instance);

	/**
	 * Return a StoryPageID reply for a DoCommand.
	 */
	static void DoCommandReturnStoryPageID(ScriptInstance *instance);

	/**
	 * Return a StoryPageElementID reply for a DoCommand.
	 */
	static void DoCommandReturnStoryPageElementID(ScriptInstance *instance);

	/**
	 * Get the controller attached to the instance.
	 */
	class ScriptController *GetController() { return controller; }

	/**
	 * Return the "this script died" value
	 */
	inline bool IsDead() const { return this->is_dead; }

	/**
	 * Call the script Save function and save all data in the savegame.
	 */
	void Save();

	/**
	 * Don't save any data in the savegame.
	 */
	static void SaveEmpty();

	/**
	 * Load data from a savegame and store it on the stack.
	 * @param version The version of the script when saving, or -1 if this was
	 *  not the original script saving the game.
	 */
	void Load(int version);

	/**
	 * Load and discard data from a savegame.
	 */
	static void LoadEmpty();

	/**
	 * Suspends the script for the current tick and then pause the execution
	 * of script. The script will not be resumed from its suspended state
	 * until the script has been unpaused.
	 */
	void Pause();

	/**
	 * Checks if the script is paused.
	 * @return true if the script is paused, otherwise false
	 */
	bool IsPaused();

	/**
	 * Resume execution of the script. This function will not actually execute
	 * the script, but set a flag so that the script is executed my the usual
	 * mechanism that executes the script.
	 */
	void Unpause();

	/**
	 * Get the number of operations the script can execute before being suspended.
	 * This function is safe to call from within a function called by the script.
	 * @return The number of operations to execute.
	 */
	SQInteger GetOpsTillSuspend();

	/**
	 * DoCommand callback function for all commands executed by scripts.
	 * @param result The result of the command.
	 * @param tile The tile on which the command was executed.
	 * @param p1 p1 as given to DoCommandPInternal.
	 * @param p2 p2 as given to DoCommandPInternal.
	 */
	void DoCommandCallback(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2);

	/**
	 * Insert an event for this script.
	 * @param event The event to insert.
	 */
	void InsertEvent(class ScriptEvent *event);

	/**
	 * Check if the instance is sleeping, which either happened because the
	 *  script executed a DoCommand, executed this.Sleep() or it has been
	 *  paused.
	 */
	bool IsSleeping() { return this->suspend != 0; }

protected:
	class Squirrel *engine;               ///< A wrapper around the squirrel vm.
	const char *versionAPI;               ///< Current API used by this script.

	/**
	 * Register all API functions to the VM.
	 */
	virtual void RegisterAPI();

	/**
	 * Load squirrel scripts to emulate an older API.
	 * @param api_version: API version to load scripts for
	 * @param dir Subdirectory to find the scripts in
	 * @return true iff script loading should proceed
	 */
	bool LoadCompatibilityScripts(const char *api_version, Subdirectory dir);

	/**
	 * Tell the script it died.
	 */
	virtual void Died();

	/**
	 * Get the callback handling DoCommands in case of networking.
	 */
	virtual CommandCallback *GetDoCommandCallback() = 0;

	/**
	 * Load the dummy script.
	 */
	virtual void LoadDummyScript() = 0;

private:
	class ScriptController *controller;   ///< The script main class.
	class ScriptStorage *storage;         ///< Some global information for each running script.
	SQObject *instance;                   ///< Squirrel-pointer to the script main class.

	bool is_started;                      ///< Is the scripts constructor executed?
	bool is_dead;                         ///< True if the script has been stopped.
	bool is_save_data_on_stack;           ///< Is the save data still on the squirrel stack?
	int suspend;                          ///< The amount of ticks to suspend this script before it's allowed to continue.
	bool is_paused;                       ///< Is the script paused? (a paused script will not be executed until unpaused)
	Script_SuspendCallbackProc *callback; ///< Callback that should be called in the next tick the script runs.

	/**
	 * Call the script Load function if it exists and data was loaded
	 *  from a savegame.
	 */
	bool CallLoad();

	/**
	 * Save one object (int / string / array / table) to the savegame.
	 * @param vm The virtual machine to get all the data from.
	 * @param index The index on the squirrel stack of the element to save.
	 * @param max_depth The maximum depth recursive arrays / tables will be stored
	 *   with before an error is returned.
	 * @param test If true, don't really store the data but only check if it is
	 *   valid.
	 * @return True if the saving was successful.
	 */
	static bool SaveObject(HSQUIRRELVM vm, SQInteger index, int max_depth, bool test);

	/**
	 * Load all objects from a savegame.
	 * @return True if the loading was successful.
	 */
	static bool LoadObjects(HSQUIRRELVM vm);
};

#endif /* SCRIPT_INSTANCE_HPP */
