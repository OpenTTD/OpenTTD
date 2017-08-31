/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_airport.h NewGRF handling of airports. */

#ifndef NEWGRF_AIRPORT_H
#define NEWGRF_AIRPORT_H

#include "airport.h"
#include "date_type.h"
#include "newgrf_class.h"
#include "newgrf_commons.h"
#include "tilearea_type.h"

/** Copy from station_map.h */
typedef byte StationGfx;

/** Tile-offset / AirportTileID pair. */
struct AirportTileTable {
	TileIndexDiffC ti; ///< Tile offset from  the top-most airport tile.
	StationGfx gfx;    ///< AirportTile to use for this tile.
};

/** Iterator to iterate over all tiles belonging to an airport spec. */
class AirportTileTableIterator : public TileIterator {
private:
	const AirportTileTable *att; ///< The offsets.
	TileIndex base_tile;         ///< The tile we base the offsets off.

public:
	/**
	 * Construct the iterator.
	 * @param att The TileTable we want to iterate over.
	 * @param base_tile The basetile for all offsets.
	 */
	AirportTileTableIterator(const AirportTileTable *att, TileIndex base_tile) : TileIterator(base_tile + ToTileIndexDiff(att->ti)), att(att), base_tile(base_tile)
	{
	}

	inline TileIterator& operator ++()
	{
		this->att++;
		if (this->att->ti.x == -0x80) {
			this->tile = INVALID_TILE;
		} else {
			this->tile = this->base_tile + ToTileIndexDiff(this->att->ti);
		}
		return *this;
	}

	/** Get the StationGfx for the current tile. */
	StationGfx GetStationGfx() const
	{
		return this->att->gfx;
	}

	virtual AirportTileTableIterator *Clone() const
	{
		return new AirportTileTableIterator(*this);
	}
};

/** List of default airport classes. */
enum AirportClassID {
	APC_BEGIN     = 0,  ///< Lowest valid airport class id
	APC_SMALL     = 0,  ///< id for small airports class
	APC_LARGE,          ///< id for large airports class
	APC_HUB,            ///< id for hub airports class
	APC_HELIPORT,       ///< id for heliports
	APC_MAX       = 16, ///< maximum number of airport classes
};

/** Allow incrementing of AirportClassID variables */
DECLARE_POSTFIX_INCREMENT(AirportClassID)

/** TTDP airport types. Used to map our types to TTDPatch's */
enum TTDPAirportType {
	ATP_TTDP_SMALL,    ///< Same as AT_SMALL
	ATP_TTDP_LARGE,    ///< Same as AT_LARGE
	ATP_TTDP_HELIPORT, ///< Same as AT_HELIPORT
	ATP_TTDP_OILRIG,   ///< Same as AT_OILRIG
};

/** A list of all hangar tiles in an airport */
struct HangarTileTable {
	TileIndexDiffC ti; ///< Tile offset from the top-most airport tile.
	Direction dir;     ///< Direction of the exit.
	byte hangar_num;   ///< The hangar to which this tile belongs.
};

/**
 * Defines the data structure for an airport.
 */
struct AirportSpec {
	const struct AirportFTAClass *fsm;     ///< the finite statemachine for the default airports
	const AirportTileTable * const *table; ///< list of the tiles composing the airport
	const Direction *rotation;             ///< the rotation of each tiletable
	byte num_table;                        ///< number of elements in the table
	const HangarTileTable *depot_table;    ///< gives the position of the depots on the airports
	byte nof_depots;                       ///< the number of hangar tiles in this airport
	byte size_x;                           ///< size of airport in x direction
	byte size_y;                           ///< size of airport in y direction
	byte noise_level;                      ///< noise that this airport generates
	byte catchment;                        ///< catchment area of this airport
	Year min_year;                         ///< first year the airport is available
	Year max_year;                         ///< last year the airport is available
	StringID name;                         ///< name of this airport
	TTDPAirportType ttd_airport_type;      ///< ttdpatch airport type (Small/Large/Helipad/Oilrig)
	AirportClassID cls_id;                 ///< the class to which this airport type belongs
	SpriteID preview_sprite;               ///< preview sprite for this airport
	uint16 maintenance_cost;               ///< maintenance cost multiplier
	/* Newgrf data */
	bool enabled;                          ///< Entity still available (by default true). Newgrf can disable it, though.
	struct GRFFileProps grf_prop;          ///< Properties related to the grf file.

	static const AirportSpec *Get(byte type);
	static AirportSpec *GetWithoutOverride(byte type);

	bool IsAvailable() const;

	static void ResetAirports();

	/** Get the index of this spec. */
	byte GetIndex() const
	{
		assert(this >= specs && this < endof(specs));
		return (byte)(this - specs);
	}

	static const AirportSpec dummy; ///< The dummy airport.

private:
	static AirportSpec specs[NUM_AIRPORTS]; ///< Specs of the airports.
};

/** Information related to airport classes. */
typedef NewGRFClass<AirportSpec, AirportClassID, APC_MAX> AirportClass;

void BindAirportSpecs();

StringID GetAirportTextCallback(const AirportSpec *as, byte layout, uint16 callback);

#endif /* NEWGRF_AIRPORT_H */
