/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_node_ship.hpp Node tailored for ship pathfinding. */

#ifndef YAPF_NODE_SHIP_HPP
#define YAPF_NODE_SHIP_HPP

#include "../../tile_type.h"
#include "../../track_type.h"
#include "nodelist.hpp"
#include "yapf_node.hpp"

/** Yapf Node for ships */
template <class TKey>
struct CYapfShipNodeT : CYapfNodeT<TKey, CYapfShipNodeT<TKey>> {
};

/* now define two major node types (that differ by key type) */
typedef CYapfShipNodeT<CYapfNodeKeyExitDir>  CYapfShipNodeExitDir;
typedef CYapfShipNodeT<CYapfNodeKeyTrackDir> CYapfShipNodeTrackDir;

/* Default NodeList types */
typedef NodeList<CYapfShipNodeExitDir , 10, 12> CShipNodeListExitDir;
typedef NodeList<CYapfShipNodeTrackDir, 10, 12> CShipNodeListTrackDir;

#endif /* YAPF_NODE_SHIP_HPP */
