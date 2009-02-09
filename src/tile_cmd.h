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
#include "company_type.h"
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

/** Tile information, used while rendering the tile */
struct TileInfo {
	uint x;         ///< X position of the tile in unit coordinates
	uint y;         ///< Y position of the tile in unit coordinates
	Slope tileh;    ///< Slope of the tile
	TileIndex tile; ///< Tile index
	uint z;         ///< Height
};

/** Tile description for the 'land area information' tool */
struct TileDesc {
	StringID str;           ///< Description of the tile
	Owner owner[4];         ///< Name of the owner(s)
	StringID owner_type[4]; ///< Type of each owner
	Date build_date;        ///< Date of construction of tile contents
	StringID station_class; ///< Class of station
	StringID station_name;  ///< Type of station within the class
	const char *grf;        ///< newGRF used for the tile contents
	uint64 dparam[2];       ///< Parameters of the \a str string
};

/**
 * Tile callback function signature for drawing a tile and its contents to the screen
 * @param ti Information about the tile to draw
 */
typedef void DrawTileProc(TileInfo *ti);
typedef uint GetSlopeZProc(TileIndex tile, uint x, uint y);
typedef CommandCost ClearTileProc(TileIndex tile, DoCommandFlag flags);

/**
 * Tile callback function signature for obtaining accepted carog of a tile
 * @param tile Tile queried for its accepted cargo
 * @param res  Storage destination of the cargo accepted
 */
typedef void GetAcceptedCargoProc(TileIndex tile, AcceptedCargo res);

/**
 * Tile callback function signature for obtaining a tile description
 * @param tile Tile being queried
 * @param td   Storage pointer for returned tile description
 */
typedef void GetTileDescProc(TileIndex tile, TileDesc *td);

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
 * @return the track status information
 */
typedef TrackStatus GetTileTrackStatusProc(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side);

/**
 * Tile callback function signature for obtaining the produced cargo of a tile.
 * @param tile Tile being queried
 * @param b    Destination array of produced cargo
 */
typedef void GetProducedCargoProc(TileIndex tile, CargoID *b);
typedef bool ClickTileProc(TileIndex tile);
typedef void AnimateTileProc(TileIndex tile);
typedef void TileLoopProc(TileIndex tile);
typedef void ChangeTileOwnerProc(TileIndex tile, Owner old_owner, Owner new_owner);

/** @see VehicleEnterTileStatus to see what the return values mean */
typedef VehicleEnterTileStatus VehicleEnterTileProc(Vehicle *v, TileIndex tile, int x, int y);
typedef Foundation GetFoundationProc(TileIndex tile, Slope tileh);

/**
 * Tile callback function signature of the terraforming callback.
 *
 * The function is called when a tile is affected by a terraforming operation.
 * It has to check if terraforming of the tile is allowed and return extra terraform-cost that depend on the tiletype.
 * With DC_EXEC in \a flags it has to perform tiletype-specific actions (like clearing land etc., but not the terraforming itself).
 *
 * @note The terraforming has not yet taken place. So GetTileZ() and GetTileSlope() refer to the landscape before the terraforming operation.
 *
 * @param tile      The involved tile.
 * @param flags     Command flags passed to the terraform command (DC_EXEC, DC_QUERY_COST, etc.).
 * @param z_new     TileZ after terraforming.
 * @param tileh_new Slope after terraforming.
 * @return Error code or extra cost for terraforming (like clearing land, building foundations, etc., but not the terraforming itself.)
 */
typedef CommandCost TerraformTileProc(TileIndex tile, DoCommandFlag flags, uint z_new, Slope tileh_new);

/**
 * Set of callback functions for performing tile operations of a given tile type.
 * @see TileType */
struct TileTypeProcs {
	DrawTileProc *draw_tile_proc;                  ///< Called to render the tile and its contents to the screen
	GetSlopeZProc *get_slope_z_proc;
	ClearTileProc *clear_tile_proc;
	GetAcceptedCargoProc *get_accepted_cargo_proc; ///< Return accepted cargo of the tile
	GetTileDescProc *get_tile_desc_proc;           ///< Get a description of a tile (for the 'land area information' tool)
	GetTileTrackStatusProc *get_tile_track_status_proc; ///< Get available tracks and status of a tile
	ClickTileProc *click_tile_proc;                ///< Called when tile is clicked
	AnimateTileProc *animate_tile_proc;
	TileLoopProc *tile_loop_proc;
	ChangeTileOwnerProc *change_tile_owner_proc;
	GetProducedCargoProc *get_produced_cargo_proc; ///< Return produced cargo of the tile
	VehicleEnterTileProc *vehicle_enter_tile_proc; ///< Called when a vehicle enters a tile
	GetFoundationProc *get_foundation_proc;
	TerraformTileProc *terraform_tile_proc;        ///< Called when a terraforming operation is about to take place
};

extern const TileTypeProcs * const _tile_type_procs[16];

TrackStatus GetTileTrackStatus(TileIndex tile, TransportType mode, uint sub_mode, DiagDirection side = INVALID_DIAGDIR);
VehicleEnterTileStatus VehicleEnterTile(Vehicle *v, TileIndex tile, int x, int y);
void GetAcceptedCargo(TileIndex tile, AcceptedCargo ac);
void ChangeTileOwner(TileIndex tile, Owner old_owner, Owner new_owner);
void AnimateTile(TileIndex tile);
bool ClickTile(TileIndex tile);
void GetTileDesc(TileIndex tile, TileDesc *td);

#endif /* TILE_CMD_H */
