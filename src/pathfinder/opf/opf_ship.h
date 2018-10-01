/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file opf_ship.h Original pathfinder for ships; very simple. */

#ifndef OPF_SHIP_H
#define OPF_SHIP_H

#include "../../direction_type.h"
#include "../../tile_type.h"
#include "../../track_type.h"
#include "../../vehicle_type.h"

/**
 * Finds the best path for given ship using OPF.
 * @param v        the ship that needs to find a path
 * @param tile     the tile to find the path from (should be next tile the ship is about to enter)
 * @param enterdir diagonal direction which the ship will enter this new tile from
 * @param tracks   available tracks on the new tile (to choose from)
 * @param path_found [out] Whether a path has been found (true) or has been guessed (false)
 * @return         the best trackdir for next turn or INVALID_TRACK if the path could not be found
 */
Track OPFShipChooseTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found);

/**
 * Used when user sends ship to the nearest depot or if ship needs servicing using OPF.
 * @param v            vehicle that needs to go to some depot
 * @param max_distance max distance (in pathfinder penalty) from the current ship position
 *                     (used also as optimization - the pathfinder can stop path finding if max_penalty
 *                     was reached and no depot was seen)
 * @return             the data about the depot
 */
FindDepotData OPFShipFindNearestDepot(const Ship *v, int max_distance);

#endif /* OPF_SHIP_H */
