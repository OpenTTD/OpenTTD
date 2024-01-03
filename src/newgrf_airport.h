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
#include "timer/timer_game_calendar.h"
#include "newgrf_class.h"
#include "newgrf_commons.h"
#include "newgrf_spritegroup.h"
#include "newgrf_town.h"
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

	inline TileIterator& operator ++() override
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

	std::unique_ptr<TileIterator> Clone() const override
	{
		return std::make_unique<AirportTileTableIterator>(*this);
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
	TimerGameCalendar::Year min_year;      ///< first year the airport is available
	TimerGameCalendar::Year max_year;      ///< last year the airport is available
	StringID name;                         ///< name of this airport
	TTDPAirportType ttd_airport_type;      ///< ttdpatch airport type (Small/Large/Helipad/Oilrig)
	AirportClassID cls_id;                 ///< the class to which this airport type belongs
	SpriteID preview_sprite;               ///< preview sprite for this airport
	uint16_t maintenance_cost;               ///< maintenance cost multiplier
	/* Newgrf data */
	bool enabled;                          ///< Entity still available (by default true). Newgrf can disable it, though.
	struct GRFFileProps grf_prop;          ///< Properties related to the grf file.

	static const AirportSpec *Get(byte type);
	static AirportSpec *GetWithoutOverride(byte type);

	bool IsAvailable() const;
	bool IsWithinMapBounds(byte table, TileIndex index) const;

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

/** Resolver for the airport scope. */
struct AirportScopeResolver : public ScopeResolver {
	struct Station *st; ///< Station of the airport for which the callback is run, or \c nullptr for build gui.
	byte airport_id;    ///< Type of airport for which the callback is run.
	byte layout;        ///< Layout of the airport to build.
	TileIndex tile;     ///< Tile for the callback, only valid for airporttile callbacks.

	/**
	 * Constructor of the scope resolver for an airport.
	 * @param ro Surrounding resolver.
	 * @param tile %Tile for the callback, only valid for airporttile callbacks.
	 * @param st %Station of the airport for which the callback is run, or \c nullptr for build gui.
	 * @param airport_id Type of airport for which the callback is run.
	 * @param layout Layout of the airport to build.
	 */
	AirportScopeResolver(ResolverObject &ro, TileIndex tile, Station *st, byte airport_id, byte layout)
		: ScopeResolver(ro), st(st), airport_id(airport_id), layout(layout), tile(tile)
	{
	}

	uint32_t GetRandomBits() const override;
	uint32_t GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const override;
	void StorePSA(uint pos, int32_t value) override;
};


/** Resolver object for airports. */
struct AirportResolverObject : public ResolverObject {
	AirportScopeResolver airport_scope;
	std::unique_ptr<TownScopeResolver> town_scope; ///< The town scope resolver (created on the first call).

	AirportResolverObject(TileIndex tile, Station *st, byte airport_id, byte layout,
			CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0);

	TownScopeResolver *GetTown();

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override
	{
		switch (scope) {
			case VSG_SCOPE_SELF: return &this->airport_scope;
			case VSG_SCOPE_PARENT:
			{
				TownScopeResolver *tsr = this->GetTown();
				if (tsr != nullptr) return tsr;
				FALLTHROUGH;
			}
			default: return ResolverObject::GetScope(scope, relative);
		}
	}

	GrfSpecFeature GetFeature() const override;
	uint32_t GetDebugID() const override;
};

StringID GetAirportTextCallback(const AirportSpec *as, byte layout, uint16_t callback);

#endif /* NEWGRF_AIRPORT_H */
