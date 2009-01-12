/* $Id$ */

/** @file ai_cargolist.hpp List all the cargos. */

#ifndef AI_CARGOLIST_HPP
#define AI_CARGOLIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Creates a list of cargos that can be produced in the current game.
 * @ingroup AIList
 */
class AICargoList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AICargoList"; }
	AICargoList();
};

/**
 * Creates a list of cargos that the given industry accepts.
 * @ingroup AIList
 */
class AICargoList_IndustryAccepting : public AIAbstractList {
public:
	static const char *GetClassName() { return "AICargoList_IndustryAccepting"; }

	/**
	 * @param industry_id The industry to get the list of cargos it accepts from.
	 */
	AICargoList_IndustryAccepting(IndustryID industry_id);
};

/**
 * Creates a list of cargos that the given industry can produce.
 * @ingroup AIList
 */
class AICargoList_IndustryProducing : public AIAbstractList {
public:
	static const char *GetClassName() { return "AICargoList_IndustryProducing"; }

	/**
	 * @param industry_id The industry to get the list of cargos it produces from.
	 */
	AICargoList_IndustryProducing(IndustryID industry_id);
};

#endif /* AI_CARGOLIST_HPP */
