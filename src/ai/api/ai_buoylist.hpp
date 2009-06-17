/* $Id$ */

/** @file ai_buoylist.hpp List all the buoys. */

#ifndef AI_BUOYLIST_HPP
#define AI_BUOYLIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Creates a list of buoys.
 * @ingroup AIList
 */
class AIBuoyList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIBuoyList"; }
	AIBuoyList();
};


#endif /* AI_BUOYLIST_HPP */
