/* $Id$ */

/** @file ai_grouplist.hpp List all the groups (you own). */

#ifndef AI_GROUPLIST_HPP
#define AI_GROUPLIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Creates a list of groups of which you are the owner.
 * @note Neither AIGroup.ALL_GROUP nor AIGroup.DEFAULT_GROUP is in this list.
 * @ingroup AIList
 */
class AIGroupList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIGroupList"; }
	AIGroupList();
};

#endif /* AI_GROUPLIST_HPP */
