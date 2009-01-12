/* $Id$ */

/** @file ai_depotlist.hpp List all the depots (you own). */

#ifndef AI_DEPOTLIST_HPP
#define AI_DEPOTLIST_HPP

#include "ai_abstractlist.hpp"
#include "ai_tile.hpp"

/**
 * Creates a list of the locations of the depots (and hangars) of which you are the owner.
 * @ingroup AIList
 */
class AIDepotList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIDepotList"; }

	/**
	 * @param transport_type The type of transport to make a list of depots for.
	 */
	AIDepotList(AITile::TransportType transport_type);
};

#endif /* AI_DEPOTLIST_HPP */
