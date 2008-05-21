/* $Id$ */

/** @file tile_cmd.h Generic 'commands' that can be performed on all tiles. */

#ifndef TILE_CMD_H
#define TILE_CMD_H

#include "slope_type.h"
#include "tile_type.h"
#include "command_type.h"
#include "vehicle_type.h"
#include "cargo_type.h"
#include "strings_type.h"
#include "date_type.h"
#include "player_type.h"
#include "direction_type.h"
#include "track_type.h"
#include "transport_type.h"

/** The returned bits of VehicleEnterTile. */
enum VehicleEnterTileStatus {
	VETS_ENTERED_STATION  = 1, ///< The vehicle entered a station
	VETS_ENTERED_WORMHOLE = 2, ///< The vehicle either entered a bridge, tunnel or depot tile (this includes the last tile of the bridge/tunnel)
	VETS_CANNOT_ENTER     = 3, ///< The vehicle cannot enter the tile

	/**
	 * Shift the VehicleEnterTileStatus this many bits
	 * to the right to get the station ID when
	 * VETS_ENTERED_STATION is set
	 */
	VETS_STATION_ID_OFFSET = 8,
	VETS_STATION_MASK      = 0xFFFF << VETS_STATION_ID_OFFSET,

	/** Bit sets of the above specified bits */
	VETSB_CONTINUE         = 0,                          ///< The vehicle can continue normally
	VETSB_ENTERED_STATION  = 1 << VETS_ENTERED_STATION,  ///< The vehicle entered a station
	VETSB_ENTERED_WORMHOLE = 1 << VETS_ENTERED_WORMHOLE, ///< The vehicle either entered a bridge, tunnel or depot tile (this includes the last tile of the bridge/tunnel)
	VETSB_CANNOT_ENTER     = 1 << VETS_CANNOT_ENTER,     ///< The vehicle cannot enter the tile
};
DECLARE_ENUM_AS_BIT_SET(VehicleEnterTileStatus);

struct TileInfo {
	uint x;
	uint y;
	Slope tileh;
	TileIndex tile;
	uint z;
};

struct TileDesc {
	StringID str;
	Owner owner[4];
	StringID owner_type[4];
	Date build_date;
	uint64 dparam[2];
};

typedef void DrawTileProc(TileInfo *ti);
typedef uint GetSlopeZProc(TileIndex tile, uint x, uint y);
typedef CommandCost ClearTileProc(TileIndex tile, byte flags);
typedef void GetAcceptedCargoProc(TileIndex tile, AcceptedCargo res);
typedef void GetTileDescProc(TileIndex tile, TileDesc *td);

/**
 * GetTileTrackStatusProcs return a value that contains the possible tracks
 * that can be taken on a given tile by a given transport.
 * The return value contains the existing trackdirs and signal states.
 *
 * see track_func.h for usage of TrackStatus.
 *
 * @param tile     the tile to get the track status from
 * @param mode     the mode of transportation
 * @param sub_mode used to differentiate between different kinds within the mode
 * @return the track status information
 */
typedef TrackStatus GetTileTrackStatusProc(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side);
typedef void GetProducedCargoProc(TileIndex tile, CargoID *b);
typedef void ClickTileProc(TileIndex tile);
typedef void AnimateTileProc(TileIndex tile);
typedef void TileLoopProc(TileIndex tile);
typedef void ChangeTileOwnerProc(TileIndex tile, PlayerID old_player, PlayerID new_player);

/** @see VehicleEnterTileStatus to see what the return values mean */
typedef VehicleEnterTileStatus VehicleEnterTileProc(Vehicle *v, TileIndex tile, int x, int y);
typedef Foundation GetFoundationProc(TileIndex tile, Slope tileh);

/**
 * Called when a tile is affected by a terraforming operation.
 * The function has to check if terraforming of the tile is allowed and return extra terraform-cost that depend on the tiletype.
 * With DC_EXEC in flags it has to perform tiletype-specific actions (like clearing land etc., but not the terraforming itself).
 *
 * @note The terraforming has not yet taken place. So GetTileZ() and GetTileSlope() refer to the landscape before the terraforming operation.
 *
 * @param tile      The involved tile.
 * @param flags     Command flags passed to the terraform command (DC_EXEC, DC_QUERY_COST, etc.).
 * @param z_new     TileZ after terraforming.
 * @param tileh_new Slope after terraforming.
 * @return Error code or extra cost for terraforming (like clearing land, building foundations, etc., but not the terraforming itself.)
 */
typedef CommandCost TerraformTileProc(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new);

struct TileTypeProcs {
	DrawTileProc *draw_tile_proc;
	GetSlopeZProc *get_slope_z_proc;
	ClearTileProc *clear_tile_proc;
	GetAcceptedCargoProc *get_accepted_cargo_proc;
	GetTileDescProc *get_tile_desc_proc;
	GetTileTrackStatusProc *get_tile_track_status_proc;
	ClickTileProc *click_tile_proc;
	AnimateTileProc *animate_tile_proc;
	TileLoopProc *tile_loop_proc;
	ChangeTileOwnerProc *change_tile_owner_proc;
	GetProducedCargoProc *get_produced_cargo_proc;
	VehicleEnterTileProc *vehicle_enter_tile_proc;
	GetFoundationProc *get_foundation_proc;
	TerraformTileProc *terraform_tile_proc;
};

extern const TileTypeProcs * const _tile_type_procs[16];

TrackStatus GetTileTrackStatus(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side = INVALID_DIAGDIR);
void GetAcceptedCargo(TileIndex tile, AcceptedCargo ac);
void ChangeTileOwner(TileIndex tile, PlayerID old_player, PlayerID new_player);
void AnimateTile(TileIndex tile);
void ClickTile(TileIndex tile);
void GetTileDesc(TileIndex tile, TileDesc *td);

#endif /* TILE_CMD_H */
