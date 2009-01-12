/* $Id$ */

/** @file ai_testmode.hpp Switch the AI to Test Mode. */

#ifndef AI_TESTMODE_HPP
#define AI_TESTMODE_HPP

#include "ai_object.hpp"

/**
 * Class to switch current mode to Test Mode.
 * If you create an instance of this class, the mode will be switched to
 *   Testing. The original mode is stored and recovered from when ever the
 *   instance is destroyed.
 * In Test mode all the commands you execute aren't really executed. The
 *   system only checks if it would be able to execute your requests, and what
 *   the cost would be.
 */
class AITestMode : public AIObject {
public:
	static const char *GetClassName() { return "AITestMode"; }

private:
	AIModeProc *last_mode;
	AIObject *last_instance;

protected:
	/**
	 * The callback proc for Testing mode.
	 */
	static bool ModeProc(TileIndex tile, uint32 p1, uint32 p2, uint procc, CommandCost costs);

public:
	/**
	 * Creating instance of this class switches the build mode to Testing.
	 * @note When the instance is destroyed, he restores the mode that was
	 *   current when the instance was created!
	 */
	AITestMode();

	/**
	 * Destroying this instance reset the building mode to the mode it was
	 *   in when the instance was created.
	 */
	~AITestMode();
};

#endif /* AI_TESTMODE_HPP */
