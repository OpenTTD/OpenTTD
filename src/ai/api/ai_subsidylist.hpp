/* $Id$ */

/** @file ai_subsidylist.hpp List all the subsidies. */

#ifndef AI_SUBSIDYLIST_HPP
#define AI_SUBSIDYLIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Creates a list of all current subsidies.
 * @ingroup AIList
 */
class AISubsidyList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AISubsidyList"; }
	AISubsidyList();
};

#endif /* AI_SUBSIDYLIST_HPP */
