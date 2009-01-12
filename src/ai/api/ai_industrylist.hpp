/* $Id$ */

/** @file ai_industrylist.hpp List all the industries. */

#ifndef AI_INDUSTRYLIST_HPP
#define AI_INDUSTRYLIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Creates a list of industries that are currently on the map.
 * @ingroup AIList
 */
class AIIndustryList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIIndustryList"; }
	AIIndustryList();
};

/**
 * Creates a list of industries that accepts a given cargo.
 * @ingroup AIList
 */
class AIIndustryList_CargoAccepting : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIIndustryList_CargoAccepting"; }

	/**
	 * @param cargo_id The cargo this industry should accept.
	 */
	AIIndustryList_CargoAccepting(CargoID cargo_id);
};

/**
 * Creates a list of industries that can produce a given cargo.
 * @note It also contains industries that currently produces 0 units of the cargo.
 * @ingroup AIList
 */
class AIIndustryList_CargoProducing : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIIndustryList_CargoProducing"; }

	/**
	 * @param cargo_id The cargo this industry should produce.
	 */
	AIIndustryList_CargoProducing(CargoID cargo_id);
};

#endif /* AI_INDUSTRYLIST_HPP */
