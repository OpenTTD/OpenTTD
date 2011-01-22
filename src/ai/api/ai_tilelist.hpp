/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_tilelist.hpp List tiles. */

#ifndef AI_TILELIST_HPP
#define AI_TILELIST_HPP

#include "ai_station.hpp"
#include "ai_list.hpp"

/**
 * Creates an empty list, in which you can add tiles.
 * @ingroup AIList
 */
class AITileList : public AIList {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AITileList"; }

	/**
	 * Adds the rectangle between tile_from and tile_to to the to-be-evaluated tiles.
	 * @param tile_from One corner of the tiles to add.
	 * @param tile_to The other corner of the tiles to add.
	 * @pre AIMap::IsValidTile(tile_from).
	 * @pre AIMap::IsValidTile(tile_to).
	 */
	void AddRectangle(TileIndex tile_from, TileIndex tile_to);

	/**
	 * Add a tile to the to-be-evaluated tiles.
	 * @param tile The tile to add.
	 * @pre AIMap::IsValidTile(tile).
	 */
	void AddTile(TileIndex tile);

	/**
	 * Remove the tiles inside the rectangle between tile_from and tile_to form the list.
	 * @param tile_from One corner of the tiles to remove.
	 * @param tile_to The other corner of the files to remove.
	 * @pre AIMap::IsValidTile(tile_from).
	 * @pre AIMap::IsValidTile(tile_to).
	 */
	void RemoveRectangle(TileIndex tile_from, TileIndex tile_to);

	/**
	 * Remove a tile from the list.
	 * @param tile The tile to remove.
	 * @pre AIMap::IsValidTile(tile).
	 */
	void RemoveTile(TileIndex tile);
};

/**
 * Creates a list of tiles that will accept cargo for the given industry.
 * @note If a simular industry is close, it might happen that this industry receives the cargo.
 * @ingroup AIList
 */
class AITileList_IndustryAccepting : public AITileList {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AITileList_IndustryAccepting"; }

	/**
	 * @param industry_id The industry to create the AITileList around.
	 * @param radius The radius of the station you will be using.
	 * @pre AIIndustry::IsValidIndustry(industry_id).
	 * @pre radius > 0.
	 */
	AITileList_IndustryAccepting(IndustryID industry_id, int radius);
};

/**
 * Creates a list of tiles which the industry checks to see if a station is
 *  there to receive cargo produced by this industry.
 * @ingroup AIList
 */
class AITileList_IndustryProducing : public AITileList {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AITileList_IndustryProducing"; }

	/**
	 * @param industry_id The industry to create the AITileList around.
	 * @param radius The radius of the station you will be using.
	 * @pre AIIndustry::IsValidIndustry(industry_id).
	 * @pre radius > 0.
	 */
	AITileList_IndustryProducing(IndustryID industry_id, int radius);
};

/**
 * Creates a list of tiles which have the requested StationType of the
 *  StationID.
 * @ingroup AIList
 */
class AITileList_StationType : public AITileList {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AITileList_StationType"; }

	/**
	 * @param station_id The station to create the AITileList for.
	 * @param station_type The StationType to create the AIList for.
	 */
	AITileList_StationType(StationID station_id, AIStation::StationType station_type);
};

#endif /* AI_TILELIST_HPP */
