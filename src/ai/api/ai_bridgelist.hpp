/* $Id$ */

/** @file ai_bridgelist.hpp List all the bridges. */

#ifndef AI_BRIDGELIST_HPP
#define AI_BRIDGELIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Create a list of bridges.
 * @ingroup AIList
 */
class AIBridgeList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIBridgeList"; }
	AIBridgeList();
};

/**
 * Create a list of bridges that can be built on a specific length.
 * @ingroup AIList
 */
class AIBridgeList_Length : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIBridgeList_Length"; }

	/**
	 * @param length The length of the bridge you want to build.
	 */
	AIBridgeList_Length(uint length);
};

#endif /* AI_BRIDGELIST_HPP */
