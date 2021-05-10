/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_depotlist.hpp List all the depots (you own). */

#ifndef SCRIPT_DEPOTLIST_HPP
#define SCRIPT_DEPOTLIST_HPP

#include "script_list.hpp"
#include "script_tile.hpp"

/**
 * Creates a list of the locations of the depots (and hangars) of which you are the owner.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptDepotList : public ScriptList {
public:
	/**
	 * @param transport_type The type of transport to make a list of depots for.
	 */
	ScriptDepotList(ScriptTile::TransportType transport_type);
};

#endif /* SCRIPT_DEPOTLIST_HPP */
