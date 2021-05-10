/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_tilelist.hpp List tiles. */

#ifndef SCRIPT_TILELIST_HPP
#define SCRIPT_TILELIST_HPP

#include "script_station.hpp"
#include "script_list.hpp"

/**
 * Creates an empty list, in which you can add tiles.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptTileList : public ScriptList {
public:
	/**
	 * Adds the rectangle between tile_from and tile_to to the to-be-evaluated tiles.
	 * @param tile_from One corner of the tiles to add.
	 * @param tile_to The other corner of the tiles to add.
	 * @pre ScriptMap::IsValidTile(tile_from).
	 * @pre ScriptMap::IsValidTile(tile_to).
	 */
	void AddRectangle(TileIndex tile_from, TileIndex tile_to);

	/**
	 * Add a tile to the to-be-evaluated tiles.
	 * @param tile The tile to add.
	 * @pre ScriptMap::IsValidTile(tile).
	 */
	void AddTile(TileIndex tile);

	/**
	 * Remove the tiles inside the rectangle between tile_from and tile_to form the list.
	 * @param tile_from One corner of the tiles to remove.
	 * @param tile_to The other corner of the files to remove.
	 * @pre ScriptMap::IsValidTile(tile_from).
	 * @pre ScriptMap::IsValidTile(tile_to).
	 */
	void RemoveRectangle(TileIndex tile_from, TileIndex tile_to);

	/**
	 * Remove a tile from the list.
	 * @param tile The tile to remove.
	 * @pre ScriptMap::IsValidTile(tile).
	 */
	void RemoveTile(TileIndex tile);
};

/**
 * Creates a list of tiles that will accept cargo for the given industry.
 * @note If a simular industry is close, it might happen that this industry receives the cargo.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptTileList_IndustryAccepting : public ScriptTileList {
public:
	/**
	 * @param industry_id The industry to create the ScriptTileList around.
	 * @param radius The coverage radius of the station type you will be using.
	 * @pre ScriptIndustry::IsValidIndustry(industry_id).
	 * @pre radius > 0.
	 * @note A station part built on any of the returned tiles will give you coverage.
	 */
	ScriptTileList_IndustryAccepting(IndustryID industry_id, int radius);
};

/**
 * Creates a list of tiles which the industry checks to see if a station is
 *  there to receive cargo produced by this industry.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptTileList_IndustryProducing : public ScriptTileList {
public:
	/**
	 * @param industry_id The industry to create the ScriptTileList around.
	 * @param radius The coverage radius of the station type you will be using.
	 * @pre ScriptIndustry::IsValidIndustry(industry_id).
	 * @pre radius > 0.
	 * @note A station part built on any of the returned tiles will give you acceptance.
	 */
	ScriptTileList_IndustryProducing(IndustryID industry_id, int radius);
};

/**
 * Creates a list of tiles which have the requested StationType of the
 *  StationID.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptTileList_StationType : public ScriptTileList {
public:
	/**
	 * @param station_id The station to create the ScriptTileList for.
	 * @param station_type The StationType to create the ScriptList for.
	 */
	ScriptTileList_StationType(StationID station_id, ScriptStation::StationType station_type);
};

#endif /* SCRIPT_TILELIST_HPP */
