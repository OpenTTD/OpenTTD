/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_instance.hpp The AIInstance tracks an AI. */

#ifndef AI_INSTANCE_HPP
#define AI_INSTANCE_HPP

#include <squirrel.h>

/**
 * The callback function when an AI suspends.
 */
typedef void (AISuspendCallbackProc)(class AIInstance *instance);

/**
 * A throw-class that is given when the VM wants to suspend.
 */
class AI_VMSuspend {
public:
	/**
	 * Create the suspend exception.
	 * @param time The amount of ticks to suspend.
	 * @param callback The callback to call when the AI may resume again.
	 */
	AI_VMSuspend(int time, AISuspendCallbackProc *callback) :
		time(time),
		callback(callback)
	{}

	/**
	 * Get the amount of ticks the AI should be suspended.
	 * @return The amount of AI ticks to suspend the AI.
	 */
	int GetSuspendTime() { return time; }

	/**
	 * Get the callback to call when the AI can run again.
	 * @return The callback function to run.
	 */
	AISuspendCallbackProc *GetSuspendCallback() { return callback; }

private:
	int time;                        ///< Amount of ticks to suspend the AI.
	AISuspendCallbackProc *callback; ///< Callback function to call when the AI can run again.
};

/** Runtime information about an AI like a pointer to the squirrel vm and the current state. */
class AIInstance {
public:
	friend class ScriptObject;
	friend class ScriptController;

	/**
	 * Create a new AI.
	 */
	AIInstance();
	~AIInstance();

	/**
	 * Initialize the AI and prepare it for its first run.
	 * @param info The AI to create the instance of.
	 */
	void Initialize(class AIInfo *info);

	/**
	 * An AI in multiplayer waits for the server to handle his DoCommand.
	 *  It keeps waiting for this until this function is called.
	 */
	void Continue();

	/**
	 * Run the GameLoop of an AI.
	 */
	void GameLoop();

	/**
	 * Let the VM collect any garbage.
	 */
	void CollectGarbage() const;

	/**
	 * Get the storage of this AI.
	 */
	class ScriptStorage *GetStorage();

	/**
	 * Get the log pointer of this AI.
	 */
	void *GetLogPointer();

	/**
	 * Return a true/false reply for a DoCommand.
	 */
	static void DoCommandReturn(AIInstance *instance);

	/**
	 * Return a VehicleID reply for a DoCommand.
	 */
	static void DoCommandReturnVehicleID(AIInstance *instance);

	/**
	 * Return a SignID reply for a DoCommand.
	 */
	static void DoCommandReturnSignID(AIInstance *instance);

	/**
	 * Return a GroupID reply for a DoCommand.
	 */
	static void DoCommandReturnGroupID(AIInstance *instance);

	/**
	 * Get the controller attached to the instance.
	 */
	class ScriptController *GetController() { return controller; }

	/**
	 * Return the "this AI died" value
	 */
	inline bool IsDead() const { return this->is_dead; }

	/**
	 * Call the AI Save function and save all data in the savegame.
	 */
	void Save();

	/**
	 * Don't save any data in the savegame.
	 */
	static void SaveEmpty();

	/**
	 * Load data from a savegame and store it on the stack.
	 * @param version The version of the AI when saving, or -1 if this was
	 *  not the original AI saving the game.
	 */
	void Load(int version);

	/**
	 * Load and discard data from a savegame.
	 */
	static void LoadEmpty();

	/**
	 * Reduces the number of opcodes the AI have left to zero. Unless
	 * the AI is in a state where it cannot suspend it will be suspended
	 * for the reminder of the current tick. This function is safe to
	 * call from within a function called by the AI.
	 */
	void Suspend();

	/**
	 * Get the number of operations the AI can execute before being suspended.
	 * This function is safe to call from within a function called by the AI.
	 * @return The number of operations to execute.
	 */
	SQInteger GetOpsTillSuspend();

	/**
	 * DoCommand callback function for all commands executed by AIs.
	 * @param result The result of the command.
	 * @param tile The tile on which the command was executed.
	 * @param p1 p1 as given to DoCommandPInternal.
	 * @param p2 p2 as given to DoCommandPInternal.
	 */
	void DoCommandCallback(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2);

	/**
	 * Insert an event for this AI.
	 * @param event The event to insert.
	 */
	void InsertEvent(class ScriptEvent *event);

private:
	class ScriptController *controller; ///< The AI main class.
	class ScriptStorage *storage;       ///< Some global information for each running AI.
	class Squirrel *engine;             ///< A wrapper around the squirrel vm.
	SQObject *instance;                 ///< Squirrel-pointer to the AI main class.

	bool is_started;                    ///< Is the AIs constructor executed?
	bool is_dead;                       ///< True if the AI has been stopped.
	bool is_save_data_on_stack;         ///< Is the save data still on the squirrel stack?
	int suspend;                        ///< The amount of ticks to suspend this AI before it's allowed to continue.
	AISuspendCallbackProc *callback;    ///< Callback that should be called in the next tick the AI runs.

	/**
	 * Register all API functions to the VM.
	 */
	void RegisterAPI();

	/**
	 * Load squirrel scripts to emulate an older API.
	 */
	bool LoadCompatibilityScripts(const char *api_version);

	/**
	 * Tell the AI it died.
	 */
	void Died();

	/**
	 * Call the AI Load function if it exists and data was loaded
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

#endif /* AI_INSTANCE_HPP */
