/* $Id$ */

/** @file ai_instance.hpp The AIInstance tracks an AI. */

#ifndef AI_INSTANCE_HPP
#define AI_INSTANCE_HPP

/**
 * The callback function when an AI suspends.
 */
typedef void (AISuspendCallbackProc)(class AIInstance *instance);

/**
 * A throw-class that is given when the VM wants to suspend.
 */
class AI_VMSuspend {
public:
	AI_VMSuspend(int time, AISuspendCallbackProc *callback) :
		time(time),
		callback(callback)
		{}

	int GetSuspendTime() { return time; }
	AISuspendCallbackProc *GetSuspendCallback() { return callback; }

private:
	int time;
	AISuspendCallbackProc *callback;
};

class AIInstance {
public:
	AIInstance(class AIInfo *info);
	~AIInstance();

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
	void CollectGarbage();

	/**
	 * Get the storage of this AI.
	 */
	static class AIStorage *GetStorage();

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
	class AIController *GetController() { return controller; }

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
	 * Call the AI Load function if it exists and data was loaded
	 *  from a savegame.
	 */
	bool CallLoad();

	/**
	 * Load and discard data from a savegame.
	 */
	static void LoadEmpty();

private:
	static class AIInstance *current_instance; //!< Static current AIInstance, so we can register AIs.

	class AIController *controller;
	class AIStorage *storage;
	class Squirrel *engine;
	SQObject *instance;

	bool is_started;
	bool is_dead;
	int suspend;
	AISuspendCallbackProc *callback;

	/**
	 * Register all API functions to the VM.
	 */
	void RegisterAPI();

	/**
	 * Tell the AI it died.
	 */
	void Died();

	/**
	 * Save one object (int / string / arrray / table) to the savegame.
	 * @param index The index on the squirrel stack of the element to save.
	 * @param max_depth The maximum depth recursive arrays / tables will be stored
	 *   with before an error is returned.
	 * @param test If true, don't really store the data but only check if it is
	 *   valid.
	 * @return True if the saving was successfull.
	 */
	static bool SaveObject(HSQUIRRELVM vm, SQInteger index, int max_depth, bool test);

	/**
	 * Load all objects from a savegame.
	 * @return True if the loading was successfull.
	 */
	static bool LoadObjects(HSQUIRRELVM vm);
};

#endif /* AI_INSTANCE_HPP */
