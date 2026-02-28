/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tile_cmd.h Generic 'commands' that can be performed on all tiles. */

#ifndef TILE_CMD_H
#define TILE_CMD_H

#include "core/enum_type.hpp"
#include "core/geometry_type.hpp"
#include "command_type.h"
#include "vehicle_type.h"
#include "cargo_type.h"
#include "track_type.h"
#include "tile_map.h"
#include "timer/timer_game_calendar.h"

/** Flags to describe several special states upon entering a tile. */
enum class VehicleEnterTileState : uint8_t {
	EnteredStation, ///< The vehicle entered a station
	EnteredWormhole, ///< The vehicle either entered a bridge, tunnel or depot tile (this includes the last tile of the bridge/tunnel)
	CannotEnter, ///< The vehicle cannot enter the tile
};

using VehicleEnterTileStates = EnumBitSet<VehicleEnterTileState, uint8_t>;

/** Tile information, used while rendering the tile */
struct TileInfo : Coord3D<int> {
	Slope tileh;    ///< Slope of the tile
	TileIndex tile; ///< Tile index
};

/** Tile description for the 'land area information' tool */
struct TileDesc {
	StringID str{}; ///< Description of the tile
	uint64_t dparam = 0; ///< Parameter of the \a str string
	std::array<Owner, 4> owner{}; ///< Name of the owner(s)
	std::array<StringID, 4> owner_type{}; ///< Type of each owner
	TimerGameCalendar::Date build_date = CalendarTime::INVALID_DATE; ///< Date of construction of tile contents
	StringID station_class{}; ///< Class of station
	StringID station_name{}; ///< Type of station within the class
	StringID airport_class{}; ///< Name of the airport class
	StringID airport_name{}; ///< Name of the airport
	StringID airport_tile_name{}; ///< Name of the airport tile
	std::optional<std::string> grf = std::nullopt; ///< newGRF used for the tile contents
	StringID railtype{}; ///< Type of rail on the tile.
	uint16_t rail_speed = 0; ///< Speed limit of rail (bridges and track)
	StringID roadtype{}; ///< Type of road on the tile.
	uint16_t road_speed = 0; ///< Speed limit of road (bridges and track)
	StringID tramtype{}; ///< Type of tram on the tile.
	uint16_t tram_speed = 0; ///< Speed limit of tram (bridges and track)
	std::optional<bool> town_can_upgrade = std::nullopt; ///< Whether the town can upgrade this house during town growth.
};

/**
 * Tile callback function signature for drawing a tile and its contents to the screen
 * @param ti Information about the tile to draw
 */
using DrawTileProc = void(TileInfo *ti);

/**
 * Tile callback function signature for obtaining the world \c Z coordinate of a given
 * point of a tile.
 *
 * @param tile The queries tile for the Z coordinate.
 * @param x World X coordinate in tile "units".
 * @param y World Y coordinate in tile "units".
 * @param ground_vehicle Whether to get the Z coordinate of the ground vehicle, or the ground.
 * @return World Z coordinate at tile ground (vehicle) level, including slopes and foundations.
 * @see GetSlopePixelZ
 */
using GetSlopePixelZProc = int(TileIndex tile, uint x, uint y, bool ground_vehicle);

/**
 * Tile callback function signature for clearing a tile.
 * @param tile The tile to clear.
 * @param flags The command flags.
 * @return The cost or error.
 * @see ClearTile
 */
using ClearTileProc = CommandCost(TileIndex tile, DoCommandFlags flags);

/**
 * Tile callback function signature for obtaining cargo acceptance of a tile
 * @param tile            Tile queried for its accepted cargo
 * @param acceptance      Storage destination of the cargo acceptance in 1/8
 * @param always_accepted Bitmask of always accepted cargo types
 * @see AddAcceptedCargo
 */
using AddAcceptedCargoProc = void(TileIndex tile, CargoArray &acceptance, CargoTypes &always_accepted);

/**
 * Tile callback function signature for obtaining a tile description
 * @param tile Tile being queried
 * @param td   Storage pointer for returned tile description
 * @see GetTileDesc
 */
using GetTileDescProc = void(TileIndex tile, TileDesc &td);

/**
 * Tile callback function signature for getting the possible tracks
 * that can be taken on a given tile by a given transport.
 *
 * The return value contains the existing trackdirs and signal states.
 *
 * see track_func.h for usage of TrackStatus.
 *
 * @param tile     the tile to get the track status from
 * @param mode     the mode of transportation
 * @param sub_mode used to differentiate between different kinds within the mode
 * @param side The side where the tile is entered.
 * @return the track status information
 * @see GetTileTrackStatus
 */
using GetTileTrackStatusProc = TrackStatus(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side);

/**
 * Tile callback function signature for obtaining the produced cargo of a tile.
 * @param tile      Tile being queried
 * @param produced  Destination array for produced cargo
 * @see AddProducedCargo
 */
using AddProducedCargoProc = void(TileIndex tile, CargoArray &produced);

/**
 * Tile callback function signature for clicking a tile.
 * @param tile The tile that was clicked.
 * @return Whether any action was taken.
 * @see ClickTile
 */
using ClickTileProc = bool(TileIndex tile);

/**
 * Tile callback function signature for animating a tile.
 * @param tile The tile to animate.
 * @see AnimateTile
 */
using AnimateTileProc = void(TileIndex tile);

/**
 * Tile callback function signature for running periodic tile updates.
 * @param tile The tile to update.
 * @see RunTileLoop
 */
using TileLoopProc = void(TileIndex tile);

/**
 * Tile callback function signature for changing the owner of a tile.
 * @param tile The tile to process.
 * @param old_owner The owner to replace.
 * @param new_owner The owner to replace with.
 * @see ChangeTileOwner
 */
using ChangeTileOwnerProc = void(TileIndex tile, Owner old_owner, Owner new_owner);

/**
 * Tile callback function for a vehicle entering a tile.
 * @param v Vehicle entering the tile.
 * @param tile Tile entered.
 * @param x X position in world coordinates.
 * @param y Y position in world coordinates.
 * @return Some meta-data over the to be entered tile.
 * @see VehicleEnterTile
 */
using VehicleEnterTileProc = VehicleEnterTileStates(Vehicle *v, TileIndex tile, int x, int y);

/**
 * Tile callback function signature for getting the foundation of a tile.
 * @param tile The tile to check.
 * @param tileh The current slope.
 * @return The foundation that will be used.
 */
using GetFoundationProc = Foundation(TileIndex tile, Slope tileh);

/**
 * Tile callback function signature of the terraforming callback.
 *
 * The function is called when a tile is affected by a terraforming operation.
 * It has to check if terraforming of the tile is allowed and return extra terraform-cost that depend on the tiletype.
 * With DoCommandFlag::Execute in \a flags it has to perform tiletype-specific actions (like clearing land etc., but not the terraforming itself).
 *
 * @note The terraforming has not yet taken place. So GetTileZ() and GetTileSlope() refer to the landscape before the terraforming operation.
 *
 * @param tile      The involved tile.
 * @param flags     Command flags passed to the terraform command (DoCommandFlag::Execute, DoCommandFlag::QueryCost, etc.).
 * @param z_new     TileZ after terraforming.
 * @param tileh_new Slope after terraforming.
 * @return Error code or extra cost for terraforming (like clearing land, building foundations, etc., but not the terraforming itself.)
 * @see TerraformTile
 */
using TerraformTileProc = CommandCost(TileIndex tile, DoCommandFlags flags, int z_new, Slope tileh_new);

/**
 * Tile callback function signature to test if a bridge can be built above a tile.
 * @param tile The involved tile.
 * @param flags Command flags passed to the build command.
 * @param axis Axis of bridge being built.
 * @param height Absolute height of bridge platform.
 * @return Error code or extra cost for building bridge above the tile.
 * @see CheckBuildAbove
 */
using CheckBuildAboveProc = CommandCost(TileIndex tile, DoCommandFlags flags, Axis axis, int height);

/**
 * Set of callback functions for performing tile operations of a given tile type.
 * @see TileType
 */
struct TileTypeProcs {
	DrawTileProc *draw_tile_proc; ///< Called to render the tile and its contents to the screen.
	GetSlopePixelZProc *get_slope_pixel_z_proc; ///< Called to get the world Z coordinate for a given location within the tile.
	ClearTileProc *clear_tile_proc; ////< Called to clear a tile.
	AddAcceptedCargoProc *add_accepted_cargo_proc = nullptr; ///< Adds accepted cargo of the tile to cargo array supplied as parameter.
	GetTileDescProc *get_tile_desc_proc; ///< Get a description of a tile (for the 'land area information' tool).
	GetTileTrackStatusProc *get_tile_track_status_proc = [](TileIndex, TransportType, uint, DiagDirection) -> TrackStatus { return {}; }; ///< Get available tracks and status of a tile.
	ClickTileProc *click_tile_proc = nullptr; ///< Called when tile is clicked
	AnimateTileProc *animate_tile_proc = nullptr; ///< Called to animate a tile.
	TileLoopProc *tile_loop_proc; ///< Called to periodically update the tile.
	ChangeTileOwnerProc *change_tile_owner_proc = [](TileIndex, CompanyID, CompanyID) {}; ///< Called to change the ownership of elements on a tile.
	AddProducedCargoProc *add_produced_cargo_proc = nullptr; ///< Adds produced cargo of the tile to cargo array supplied as parameter.
	VehicleEnterTileProc *vehicle_enter_tile_proc = nullptr; ///< Called when a vehicle enters a tile.
	GetFoundationProc *get_foundation_proc = [](TileIndex, Slope) { return FOUNDATION_NONE; }; ///< Called to get the foundation.
	TerraformTileProc *terraform_tile_proc; ///< Called when a terraforming operation is about to take place.
	CheckBuildAboveProc *check_build_above_proc = nullptr; ///< Called to check whether a bridge can be build above.
};

extern const EnumClassIndexContainer<std::array<const TileTypeProcs *, to_underlying(TileType::MaxSize)>, TileType> _tile_type_procs;

TrackStatus GetTileTrackStatus(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side = INVALID_DIAGDIR);
VehicleEnterTileStates VehicleEnterTile(Vehicle *v, TileIndex tile, int x, int y);
void ChangeTileOwner(TileIndex tile, Owner old_owner, Owner new_owner);
void GetTileDesc(TileIndex tile, TileDesc &td);

/**
 * Obtain cargo acceptance of a tile.
 * @param tile Tile queried for its accepted cargo.
 * @param acceptance Storage destination of the cargo acceptance in 1/8.
 * @param always_accepted Bitmask of always accepted cargo types.
 */
inline void AddAcceptedCargo(TileIndex tile, CargoArray &acceptance, CargoTypes &always_accepted)
{
	AddAcceptedCargoProc *proc = _tile_type_procs[GetTileType(tile)]->add_accepted_cargo_proc;
	if (proc == nullptr) return;
	proc(tile, acceptance, always_accepted);
}

/**
 * Obtain the produced cargo of a tile.
 * @param tile Tile being queried.
 * @param produced  Destination array for produced cargo.
 */
inline void AddProducedCargo(TileIndex tile, CargoArray &produced)
{
	AddProducedCargoProc *proc = _tile_type_procs[GetTileType(tile)]->add_produced_cargo_proc;
	if (proc == nullptr) return;
	proc(tile, produced);
}

/**
 * Test if a tile may be animated.
 * @param tile Tile to test.
 * @returns True iff the type of the tile has a handler for tile animation.
 */
inline bool MayAnimateTile(TileIndex tile)
{
	return _tile_type_procs[GetTileType(tile)]->animate_tile_proc != nullptr;
}

inline void AnimateTile(TileIndex tile)
{
	AnimateTileProc *proc = _tile_type_procs[GetTileType(tile)]->animate_tile_proc;
	assert(proc != nullptr);
	proc(tile);
}

inline bool ClickTile(TileIndex tile)
{
	ClickTileProc *proc = _tile_type_procs[GetTileType(tile)]->click_tile_proc;
	if (proc == nullptr) return false;
	return proc(tile);
}

#endif /* TILE_CMD_H */
