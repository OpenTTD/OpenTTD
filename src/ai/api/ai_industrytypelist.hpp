/* $Id$ */

/** @file ai_industrytypelist.hpp List all available industry types. */

#ifndef AI_INDUSTRYTYPELIST_HPP
#define AI_INDUSTRYTYPELIST_HPP

#include "ai_abstractlist.hpp"
#include "ai_industrytype.hpp"

/**
 * Creates a list of valid industry types.
 * @ingroup AIList
 */
class AIIndustryTypeList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIIndustryTypeList"; }
	AIIndustryTypeList();
};


#endif /* AI_INDUSTRYTYPELIST_HPP */
