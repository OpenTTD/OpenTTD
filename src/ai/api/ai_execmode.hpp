/* $Id$ */

/** @file ai_execmode.hpp Switch the AI to Execute Mode. */

#ifndef AI_EXECMODE_HPP
#define AI_EXECMODE_HPP

#include "ai_object.hpp"

/**
 * Class to switch current mode to Execute Mode.
 * If you create an instance of this class, the mode will be switched to
 *   Execute. The original mode is stored and recovered from when ever the
 *   instance is destroyed.
 * In Execute mode all commands you do are executed for real.
 */
class AIExecMode : public AIObject {
public:
	static const char *GetClassName() { return "AIExecMode"; }

private:
	AIModeProc *last_mode;
	AIObject *last_instance;

protected:
	/**
	 * The callback proc for Execute mode.
	 */
	static bool ModeProc(TileIndex tile, uint32 p1, uint32 p2, uint procc, CommandCost costs);

public:
	/**
	 * Creating instance of this class switches the build mode to Execute.
	 * @note When the instance is destroyed, he restores the mode that was
	 *   current when the instance was created!
	 */
	AIExecMode();

	/**
	 * Destroying this instance reset the building mode to the mode it was
	 *   in when the instance was created.
	 */
	~AIExecMode();
};

#endif /* AI_EXECMODE_HPP */
