/* $Id$ */

/** @file ai_railtypelist.hpp List all available railtypes. */

#ifndef AI_RAILTYPELIST_HPP
#define AI_RAILTYPELIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Creates a list of all available railtypes.
 * @ingroup AIList
 */
class AIRailTypeList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIRailTypeList"; }
	AIRailTypeList();
};

#endif /* AI_RAILTYPELIST_HPP */
