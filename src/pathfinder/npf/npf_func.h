/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file npf_func.h Functions to access the new pathfinder. */

#ifndef NPF_FUNC_H
#define NPF_FUNC_H

#include "../../track_type.h"
#include "../../direction_type.h"
#include "../../vehicle_type.h"
#include "../pathfinder_type.h"

/**
 * Used when user sends road vehicle to the nearest depot or if road vehicle needs servicing using NPF.
 * @param v            vehicle that needs to go to some depot
 * @param max_penalty  max distance (in pathfinder penalty) from the current vehicle position
 *                     (used also as optimization - the pathfinder can stop path finding if max_penalty
 *                     was reached and no depot was seen)
 * @return             the data about the depot
 */
FindDepotData NPFRoadVehicleFindNearestDepot(const RoadVehicle *v, int max_penalty);

/**
 * Finds the best path for given road vehicle using NPF.
 * @param v         the RV that needs to find a path
 * @param tile      the tile to find the path from (should be next tile the RV is about to enter)
 * @param enterdir  diagonal direction which the RV will enter this new tile from
 * @param trackdirs available trackdirs on the new tile (to choose from)
 * @param path_found [out] Whether a path has been found (true) or has been guessed (false)
 * @return          the best trackdir for next turn or INVALID_TRACKDIR if the path could not be found
 */
Trackdir NPFRoadVehicleChooseTrack(const RoadVehicle *v, TileIndex tile, DiagDirection enterdir, TrackdirBits trackdirs, bool &path_found);

/**
 * Finds the best path for given ship using NPF.
 * @param v        the ship that needs to find a path
 * @param tile     the tile to find the path from (should be next tile the ship is about to enter)
 * @param enterdir diagonal direction which the ship will enter this new tile from
 * @param tracks   available tracks on the new tile (to choose from)
 * @param path_found [out] Whether a path has been found (true) or has been guessed (false)
 * @return         the best trackdir for next turn or INVALID_TRACK if the path could not be found
 */
Track NPFShipChooseTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found);

/**
 * Used when user sends train to the nearest depot or if train needs servicing using NPF
 * @param v            train that needs to go to some depot
 * @param max_penalty  max max_penalty (in pathfinder penalty) from the current train position
 *                     (used also as optimization - the pathfinder can stop path finding if max_penalty
 *                     was reached and no depot was seen)
 * @return             the data about the depot
 */
FindDepotData NPFTrainFindNearestDepot(const Train *v, int max_penalty);

/**
 * Try to extend the reserved path of a train to the nearest safe tile using NPF.
 *
 * @param v    The train that needs to find a safe tile.
 * @param tile Last tile of the current reserved path.
 * @param td   Last trackdir of the current reserved path.
 * @param override_railtype Should all physically compatible railtypes be searched, even if the vehicle can't run on them on its own?
 * @return True if the path could be extended to a safe tile.
 */
bool NPFTrainFindNearestSafeTile(const Train *v, TileIndex tile, Trackdir td, bool override_railtype);

/**
 * Returns true if it is better to reverse the train before leaving station using NPF.
 * @param v the train leaving the station
 * @return true if reversing is better
 */
bool NPFTrainCheckReverse(const Train *v);

/**
 * Finds the best path for given train using NPF.
 * @param v        the train that needs to find a path
 * @param tile     the tile to find the path from (should be next tile the train is about to enter)
 * @param enterdir diagonal direction which the RV will enter this new tile from
 * @param tracks   available trackdirs on the new tile (to choose from)
 * @param path_found [out] Whether a path has been found (true) or has been guessed (false)
 * @param reserve_track indicates whether YAPF should try to reserve the found path
 * @param target   [out] the target tile of the reservation, free is set to true if path was reserved
 * @return         the best track for next turn
 */
Track NPFTrainChooseTrack(const Train *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found, bool reserve_track, struct PBSTileInfo *target);

#endif /* NPF_FUNC_H */
