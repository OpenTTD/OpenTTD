/* $Id$ */

/** @file ai_townlist.hpp List all the towns. */

#ifndef AI_TOWNLIST_HPP
#define AI_TOWNLIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Creates a list of towns that are currently on the map.
 * @ingroup AIList
 */
class AITownList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AITownList"; }
	AITownList();
};

#endif /* AI_TOWNLIST_HPP */
