/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_depotlist.hpp List all the depots (you own). */

#ifndef AI_DEPOTLIST_HPP
#define AI_DEPOTLIST_HPP

#include "ai_list.hpp"
#include "ai_tile.hpp"

/**
 * Creates a list of the locations of the depots (and hangars) of which you are the owner.
 * @ingroup AIList
 */
class AIDepotList : public AIList {
public:
	/**
	 * @param transport_type The type of transport to make a list of depots for.
	 */
	AIDepotList(AITile::TransportType transport_type);
};

#endif /* AI_DEPOTLIST_HPP */
