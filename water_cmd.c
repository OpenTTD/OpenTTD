#include "stdafx.h"
#include "ttd.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "town.h"

bool IsShipDepotTile(TileIndex tile)
{
	return IS_TILETYPE(tile, MP_WATER) &&	(_map5[tile]&~3) == 0x80;
}

bool IsClearWaterTile(uint tile)
{
	TileInfo ti;
	FindLandscapeHeightByTile(&ti, tile);
	return (ti.type == MP_WATER && ti.tileh == 0 && ti.map5 == 0);
}

/* Build a ship depot
 * p1 - direction
 */

int32 CmdBuildShipDepot(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile, tile2;
	
	int32 cost, ret;
	Depot *dep;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	tile = TILE_FROM_XY(x,y);
	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	tile2 = tile + (p1 ? TILE_XY(0,1) : TILE_XY(1,0));
	if (!EnsureNoVehicle(tile2))
		return CMD_ERROR;

	if (!IsClearWaterTile(tile) || !IsClearWaterTile(tile2))
		return_cmd_error(STR_3801_MUST_BE_BUILT_ON_WATER);

	ret = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (ret == CMD_ERROR) return CMD_ERROR;
	ret = DoCommandByTile(tile2, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (ret == CMD_ERROR)
		return CMD_ERROR;
	
	// pretend that we're not making land from the water even though we actually are.
	cost = 0;

	dep = AllocateDepot();
	if (dep == NULL)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		dep->xy = tile;
		_last_built_ship_depot_tile = tile;
		dep->town_index = ClosestTownFromTile(tile, (uint)-1)->index;

		ModifyTile(tile, 
			MP_SETTYPE(MP_WATER) | MP_MAPOWNER_CURRENT | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR,
			(0x80 + p1*2)
		);
	
		ModifyTile(tile2, 
			MP_SETTYPE(MP_WATER) | MP_MAPOWNER_CURRENT | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR,
			(0x81 + p1*2)
		);
	}

	return cost + _price.build_ship_depot;
}

static int32 RemoveShipDepot(uint tile, uint32 flags)
{
	uint tile2;

	if (!CheckTileOwnership(tile))
		return CMD_ERROR;

	if (!EnsureNoVehicle(tile))
		return CMD_ERROR;

	tile2 = tile + ((_map5[tile] & 2) ? TILE_XY(0,1) : TILE_XY(1,0));

	if (!EnsureNoVehicle(tile2))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		Depot *d;

		// convert the cleared tiles to water
		ModifyTile(tile, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0);
		ModifyTile(tile2, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0);

		// Kill the entry from the depot table
		for(d=_depots; d->xy != tile; d++) {}
		d->xy = 0;
				
		DeleteWindowById(WC_VEHICLE_DEPOT, tile);		
	}

	return _price.remove_ship_depot;
}

// build a shiplift
static int32 DoBuildShiplift(uint tile, int dir, uint32 flags)
{
	int32 ret;
	int delta;

	// middle tile
	ret = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (ret == CMD_ERROR) return CMD_ERROR;
	
	delta = _tileoffs_by_dir[dir];
	// lower tile
	ret = DoCommandByTile(tile - delta, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (ret == CMD_ERROR) return CMD_ERROR;
	if (GetTileSlope(tile - delta, NULL)) return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

	// upper tile
	ret = DoCommandByTile(tile + delta, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
	if (ret == CMD_ERROR) return CMD_ERROR;
	if (GetTileSlope(tile + delta, NULL)) return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

	if (flags & DC_EXEC) {
		ModifyTile(tile, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0x10 + dir);
		ModifyTile(tile - delta, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0x14 + dir);
		ModifyTile(tile + delta, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0x18 + dir);
	}

	return _price.clear_water * 22 >> 3;
}

static int32 RemoveShiplift(uint tile, uint32 flags)
{
	int delta = _tileoffs_by_dir[_map5[tile] & 3];

	// make sure no vehicle is on the tile.
	if (!EnsureNoVehicle(tile) || !EnsureNoVehicle(tile + delta) || !EnsureNoVehicle(tile - delta))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoClearSquare(tile);
		DoClearSquare(tile + delta);
		DoClearSquare(tile - delta);
	}
	
	return _price.clear_water * 2;
}

static void MarkTilesAroundDirty(uint tile)
{
	MarkTileDirtyByTile(TILE_ADDXY(tile, 0, 1));
	MarkTileDirtyByTile(TILE_ADDXY(tile, 0, -1));
	MarkTileDirtyByTile(TILE_ADDXY(tile, 1, 0));
	MarkTileDirtyByTile(TILE_ADDXY(tile, -1, 0));
}

int32 CmdBuildLock(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile = TILE_FROM_XY(x,y);
	int32 ret;
	uint th;
	th = GetTileSlope(tile, NULL); 

	if (th==3 || th==6 || th==9 || th==12) {
		static const byte _shiplift_dirs[16] = {0,0,0,2,0,0,1,0,0,3,0,0,0};
		ret = DoBuildShiplift(tile, _shiplift_dirs[th], flags);
		return ret;
		}
	else
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

	return 0;
}

int32 CmdBuildCanal(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile = TILE_FROM_XY(x,y);
	int32 ret;
	uint th;
	uint endtile = (uint)p1;
	int delta;
	int32 cost;
	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	// move in which direction?
	delta = (GET_TILE_X(tile) == GET_TILE_X(endtile)) ? TILE_XY(0,1) : TILE_XY(1,0);
	if (endtile < tile) delta = -delta;
	
	cost = 0;
	for(;;) {
		ret = 0;
		th = GetTileSlope(tile, NULL);
		if(th!=0)
			return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

			// can't make water of water!
			if (IS_TILETYPE(tile, MP_WATER)) {
				_error_message = STR_1007_ALREADY_BUILT;
			} else {

				/* is middle piece of a bridge? */
				if (IS_TILETYPE(tile, MP_TUNNELBRIDGE) && _map5[tile] & 0x40) { /* build under bridge */
					if(_map5[tile] & 0x20) { // transport route under bridge
						_error_message = STR_5800_OBJECT_IN_THE_WAY;
						ret = CMD_ERROR;
					}
					else if (_map5[tile] & 0x18) { // already water under bridge
						_error_message = STR_1007_ALREADY_BUILT;
						ret = CMD_ERROR;
					}

				/* no bridge? then try to clear it. */
				} else  {
					ret = DoCommandByTile(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
				}
				if (ret == CMD_ERROR) return ret;
				cost += ret;

				/* execute modifications */
				if (flags & DC_EXEC) {
					if(IS_TILETYPE(tile, MP_TUNNELBRIDGE)) {
						// change owner to OWNER_WATER and set land under bridge bit to water
						ModifyTile(tile, MP_MAP5 | MP_MAPOWNER, OWNER_WATER, _map5[tile] | 0x08);
					} else {
						ModifyTile(tile, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR, OWNER_WATER, 0);
					}
					// mark the tiles around dirty too
					MarkTilesAroundDirty(tile);
				}

				cost += _price.clear_water;
			}
		if (tile == endtile)
			break;
		tile += delta;
	}
	if (cost == 0) return CMD_ERROR;

	return cost;
}

static int32 ClearTile_Water(uint tile, byte flags) {
	byte m5 = _map5[tile];
	uint slope;

	if (m5 <= 1) { // water and shore
		// Allow building on water? It's ok to build on shores.
		if (flags & DC_NO_WATER && m5 != 1)
			return_cmd_error(STR_3807_CAN_T_BUILD_ON_WATER);

		// Make sure no vehicle is on the tile
		if (!EnsureNoVehicle(tile))
			return CMD_ERROR;

		// Make sure it's not an edge tile.
		if (!(IS_INT_INSIDE(GET_TILE_X(tile),1,TILE_X_MAX-1) &&
				IS_INT_INSIDE(GET_TILE_Y(tile),1,TILE_Y_MAX-1)))
			return_cmd_error(STR_0002_TOO_CLOSE_TO_EDGE_OF_MAP);

		if (m5 == 0) {
			if (flags & DC_EXEC)
				DoClearSquare(tile);
			return _price.clear_water;
		} else if (m5 == 1) {
			slope = GetTileSlope(tile,NULL);
			if (slope == 8 || slope == 4 || slope == 2 || slope == 1) {
				if (flags & DC_EXEC)
					DoClearSquare(tile);
				return _price.clear_water;
			}
			if (flags & DC_EXEC)			
				DoClearSquare(tile);
			return _price.purchase_land;
		} else
			return CMD_ERROR;
	} else if ((m5 & 0x10) == 0x10) {
		// shiplift

		static const TileIndexDiff _shiplift_tomiddle_offs[12] = {
			0,0,0,0, // middle
			TILE_XY(-1, 0),TILE_XY(0, 1),TILE_XY(1, 0),TILE_XY(0, -1), // lower
			TILE_XY(1, 0),TILE_XY(0, -1),TILE_XY(-1, 0),TILE_XY(0, 1), // upper
		};
				
		if (flags & DC_AUTO) return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);
		// don't allow water to delete it.
		if (_current_player == OWNER_WATER) return CMD_ERROR;
		// move to the middle tile..
		return RemoveShiplift(tile + _shiplift_tomiddle_offs[m5 & 0xF], flags);
	} else {
		// ship depot
		if (flags & DC_AUTO)
			return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);

		if (m5 == 0x80 || m5 == 0x82) {}
		else if (m5 == 0x81) { tile -= TILE_XY(1,0); }
		else if (m5 == 0x83) { tile -= TILE_XY(0,1); }
		else
			return CMD_ERROR;

		return RemoveShipDepot(tile,flags);
	}
}

// return true if a tile is a water tile.
static bool IsWateredTile(uint tile)
{
	byte m5 = _map5[tile];
	if (IS_TILETYPE(tile, MP_WATER)) {
		return m5 != 1;	
	} else if (IS_TILETYPE(tile, MP_STATION)) {
		// returns true if it is a dock-station (m5 inside values is m5<75 all stations, 
		// 83<=m5<=114 new airports
		return !(m5 < 75 || (m5 >= 83 && m5 <= 114));
	} else if (IS_TILETYPE(tile, MP_TUNNELBRIDGE)) {
		return (m5 & 0xF8) == 0xC8;
	} else
		return false;
}

// draw a canal styled water tile with dikes around
void DrawCanalWater(uint tile)
{
	uint wa;
	
	// determine the edges around with water.
	wa = IsWateredTile(TILE_ADDXY(tile, -1, 0)) << 0;
	wa += IsWateredTile(TILE_ADDXY(tile, 0, 1)) << 1;
	wa += IsWateredTile(TILE_ADDXY(tile, 1, 0)) << 2;
	wa += IsWateredTile(TILE_ADDXY(tile, 0, -1)) << 3;
	
	if (!(wa & 1)) DrawGroundSprite(SPR_CANALS_BASE + 57);
	if (!(wa & 2)) DrawGroundSprite(SPR_CANALS_BASE + 58);
	if (!(wa & 4)) DrawGroundSprite(SPR_CANALS_BASE + 59);
	if (!(wa & 8)) DrawGroundSprite(SPR_CANALS_BASE + 60);

	// right corner
	if ((wa & 3) == 0) DrawGroundSprite(SPR_CANALS_BASE + 57 + 4);
	else if ((wa & 3) == 3 && !IsWateredTile(TILE_ADDXY(tile, -1, 1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 8);

	// bottom corner
	if ((wa & 6) == 0) DrawGroundSprite(SPR_CANALS_BASE + 57 + 5);
	else if ((wa & 6) == 6 && !IsWateredTile(TILE_ADDXY(tile, 1, 1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 9);

	// left corner
	if ((wa & 12) == 0) DrawGroundSprite(SPR_CANALS_BASE + 57 + 6);
	else if ((wa & 12) == 12 && !IsWateredTile(TILE_ADDXY(tile, 1, -1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 10);

	// upper corner
	if ((wa & 9) == 0) DrawGroundSprite(SPR_CANALS_BASE + 57 + 7);
	else if ((wa & 9) == 9 && !IsWateredTile(TILE_ADDXY(tile, -1, -1))) DrawGroundSprite(SPR_CANALS_BASE + 57 + 11);
}

typedef struct WaterDrawTileStruct {
	int8 delta_x;
	int8 delta_y;
	int8 delta_z;
	byte width;
	byte height;
	byte unk;
	SpriteID image;
} WaterDrawTileStruct;

typedef struct LocksDrawTileStruct {
	int8 delta_x, delta_y, delta_z;
	byte width, height, depth;
	SpriteID image;
} LocksDrawTileStruct;

#include "table/water_land.h"

static void DrawWaterStuff(TileInfo *ti, const byte *t, uint32 palette, uint base)
{
	const WaterDrawTileStruct *wdts;
	uint32 image;

	DrawGroundSprite(*(uint16*)t);	
	t += sizeof(uint16);
		
	for(wdts = (WaterDrawTileStruct *)t; (byte)wdts->delta_x != 0x80; wdts++) {
		image =	wdts->image + base;
		if (_display_opt & DO_TRANS_BUILDINGS) {
			image |= palette;
		} else {
			image = (image & 0x3FFF) | 0x03224000;
		}
		AddSortableSpriteToDraw(image, ti->x + wdts->delta_x, ti->y + wdts->delta_y, wdts->width, wdts->height, wdts->unk, ti->z + wdts->delta_z);
	}
}

static void DrawTile_Water(TileInfo *ti)
{
	// draw water tile
	if (ti->map5 == 0) {
		DrawGroundSprite(0xFDD);
		if (ti->z != 0) DrawCanalWater(ti->tile);
		return;
	}

	// draw shore
	if (ti->map5 == 1) {
		assert(ti->tileh < 16);
		DrawGroundSprite(_water_shore_sprites[ti->tileh]);
		return;
	}

	// draw shiplift
	if ((ti->map5 & 0xF0) == 0x10) {
		const byte *t = _shiplift_display_seq[ti->map5 & 0xF];
		DrawWaterStuff(ti, t, 0, ti->z > t[19] ? 24 : 0);
		return;
	}

	DrawWaterStuff(ti, _shipdepot_display_seq[ti->map5 & 0x7F], PLAYER_SPRITE_COLOR(_map_owner[ti->tile]), 0);
}

void DrawShipDepotSprite(int x, int y, int image)
{
	const byte *t;
	const WaterDrawTileStruct *wdts;

	t = _shipdepot_display_seq[image];
	DrawSprite(*(uint16*)t, x, y);
	t += sizeof(uint16);
		
	for(wdts = (WaterDrawTileStruct *)t; (byte)wdts->delta_x != 0x80; wdts++) {
		Point pt = RemapCoords(wdts->delta_x, wdts->delta_y, wdts->delta_z);
		DrawSprite(wdts->image + PLAYER_SPRITE_COLOR(_local_player), x + pt.x, y + pt.y);
	}
}


uint GetSlopeZ_Water(TileInfo *ti)
{ 
	return GetPartialZ(ti->x&0xF, ti->y&0xF, ti->tileh) + ti->z;
}

uint GetSlopeTileh_Water(TileInfo *ti)
{ 
	return ti->tileh;
}

static void GetAcceptedCargo_Water(uint tile, AcceptedCargo *ac)
{
	/* not used */
}

static void GetTileDesc_Water(uint tile, TileDesc *td)
{
	if (_map5[tile] == 0 && GET_TILEHEIGHT(tile) == 0)
		td->str = STR_3804_WATER;
	else if (_map5[tile] == 0)
		td->str = STR_LANDINFO_CANAL;
	else if (_map5[tile] == 1)
		td->str = STR_3805_COAST_OR_RIVERBANK;
	else if ((_map5[tile]&0xF0) == 0x10)
		td->str = STR_LANDINFO_LOCK;
	else
		td->str = STR_3806_SHIP_DEPOT;

	td->owner = _map_owner[tile];
}

static void AnimateTile_Water(uint tile)
{
	/* not used */
}

static void TileLoopWaterHelper(uint tile, const int16 *offs)
{
	byte *p;

	p = &_map_type_and_height[tile];
	tile += offs[0];

	// type of this tile mustn't be water already.
	if (p[offs[0]] >> 4 == MP_WATER)
		return;

	if ( (p[offs[1]] | p[offs[2]]) & 0xF )
		return;

	if ( (p[offs[3]] | p[offs[4]]) & 0xF ) {
		// make coast..
		if (p[offs[0]] >> 4 == MP_CLEAR || p[offs[0]] >> 4 == MP_TREES) {
			_current_player = OWNER_WATER;
			if (DoCommandByTile(tile,0,0,DC_EXEC | DC_AUTO, CMD_LANDSCAPE_CLEAR) != CMD_ERROR)
				ModifyTile(tile, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR,OWNER_WATER,1);
		}
	} else {
		if (IS_TILETYPE(tile, MP_TUNNELBRIDGE)) {
			byte m5 = _map5[tile];
			if ( (m5&0xF8) == 0xC8 || (m5&0xF8) == 0xF0)
				return;

			if ( (m5&0xC0) == 0xC0) {
				ModifyTile(tile, MP_MAPOWNER | MP_MAP5,OWNER_WATER,(m5 & ~0x38)|0x8);
				return;
			}
		}

		_current_player = OWNER_WATER;
		if (DoCommandByTile(tile,0,0,DC_EXEC, CMD_LANDSCAPE_CLEAR) != CMD_ERROR)
			ModifyTile(tile, MP_SETTYPE(MP_WATER) | MP_MAPOWNER | MP_MAP5 | MP_MAP2_CLEAR | MP_MAP3LO_CLEAR | MP_MAP3HI_CLEAR,OWNER_WATER,0);
	}
}

// called from tunnelbridge_cmd
void TileLoop_Water(uint tile)
{
	int i;
	static const TileIndexDiff _tile_loop_offs_array[4][5] = {
		// tile to mod																shore?				shore?
		{TILE_XY(-1,0), TILE_XY(0,0), TILE_XY(0,1), TILE_XY(-1,0), TILE_XY(-1,1)},
		{TILE_XY(0,1),  TILE_XY(0,1), TILE_XY(1,1), TILE_XY(0,2),  TILE_XY(1,2)},
		{TILE_XY(1,0),  TILE_XY(1,0), TILE_XY(1,1), TILE_XY(2,0),  TILE_XY(2,1)},
		{TILE_XY(0,-1), TILE_XY(0,0), TILE_XY(1,0), TILE_XY(0,-1), TILE_XY(1,-1)},
	};

	if ( IS_INT_INSIDE(GET_TILE_X(tile),1,TILES_X-3+1) &&
	     IS_INT_INSIDE(GET_TILE_Y(tile),1,TILES_Y-3+1)) {
		for(i=0; i!=4; i++)
			TileLoopWaterHelper(tile, _tile_loop_offs_array[i]);
	}

	// edges
	if ( GET_TILE_X(tile)==0 && IS_INT_INSIDE(GET_TILE_Y(tile),1,TILES_Y-3+1)) //NE
		TileLoopWaterHelper(tile, _tile_loop_offs_array[2]);

	if ( GET_TILE_X(tile)==(TILES_X-2) && IS_INT_INSIDE(GET_TILE_Y(tile),1,TILES_Y-3+1)) //SW
		TileLoopWaterHelper(tile, _tile_loop_offs_array[0]);

	if ( GET_TILE_Y(tile)==0 && IS_INT_INSIDE(GET_TILE_X(tile),1,TILES_X-3+1)) //NW
		TileLoopWaterHelper(tile, _tile_loop_offs_array[1]);

	if ( GET_TILE_Y(tile)==(TILES_Y-2) && IS_INT_INSIDE(GET_TILE_X(tile),1,TILES_X-3+1)) //SE
		TileLoopWaterHelper(tile, _tile_loop_offs_array[3]);

}


static const byte _coast_tracks[16] = {0, 32, 4, 0, 16, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0};
static const byte _shipdepot_tracks[4] = {1,1,2,2};
static const byte _shiplift_tracks[12] = {1,2,1,2,1,2,1,2,1,2,1,2};
static uint32 GetTileTrackStatus_Water(uint tile, int mode)
{
	uint m5;
	uint b;

	if (mode != 4)
		return 0;

	m5 = _map5[tile];
	if (m5 == 0)
		return 0x3F3F;

	if (m5 == 1) {
		b = _coast_tracks[GetTileSlope(tile, NULL)&0xF];
		return b + (b<<8);
	}

	if ( (m5 & 0x10) == 0x10) {
		// 
		b = _shiplift_tracks[m5 & 0xF];
		return b + (b<<8);
	}

	if (!(m5 & 0x80))
		return 0;

	b = _shipdepot_tracks[m5 & 0x7F];
	return b + (b<<8);
}

extern void ShowShipDepotWindow(uint tile);

static void ClickTile_Water(uint tile)
{
	byte m5 = _map5[tile] - 0x80;
	
	if (IS_BYTE_INSIDE(m5, 0, 3+1)) {
		if (m5 & 1)
			tile += (m5==1) ? TILE_XY(-1,0) : TILE_XY(0,-1);
		ShowShipDepotWindow(tile);
	}
}

static void ChangeTileOwner_Water(uint tile, byte old_player, byte new_player)
{
	if (_map_owner[tile] != old_player)
		return;

	if (new_player != 255) {
		_map_owner[tile] = new_player;
	} else {
		DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	}
}

static uint32 VehicleEnter_Water(Vehicle *v, uint tile, int x, int y)
{
	return 0;
}

void InitializeDock()
{
	_last_built_ship_depot_tile = 0;
}

const TileTypeProcs _tile_type_water_procs = {
	DrawTile_Water,						/* draw_tile_proc */
	GetSlopeZ_Water,					/* get_slope_z_proc */
	ClearTile_Water,					/* clear_tile_proc */
	GetAcceptedCargo_Water,		/* get_accepted_cargo_proc */
	GetTileDesc_Water,				/* get_tile_desc_proc */
	GetTileTrackStatus_Water,	/* get_tile_track_status_proc */
	ClickTile_Water,					/* click_tile_proc */
	AnimateTile_Water,				/* animate_tile_proc */
	TileLoop_Water,						/* tile_loop_clear */
	ChangeTileOwner_Water,		/* change_tile_owner_clear */
	NULL,											/* get_produced_cargo_proc */
	VehicleEnter_Water,				/* vehicle_enter_tile_proc */
	NULL,											/* vehicle_leave_tile_proc */
	GetSlopeTileh_Water,			/* get_slope_tileh_proc */
};

