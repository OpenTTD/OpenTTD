/* $Id$ */

/** @file ai_tilelist.cpp Implementation of AITileList and friends. */

#include "ai_tilelist.hpp"
#include "ai_industry.hpp"
#include "../../tile_map.h"
#include "../../industry_map.h"
#include "../../station_map.h"
#include "../../settings_type.h"

void AITileList::FixRectangleSpan(TileIndex &t1, TileIndex &t2)
{
	uint x1 = ::TileX(t1);
	uint x2 = ::TileX(t2);

	uint y1 = ::TileY(t1);
	uint y2 = ::TileY(t2);

	if (x1 >= x2) ::Swap(x1, x2);
	if (y1 >= y2) ::Swap(y1, y2);

	t1 = ::TileXY(x1, y1);
	t2 = ::TileXY(x2, y2);
}

void AITileList::AddRectangle(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1)) return;
	if (!::IsValidTile(t2)) return;

	this->FixRectangleSpan(t1, t2);

	uint w = TileX(t2) - TileX(t1) + 1;
	uint h = TileY(t2) - TileY(t1) + 1;

	BEGIN_TILE_LOOP(t, w, h, t1) {
		this->AddItem(t);
	} END_TILE_LOOP(t, w, h, t1)
}

void AITileList::AddTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return;

	this->AddItem(tile);
}

void AITileList::RemoveRectangle(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1)) return;
	if (!::IsValidTile(t2)) return;

	this->FixRectangleSpan(t1, t2);

	uint w = TileX(t2) - TileX(t1) + 1;
	uint h = TileY(t2) - TileY(t1) + 1;

	BEGIN_TILE_LOOP(t, w, h, t1) {
		this->RemoveItem(t);
	} END_TILE_LOOP(t, w, h, t1)
}

void AITileList::RemoveTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return;

	this->RemoveItem(tile);
}

AITileList_IndustryAccepting::AITileList_IndustryAccepting(IndustryID industry_id, uint radius)
{
	if (!AIIndustry::IsValidIndustry(industry_id)) return;

	const Industry *i = ::GetIndustry(industry_id);

	/* Check if this industry accepts anything */
	{
		bool cargo_accepts = false;
		for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
			if (i->accepts_cargo[j] != CT_INVALID) cargo_accepts = true;
		}
		if (!cargo_accepts) return;
	}

	if (!_settings_game.station.modified_catchment) radius = CA_UNMODIFIED;

	BEGIN_TILE_LOOP(cur_tile, i->width + radius * 2, i->height + radius * 2, i->xy - ::TileDiffXY(radius, radius)) {
		if (!::IsValidTile(cur_tile)) continue;
		/* Exclude all tiles that belong to this industry */
		if (::IsTileType(cur_tile, MP_INDUSTRY) && ::GetIndustryIndex(cur_tile) == industry_id) continue;

		/* Only add the tile if it accepts the cargo (sometimes just 1 tile of an
		 *  industry triggers the acceptance). */
		AcceptedCargo accepts;
		::GetAcceptanceAroundTiles(accepts, cur_tile, 1, 1, radius);
		{
			bool cargo_accepts = false;
			for (byte j = 0; j < lengthof(i->accepts_cargo); j++) {
				if (i->accepts_cargo[j] != CT_INVALID && accepts[i->accepts_cargo[j]] != 0) cargo_accepts = true;
			}
			if (!cargo_accepts) continue;
		}

		this->AddTile(cur_tile);
	} END_TILE_LOOP(cur_tile, i->width + radius * 2, i->height + radius * 2, i->xy - ::TileDiffXY(radius, radius))
}

AITileList_IndustryProducing::AITileList_IndustryProducing(IndustryID industry_id, uint radius)
{
	if (!AIIndustry::IsValidIndustry(industry_id)) return;

	const Industry *i = ::GetIndustry(industry_id);

	/* Check if this industry produces anything */
	{
		bool cargo_produces = false;
		for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
			if (i->produced_cargo[j] != CT_INVALID) cargo_produces = true;
		}
		if (!cargo_produces) return;
	}

	if (!_settings_game.station.modified_catchment) radius = CA_UNMODIFIED;

	BEGIN_TILE_LOOP(cur_tile, i->width + radius * 2, i->height + radius * 2, i->xy - ::TileDiffXY(radius, radius)) {
		if (!::IsValidTile(cur_tile)) continue;
		/* Exclude all tiles that belong to this industry */
		if (::IsTileType(cur_tile, MP_INDUSTRY) && ::GetIndustryIndex(cur_tile) == industry_id) continue;

		/* Only add the tile if it produces the cargo (a bug in OpenTTD makes this
		 *  inconsitance). */
		AcceptedCargo produces;
		::GetProductionAroundTiles(produces, cur_tile, 1, 1, radius);
		{
			bool cargo_produces = false;
			for (byte j = 0; j < lengthof(i->produced_cargo); j++) {
				if (i->produced_cargo[j] != CT_INVALID && produces[i->produced_cargo[j]] != 0) cargo_produces = true;
			}
			if (!cargo_produces) continue;
		}

		this->AddTile(cur_tile);
	} END_TILE_LOOP(cur_tile, i->width + radius * 2, i->height + radius * 2, i->xy - ::TileDiffXY(radius, radius))
}

AITileList_StationType::AITileList_StationType(StationID station_id, AIStation::StationType station_type)
{
	if (!AIStation::IsValidStation(station_id)) return;

	const StationRect *rect = &::GetStation(station_id)->rect;

	uint station_type_value = 0;
	/* Convert AIStation::StationType to ::StationType, but do it in a
	 *  bitmask, so we can scan for multiple entries at the same time. */
	if ((station_type & AIStation::STATION_TRAIN) != 0)      station_type_value |= (1 << ::STATION_RAIL);
	if ((station_type & AIStation::STATION_TRUCK_STOP) != 0) station_type_value |= (1 << ::STATION_TRUCK);
	if ((station_type & AIStation::STATION_BUS_STOP) != 0)   station_type_value |= (1 << ::STATION_BUS);
	if ((station_type & AIStation::STATION_AIRPORT) != 0)    station_type_value |= (1 << ::STATION_AIRPORT) | (1 << ::STATION_OILRIG);
	if ((station_type & AIStation::STATION_DOCK) != 0)       station_type_value |= (1 << ::STATION_DOCK)    | (1 << ::STATION_OILRIG);

	BEGIN_TILE_LOOP(cur_tile, rect->right - rect->left + 1, rect->bottom - rect->top + 1, ::TileXY(rect->left, rect->top)) {
		if (!::IsTileType(cur_tile, MP_STATION)) continue;
		if (::GetStationIndex(cur_tile) != station_id) continue;
		if (!HasBit(station_type_value, ::GetStationType(cur_tile))) continue;
		this->AddTile(cur_tile);
	} END_TILE_LOOP(cur_tile, rect->right - rect->left + 1, rect->bottom - rect->top + 1, ::TileXY(rect->left, rect->top))
}
