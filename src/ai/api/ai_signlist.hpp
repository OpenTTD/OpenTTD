/* $Id$ */

/** @file ai_signlist.hpp List all the signs of your company. */

#ifndef AI_SIGNLIST_HPP
#define AI_SIGNLIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Create a list of signs your company has created.
 * @ingroup AIList
 */
class AISignList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AISignList"; }
	AISignList();
};

#endif /* AI_SIGNLIST_HPP */
