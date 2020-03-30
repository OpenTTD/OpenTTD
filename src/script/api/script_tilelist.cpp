/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_tilelist.cpp Implementation of ScriptTileList and friends. */

#include "../../stdafx.h"
#include "script_tilelist.hpp"
#include "script_industry.hpp"
#include "../../industry.h"
#include "../../station_base.h"

#include "../../safeguards.h"

void ScriptTileList::AddRectangle(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1)) return;
	if (!::IsValidTile(t2)) return;

	TileArea ta(t1, t2);
	TILE_AREA_LOOP(t, ta) this->AddItem(t);
}

void ScriptTileList::AddTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return;

	this->AddItem(tile);
}

void ScriptTileList::RemoveRectangle(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1)) return;
	if (!::IsValidTile(t2)) return;

	TileArea ta(t1, t2);
	TILE_AREA_LOOP(t, ta) this->RemoveItem(t);
}

void ScriptTileList::RemoveTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return;

	this->RemoveItem(tile);
}

/**
 * Helper to get list of tiles that will cover an industry's production or acceptance.
 * @param i Industry in question
 * @param radius Catchment radius to test
 * @param bta BitmapTileArea to fill
 */
static void FillIndustryCatchment(const Industry *i, int radius, BitmapTileArea &bta)
{
	TILE_AREA_LOOP(cur_tile, i->location) {
		if (!::IsTileType(cur_tile, MP_INDUSTRY) || ::GetIndustryIndex(cur_tile) != i->index) continue;

		int tx = TileX(cur_tile);
		int ty = TileY(cur_tile);
		for (int y = -radius; y <= radius; y++) {
			if (ty + y < 0 || ty + y > (int)MapMaxY()) continue;
			for (int x = -radius; x <= radius; x++) {
				if (tx + x < 0 || tx + x > (int)MapMaxX()) continue;
				TileIndex tile = TileXY(tx + x, ty + y);
				if (!IsValidTile(tile)) continue;
				if (::IsTileType(tile, MP_INDUSTRY) && ::GetIndustryIndex(tile) == i->index) continue;
				bta.SetTile(tile);
			}
		}
	}
}

ScriptTileList_IndustryAccepting::ScriptTileList_IndustryAccepting(IndustryID industry_id, int radius)
{
	if (!ScriptIndustry::IsValidIndustry(industry_id) || radius <= 0) return;

	const Industry *i = ::Industry::Get(industry_id);

	/* Check if this industry is only served by its neutral station */
	if (i->neutral_station != nullptr && !_settings_game.station.serve_neutral_industries) return;

	/* Check if this industry accepts anything */
	{
		bool cargo_accepts = false;
		for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
			if (i->accepts_cargo[j] != CT_INVALID) cargo_accepts = true;
		}
		if (!cargo_accepts) return;
	}

	if (!_settings_game.station.modified_catchment) radius = CA_UNMODIFIED;

	BitmapTileArea bta(TileArea(i->location).Expand(radius));
	FillIndustryCatchment(i, radius, bta);

	BitmapTileIterator it(bta);
	for (TileIndex cur_tile = it; cur_tile != INVALID_TILE; cur_tile = ++it) {
		/* Only add the tile if it accepts the cargo (sometimes just 1 tile of an
		 *  industry triggers the acceptance). */
		CargoArray acceptance = ::GetAcceptanceAroundTiles(cur_tile, 1, 1, radius);
		{
			bool cargo_accepts = false;
			for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
				if (i->accepts_cargo[j] != CT_INVALID && acceptance[i->accepts_cargo[j]] != 0) cargo_accepts = true;
			}
			if (!cargo_accepts) continue;
		}

		this->AddTile(cur_tile);
	}
}

ScriptTileList_IndustryProducing::ScriptTileList_IndustryProducing(IndustryID industry_id, int radius)
{
	if (!ScriptIndustry::IsValidIndustry(industry_id) || radius <= 0) return;

	const Industry *i = ::Industry::Get(industry_id);

	/* Check if this industry is only served by its neutral station */
	if (i->neutral_station != nullptr && !_settings_game.station.serve_neutral_industries) return;

	/* Check if this industry produces anything */
	bool cargo_produces = false;
	for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
		if (i->produced_cargo[j] != CT_INVALID) cargo_produces = true;
	}
	if (!cargo_produces) return;

	if (!_settings_game.station.modified_catchment) radius = CA_UNMODIFIED;

	BitmapTileArea bta(TileArea(i->location).Expand(radius));
	FillIndustryCatchment(i, radius, bta);

	BitmapTileIterator it(bta);
	for (TileIndex cur_tile = it; cur_tile != INVALID_TILE; cur_tile = ++it) {
		this->AddTile(cur_tile);
	}
}

ScriptTileList_StationType::ScriptTileList_StationType(StationID station_id, ScriptStation::StationType station_type)
{
	if (!ScriptStation::IsValidStation(station_id)) return;

	const StationRect *rect = &::Station::Get(station_id)->rect;

	uint station_type_value = 0;
	/* Convert ScriptStation::StationType to ::StationType, but do it in a
	 *  bitmask, so we can scan for multiple entries at the same time. */
	if ((station_type & ScriptStation::STATION_TRAIN) != 0)      station_type_value |= (1 << ::STATION_RAIL);
	if ((station_type & ScriptStation::STATION_TRUCK_STOP) != 0) station_type_value |= (1 << ::STATION_TRUCK);
	if ((station_type & ScriptStation::STATION_BUS_STOP) != 0)   station_type_value |= (1 << ::STATION_BUS);
	if ((station_type & ScriptStation::STATION_AIRPORT) != 0)    station_type_value |= (1 << ::STATION_AIRPORT) | (1 << ::STATION_OILRIG);
	if ((station_type & ScriptStation::STATION_DOCK) != 0)       station_type_value |= (1 << ::STATION_DOCK)    | (1 << ::STATION_OILRIG);

	TileArea ta(::TileXY(rect->left, rect->top), rect->right - rect->left + 1, rect->bottom - rect->top + 1);
	TILE_AREA_LOOP(cur_tile, ta) {
		if (!::IsTileType(cur_tile, MP_STATION)) continue;
		if (::GetStationIndex(cur_tile) != station_id) continue;
		if (!HasBit(station_type_value, ::GetStationType(cur_tile))) continue;
		this->AddTile(cur_tile);
	}
}
