#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "map.h"
#include "vehicle.h"
#include "viewport.h"
#include "command.h"
#include "player.h"
#include "town.h"
#include "sound.h"

extern void DrawCanalWater(uint tile);

static const byte _bridge_available_year[MAX_BRIDGES] = {
	0, 0, 10, 0, 10, 10, 10, 10, 10, 10, 75, 85, 90
};

static const byte _bridge_minlen[MAX_BRIDGES] = {
	0, 0, 0, 2, 3, 3, 3, 3, 3, 0, 2, 2, 2
};

static const byte _bridge_maxlen[MAX_BRIDGES] = {
	16, 2, 5, 10, 16, 16, 7, 8, 9, 2, 16, 32, 32,
};

const uint16 _bridge_type_price_mod[MAX_BRIDGES] = {
	80, 112, 144, 168, 185, 192, 224, 232, 248, 240, 255, 380, 510,
};

const uint16 _bridge_speeds[MAX_BRIDGES] = {
	0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x0A0, 0x0D0, 0x0F0, 0x100, 0x140, 0x200, 0x260
};

const PalSpriteID _bridge_sprites[MAX_BRIDGES] = {
	0x0A24, 0x31E8A26, 0x0A25, 0x3208A22,
	0x0A22, 0x3218A22, 0x0A23, 0x31C8A23,
	0x31E8A23, 0x0A27, 0x0A28, 0x3218A28,
	0x3238A28,
};

const StringID _bridge_material[MAX_BRIDGES] = {
	STR_5012_WOODEN,
	STR_5013_CONCRETE,
	STR_500F_GIRDER_STEEL,
	STR_5011_SUSPENSION_CONCRETE,
	STR_500E_SUSPENSION_STEEL,
	STR_500E_SUSPENSION_STEEL,
	STR_5010_CANTILEVER_STEEL,
	STR_5010_CANTILEVER_STEEL,
	STR_5010_CANTILEVER_STEEL,
	STR_500F_GIRDER_STEEL,
	STR_5014_TUBULAR_STEEL,
	STR_5014_TUBULAR_STEEL,
	STR_BRIDGE_TUBULAR_SILICON,
};

// calculate the price factor for building a long bridge.
// basically the cost delta is 1,1, 1, 2,2, 3,3,3, 4,4,4,4, 5,5,5,5,5, 6,6,6,6,6,6,  7,7,7,7,7,7,7,  8,8,8,8,8,8,8,8,
int CalcBridgeLenCostFactor(int x)
{
	int n,r;
	if (x < 2) return x;
	x -= 2;
	for(n=0,r=2;;n++) {
		if (x <= n) return r + x * n;
		r += n * n;
		x -= n;
	}
}

enum {
	// foundation, whole tile is leveled up (tileh's 7, 11, 13, 14) --> 3 corners raised
	BRIDGE_FULL_LEVELED_FOUNDATION = 1 << 7 | 1 << 11 | 1 << 13 | 1 << 14,
	// foundation, tile is partly leveled up (tileh's 1, 2, 4, 8) --> 1 corner raised
	BRIDGE_PARTLY_LEVELED_FOUNDATION = 1 << 1 | 1 << 2 | 1 << 4 | 1 << 8,
	// no foundations (X,Y direction) (tileh's 0, 3, 6, 9, 12)
	BRIDGE_NO_FOUNDATION = 1 << 0 | 1 << 3 | 1 << 6 | 1 << 9 | 1 << 12,
};

/*	check if bridge can be built on slope
 *	direction 0 = X-axis, direction 1 = Y-axis
 *	is_start_tile = false		<-- end tile
 *	is_start_tile = true		<-- start tile
 */
static uint32 CheckBridgeSlope(uint direction, uint tileh, bool is_start_tile)
{
	if (!(tileh & 0x10)) {	// disable building on very steep slopes

		if (is_start_tile) {
			/* check slope at start tile
					- no extra cost
					- direction X: tiles 0,12
					- direction Y: tiles 0, 9
			*/
			if ((direction?0x201:0x1001) & (1 << tileh))
				return 0;

			// disallow certain start tiles to avoid certain crooked bridges
			if (tileh == 2)
				return CMD_ERROR;

		}
		else {
			/*	check slope at end tile
					- no extra cost
					- direction X: tiles 0, 3
					- direction Y: tiles 0, 6
			*/
			if ((direction?0x41:0x9) & (1 << tileh))
				return 0;

			// disallow certain end tiles to avoid certain crooked bridges
			if (tileh == 8)
				return CMD_ERROR;

		}

		/*	disallow common start/end tiles to avoid certain crooked bridges e.g.
		 *	start-tile:	X 2,1 Y 2,4 (2 was disabled before)
		 *	end-tile:		X 8,4 Y 8,1 (8 was disabled before)
		 */
		if ( (tileh == 1 && (is_start_tile != (bool)direction)) ||
			   (tileh == 4 && (is_start_tile == (bool)direction))    )
			return CMD_ERROR;

		// slope foundations
		if (BRIDGE_FULL_LEVELED_FOUNDATION & (1 << tileh) || BRIDGE_PARTLY_LEVELED_FOUNDATION & (1 << tileh))
			return _price.terraform;
	}

	return CMD_ERROR;
}

uint32 GetBridgeLength(TileIndex begin, TileIndex end)
{
	int x1, y1, x2, y2;	// coordinates of starting and end tiles
	x1 = TileX(begin);
	y1 = TileY(begin);
	x2 = TileX(end);
	y2 = TileY(end);

	return abs((x2 + y2 - x1 - y1)) - 1;
}

bool CheckBridge_Stuff(byte bridge_type, int bridge_len)
{
	int max;	// max possible length of a bridge (with patch 100)

	if (_bridge_available_year[bridge_type] > _cur_year)
				return false;

	max = _bridge_maxlen[bridge_type];
	if (max >= 16 && _patches.longbridges)
		max = 100;

	if (bridge_len < _bridge_minlen[bridge_type] || bridge_len > max)
		return false;

	return true;
}

/* Build a Bridge
 * x,y - end tile coord
 * p1  - packed start tile coords (~ dx)
 * p2&0xFF - bridge type (hi bh)
 * p2>>8 - rail type. &0x80 means road bridge.
 */
int32 CmdBuildBridge(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	int bridge_type;
	byte rail_or_road, railtype, m5;
	int sx,sy;
	TileInfo ti_start, ti_end, ti; /* OPT: only 2 of those are ever used */
	int bridge_len, odd_middle_part;
	uint direction;
	int i;
	int32 cost, terraformcost, ret;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	/* unpack parameters */
	bridge_type = p2 & 0xFF;
	railtype = (byte)(p2 >> 8);

	// type of bridge
	if (railtype & 0x80) {
		railtype = 0;
		rail_or_road = 2;
	} else {
		rail_or_road = 0;
	}

	sx = TileX(p1) * 16;
	sy = TileY(p1) * 16;

	direction = 0;

	/* check if valid, and make sure that (x,y) are smaller than (sx,sy) */
	if (x == sx) {
		if (y == sy)
			return_cmd_error(STR_5008_CANNOT_START_AND_END_ON);
		direction = 1;
		if (y > sy) {
			intswap(y,sy);
			intswap(x,sx);
		}
	} else if (y == sy) {
		if (x > sx) {
			intswap(y,sy);
			intswap(x,sx);
		}
	} else
		return_cmd_error(STR_500A_START_AND_END_MUST_BE_IN);

	/* retrieve landscape height and ensure it's on land */
	if (
		((FindLandscapeHeight(&ti_end, sx, sy),
			ti_end.type == MP_WATER) && ti_end.map5 == 0) ||
		((FindLandscapeHeight(&ti_start, x, y),
			ti_start.type == MP_WATER) && ti_start.map5 == 0))
		return_cmd_error(STR_02A0_ENDS_OF_BRIDGE_MUST_BOTH);

	if (BRIDGE_FULL_LEVELED_FOUNDATION & (1 << ti_start.tileh)) {
		ti_start.z += 8;
		ti_start.tileh = 0;
	}

	if (BRIDGE_FULL_LEVELED_FOUNDATION & (1 << ti_end.tileh)) {
		ti_end.z += 8;
		ti_end.tileh = 0;
	}

	if (ti_start.z != ti_end.z)
		return_cmd_error(STR_5009_LEVEL_LAND_OR_WATER_REQUIRED);

	_error_message = STR_500C;

	/* try and clear the start landscape */
	if ((ret=DoCommandByTile(ti_start.tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR)) == CMD_ERROR)
		return CMD_ERROR;
	cost = ret;

	terraformcost = CheckBridgeSlope(direction, ti_start.tileh, true);	// true - bridge-start-tile, false - bridge-end-tile

	// towns are not allowed to use bridges on slopes.
	if (terraformcost == CMD_ERROR ||
		 (terraformcost && ((!_patches.ainew_active && _is_ai_player) || _current_player == OWNER_TOWN || !_patches.build_on_slopes)))
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

	cost += terraformcost;

	/* try and clear the end landscape */
	if ((ret=DoCommandByTile(ti_end.tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR)) == CMD_ERROR)
		return CMD_ERROR;
	cost += ret;

	terraformcost = CheckBridgeSlope(direction, ti_end.tileh, false);	// false - end tile slope check

	// towns are not allowed to use bridges on slopes.
	if (terraformcost == CMD_ERROR ||
		 (terraformcost && ((!_patches.ainew_active && _is_ai_player) || _current_player == OWNER_TOWN || !_patches.build_on_slopes)))
		return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

	cost += terraformcost;

	/* do the drill? */
	if (flags & DC_EXEC) {
		/* build the start tile */
		ModifyTile(ti_start.tile,
			MP_SETTYPE(MP_TUNNELBRIDGE) |
			MP_MAP2 | MP_MAP3LO | MP_MAPOWNER_CURRENT | MP_MAP5,
			(bridge_type << 4), /* map2 */
			railtype, /* map3_lo */
			0x80 | direction | rail_or_road /* map5 */
		);

		/* build the end tile */
		ModifyTile(ti_end.tile,
			MP_SETTYPE(MP_TUNNELBRIDGE) |
			MP_MAP2 | MP_MAP3LO | MP_MAPOWNER_CURRENT | MP_MAP5,
			(bridge_type << 4), /* map2 */
			railtype, /* map3_lo */
			0x80 | 0x20 | direction | rail_or_road /* map5 */
		);
	}

	/* set various params */
	bridge_len = ((sx + sy - x - y) >> 4) - 1;

	//position of middle part of the odd bridge (larger than MAX(i) otherwise)
	odd_middle_part=(bridge_len%2)?(bridge_len/2):bridge_len;

	for(i = 0; i != bridge_len; i++) {
		if (direction != 0)
			y+=16;
		else
			x+=16;

		FindLandscapeHeight(&ti, x, y);

		_error_message = STR_5009_LEVEL_LAND_OR_WATER_REQUIRED;
		if (ti.tileh != 0 && ti.z >= ti_start.z)
			return CMD_ERROR;

		// Find ship below
		if ( ti.type == MP_WATER && !EnsureNoVehicle(ti.tile) )
		{
			_error_message = STR_980E_SHIP_IN_THE_WAY;
			return CMD_ERROR;
		}

		if (ti.type == MP_WATER) {
			if (ti.map5 != 0) goto not_valid_below;
			m5 = 0xC8;
		} else if (ti.type == MP_RAILWAY) {
			if (direction == 0) {
				if (ti.map5 != 2) goto not_valid_below;
			} else {
				if (ti.map5 != 1) goto not_valid_below;
			}
			m5 = 0xE0;
		} else if (ti.type == MP_STREET) {
			if (direction == 0) {
				if (ti.map5 != 5) goto not_valid_below;
			} else {
				if (ti.map5 != 10) goto not_valid_below;
			}
			m5 = 0xE8;
		} else {
not_valid_below:;
			/* try and clear the middle landscape */
			if ((ret=DoCommandByTile(ti.tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR)) == CMD_ERROR)
				return CMD_ERROR;
			cost += ret;
			m5 = 0xC0;
		}

		/* do middle part of bridge */
		if (flags & DC_EXEC) {
			_map5[ti.tile] = (byte)(m5 | direction | rail_or_road);
			SetTileType(ti.tile, MP_TUNNELBRIDGE);

			//bridges pieces sequence (middle parts)
			// bridge len 1: 0
			// bridge len 2: 0 1
			// bridge len 3: 0 4 1
			// bridge len 4: 0 2 3 1
			// bridge len 5: 0 2 5 3 1
			// bridge len 6: 0 2 3 2 3 1
			// bridge len 7: 0 2 3 4 2 3 1
			// #0 - alwats as first, #1 - always as last (if len>1)
			// #2,#3 are to pair in order
			// for odd bridges: #5 is going in the bridge middle if on even position, #4 on odd (counting from 0)

			if(i==0)                    //first tile
				m5 = 0;
			else if (i==bridge_len-1)   //last tile
				m5 = 1;
			else if(i==odd_middle_part) //we are on the middle of odd bridge: #5 on even pos, #4 on odd
				m5 = 5 - (i%2);
			else {
					// generate #2 and #3 in turns [i%2==0], after the middle of odd bridge
					// this sequence swaps [... XOR (i>odd_middle_part)],
					// for even bridges XOR does not apply as odd_middle_part==bridge_len
					m5 = 2 + ((i%2==0)^(i>odd_middle_part));
			}

			_map2[ti.tile] = (bridge_type << 4) | m5;
			_map3_lo[ti.tile] &= 0xF;
			_map3_lo[ti.tile] |= (byte)(railtype << 4);

			MarkTileDirtyByTile(ti.tile);
		}
	}

	SetSignalsOnBothDir(ti_start.tile, (direction&1) ? 1 : 0);

	/*	for human player that builds the bridge he gets a selection to choose from bridges (DC_QUERY_COST)
			It's unnecessary to execute this command every time for every bridge. So it is done only
			and cost is computed in "bridge_gui.c". For AI, Towns this has to be of course calculated
	*/
	if (!(flags & DC_QUERY_COST)) {
		bridge_len += 2;	// begin and end tiles/ramps

		if (_current_player < MAX_PLAYERS && !(_is_ai_player && !_patches.ainew_active))
			bridge_len = CalcBridgeLenCostFactor(bridge_len);

		cost += ((int64)bridge_len * _price.build_bridge * _bridge_type_price_mod[bridge_type]) >> 8;
	}

	return cost;
}

static bool DoCheckTunnelInWay(uint tile, uint z, uint dir)
{
	TileInfo ti;
	int delta;

	delta = TileOffsByDir(dir);

	do {
		tile -= delta;
		FindLandscapeHeightByTile(&ti, tile);
	} while (z < ti.z);

	if (z == ti.z && ti.type == MP_TUNNELBRIDGE && (ti.map5&0xF0) == 0 && (ti.map5&3) == dir) {
		_error_message = STR_5003_ANOTHER_TUNNEL_IN_THE_WAY;
		return false;
	}

	return true;
}

bool CheckTunnelInWay(uint tile, int z)
{
	return DoCheckTunnelInWay(tile,z,0) &&
		DoCheckTunnelInWay(tile,z,1) &&
		DoCheckTunnelInWay(tile,z,2) &&
		DoCheckTunnelInWay(tile,z,3);
}

static byte _build_tunnel_bh;
static byte _build_tunnel_railtype;

static int32 DoBuildTunnel(int x, int y, int x2, int y2, uint32 flags, uint exc_tile)
{
	uint end_tile;
	int direction;
	int32 cost, ret;
	TileInfo ti;
	uint z;

	if ((uint)x > MapMaxX() * 16 - 1 || (uint)y > MapMaxY() * 16 - 1)
		return CMD_ERROR;

	/* check if valid, and make sure that (x,y) is smaller than (x2,y2) */
	direction = 0;
	if (x == x2) {
		if (y == y2)
			return_cmd_error(STR_5008_CANNOT_START_AND_END_ON);
		direction++;
		if (y > y2) {
			intswap(y,y2);
			intswap(x,x2);
			exc_tile|=2;
		}
	} else if (y == y2) {
		if (x > x2) {
			intswap(y,y2);
			intswap(x,x2);
			exc_tile|=2;
		}
	} else
		return_cmd_error(STR_500A_START_AND_END_MUST_BE_IN);

	cost = 0;

	FindLandscapeHeight(&ti, x2, y2);
	end_tile = ti.tile;
	z = ti.z;

	if (exc_tile != 3) {
		if ( (direction ? 9U : 12U) != ti.tileh)
			return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);
		ret = DoCommandByTile(ti.tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (ret == CMD_ERROR)
			return CMD_ERROR;
		cost += ret;
	}
	cost += _price.build_tunnel;

	for(;;) {
		if (direction) y2-=16; else x2-=16;

		if (x2 == x && y2 == y)
			break;

		FindLandscapeHeight(&ti, x2, y2);
		if (ti.z <= z)
			return_cmd_error(STR_5002);

		if (!_cheats.crossing_tunnels.value && !CheckTunnelInWay(ti.tile, z))
			return CMD_ERROR;

		cost += _price.build_tunnel;
		cost += (cost >> 3);

		if (cost >= 400000000)
			cost = 400000000;
	}

	FindLandscapeHeight(&ti, x2, y2);
	if (ti.z != z)
		return_cmd_error(STR_5004);

	if (exc_tile != 1) {
		if ( (direction ? 6U : 3U) != ti.tileh)
			return_cmd_error(STR_1000_LAND_SLOPED_IN_WRONG_DIRECTION);

		ret = DoCommandByTile(ti.tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (ret == CMD_ERROR)
			return CMD_ERROR;
		cost += ret;
	}

	if (flags & DC_EXEC) {
		ModifyTile(ti.tile,
			MP_SETTYPE(MP_TUNNELBRIDGE) |
			MP_MAP3LO | MP_MAPOWNER_CURRENT | MP_MAP5,
			_build_tunnel_railtype, /* map3lo */
			((_build_tunnel_bh << 1) | 2) - direction /* map5 */
		);

		ModifyTile(end_tile,
			MP_SETTYPE(MP_TUNNELBRIDGE) |
			MP_MAP3LO | MP_MAPOWNER_CURRENT | MP_MAP5,
			_build_tunnel_railtype, /* map3lo */
			(_build_tunnel_bh << 1) | (direction ? 3:0)/* map5 */
		);

		UpdateSignalsOnSegment(end_tile, direction?7:1);
	}

	return cost + _price.build_tunnel;
}

/* Build Tunnel
 * x,y - start tile coord
 * p1 - railtype
 * p2 - ptr to uint that recieves end tile
 */
int32 CmdBuildTunnel(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	TileInfo ti, tiorg;
	int direction;
	uint z;
	static const int8 _build_tunnel_coord_mod[4+1] = { -16, 0, 16, 0, -16 };
	static const byte _build_tunnel_tileh[4] = {3, 9, 12, 6};
	uint excavated_tile;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	_build_tunnel_railtype = (byte)(p1 & 0xFF);
	_build_tunnel_bh = (byte)(p1 >> 8);

	_build_tunnel_endtile = 0;
	excavated_tile = 0;

	FindLandscapeHeight(&tiorg, x, y);

	if (!EnsureNoVehicle(tiorg.tile))
		return CMD_ERROR;

	if (!(direction=0, tiorg.tileh==12) &&
			!(direction++, tiorg.tileh==6) &&
			!(direction++, tiorg.tileh==3) &&
			!(direction++, tiorg.tileh==9) )
		return_cmd_error(STR_500B_SITE_UNSUITABLE_FOR_TUNNEL);

	z = tiorg.z;
	do {
		x += _build_tunnel_coord_mod[direction];
		y += _build_tunnel_coord_mod[direction+1];
		FindLandscapeHeight(&ti, x, y);
	} while (z != ti.z);
	_build_tunnel_endtile = ti.tile;


	if (!EnsureNoVehicle(ti.tile))
		return CMD_ERROR;

	if (ti.tileh != _build_tunnel_tileh[direction]) {
		if (DoCommandByTile(ti.tile, ti.tileh & ~_build_tunnel_tileh[direction], 0, flags, CMD_TERRAFORM_LAND) == CMD_ERROR)
			return_cmd_error(STR_5005_UNABLE_TO_EXCAVATE_LAND);
		excavated_tile = 1;
	}

	if (flags & DC_EXEC && DoBuildTunnel(x,y,tiorg.x,tiorg.y,flags&~DC_EXEC,excavated_tile) == CMD_ERROR)
		return CMD_ERROR;

	return DoBuildTunnel(x,y,tiorg.x, tiorg.y,flags,excavated_tile);
}

static const byte _updsignals_tunnel_dir[4] = { 5, 7, 1, 3};

uint CheckTunnelBusy(uint tile, int *length)
{
	int z = GetTileZ(tile);
	byte m5 = _map5[tile];
	int delta = TileOffsByDir(m5 & 3);
	int len = 0;
	uint starttile = tile;
	Vehicle *v;

	do {
		tile += delta;
		len++;
	} while (
		!IsTileType(tile, MP_TUNNELBRIDGE) ||
		(_map5[tile] & 0xF0) != 0 ||
		(byte)(_map5[tile] ^ 2) != m5 ||
		GetTileZ(tile) != z
	);

	if ((v=FindVehicleBetween(starttile, tile, z)) != NULL) {
		_error_message = v->type == VEH_Train ? STR_5000_TRAIN_IN_TUNNEL : STR_5001_ROAD_VEHICLE_IN_TUNNEL;
		return (uint)-1;
	}

	if (length) *length = len;
	return tile;
}

static int32 DoClearTunnel(uint tile, uint32 flags)
{
	Town *t;
	uint endtile;
	int length;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	// in scenario editor you can always destroy tunnels
	if (_game_mode != GM_EDITOR && !CheckTileOwnership(tile)) {
		if (!(_patches.extra_dynamite || _cheats.magic_bulldozer.value) || _map_owner[tile] != OWNER_TOWN)
			return CMD_ERROR;
	}

	endtile = CheckTunnelBusy(tile, &length);
	if (endtile == (uint)-1) return CMD_ERROR;

	_build_tunnel_endtile = endtile;

	t = ClosestTownFromTile(tile, (uint)-1); //needed for town rating penalty
	// check if you're allowed to remove the tunnel owned by a town
	// removal allowal depends on difficulty settings
	if(_map_owner[tile] == OWNER_TOWN && _game_mode != GM_EDITOR ) {
		if (!CheckforTownRating(tile, flags, t, TUNNELBRIDGE_REMOVE)) {
			SetDParam(0, t->index);
			return_cmd_error(STR_2009_LOCAL_AUTHORITY_REFUSES);
		}
	}

	if (flags & DC_EXEC) {
		// We first need to request the direction before calling DoClearSquare
		//  else the direction is always 0.. dah!! ;)
		byte tile_dir = _map5[tile]&3;
		byte endtile_dir = _map5[endtile]&3;
		DoClearSquare(tile);
		DoClearSquare(endtile);
		UpdateSignalsOnSegment(tile, _updsignals_tunnel_dir[tile_dir]);
		UpdateSignalsOnSegment(endtile, _updsignals_tunnel_dir[endtile_dir]);
		if (_map_owner[tile] == OWNER_TOWN && _game_mode != GM_EDITOR)
			ChangeTownRating(t, RATING_TUNNEL_BRIDGE_DOWN_STEP, RATING_TUNNEL_BRIDGE_MINIMUM);
	}
	return  _price.clear_tunnel * (length + 1);
}

static uint FindEdgesOfBridge(uint tile, uint *endtile)
{
	int direction = _map5[tile] & 1;
	uint start;

	// find start of bridge
	for(;;) {
		if (IsTileType(tile, MP_TUNNELBRIDGE) && (_map5[tile] & 0xE0) == 0x80)
			break;
		tile += direction ? TILE_XY(0,-1) : TILE_XY(-1,0);
	}

	start = tile;

	// find end of bridge
	for(;;) {
		if (IsTileType(tile, MP_TUNNELBRIDGE) && (_map5[tile] & 0xE0) == 0xA0)
			break;
		tile += direction ? TILE_XY(0,1) : TILE_XY(1,0);
	}

	*endtile = tile;

	return start;
}

static int32 DoClearBridge(uint tile, uint32 flags)
{
	uint endtile;
	Vehicle *v;
	Town *t;
	int direction;

	SET_EXPENSES_TYPE(EXPENSES_CONSTRUCTION);

	direction = _map5[tile]&1;

	/* delete stuff under the middle part if there's a transport route there..? */
	if ((_map5[tile] & 0xE0) == 0xE0) {
		int32 cost;

		// check if we own the tile below the bridge..
		if (_current_player != OWNER_WATER && (!CheckTileOwnership(tile) || !EnsureNoVehicleZ(tile, TilePixelHeight(tile))))
			return CMD_ERROR;

		cost = (_map5[tile] & 8) ? _price.remove_road * 2 : _price.remove_rail;

		if (flags & DC_EXEC) {
			_map5[tile] = _map5[tile] & ~0x38;
			_map_owner[tile] = OWNER_NONE;
			MarkTileDirtyByTile(tile);
		}
		return cost;

	/* delete canal under bridge */
	} else if(_map5[tile] == 0xC8 && TilePixelHeight(tile) != 0) {
		int32 cost;

		// check for vehicles under bridge
		if (!EnsureNoVehicleZ(tile, TilePixelHeight(tile)))
			return CMD_ERROR;
		cost = _price.clear_water;
		if (flags & DC_EXEC) {
			_map5[tile] = _map5[tile] & ~0x38;
			_map_owner[tile] = OWNER_NONE;
			MarkTileDirtyByTile(tile);
		}
		return cost;
	}

	tile = FindEdgesOfBridge(tile, &endtile);

	// floods, scenario editor can always destroy bridges
	if (_current_player != OWNER_WATER && _game_mode != GM_EDITOR && !CheckTileOwnership(tile)) {
		if (!(_patches.extra_dynamite || _cheats.magic_bulldozer.value) || _map_owner[tile] != OWNER_TOWN)
			return CMD_ERROR;
	}

	if (!EnsureNoVehicle(tile) || !EnsureNoVehicle(endtile))
		return CMD_ERROR;

	/*	Make sure there's no vehicle on the bridge
			Omit tile and endtile, since these are already checked, thus solving the problem
			of bridges over water, or higher bridges, where z is not increased, eg level bridge
	*/
	tile		+= direction ? TILE_XY(0, 1) : TILE_XY( 1,0);
	endtile	-= direction ? TILE_XY(0, 1) : TILE_XY( 1,0);
	/* Bridges on slopes might have their Z-value offset..correct this */
	if ((v = FindVehicleBetween(tile, endtile, TilePixelHeight(tile) + 8 + GetCorrectTileHeight(tile))) != NULL) {
		VehicleInTheWayErrMsg(v);
		return CMD_ERROR;
	}

	/* Put the tiles back to start/end position */
	tile		-= direction ? TILE_XY(0, 1) : TILE_XY( 1,0);
	endtile	+= direction ? TILE_XY(0, 1) : TILE_XY( 1,0);


	t = ClosestTownFromTile(tile, (uint)-1); //needed for town rating penalty
	// check if you're allowed to remove the bridge owned by a town.
	// removal allowal depends on difficulty settings
	if(_map_owner[tile] == OWNER_TOWN && _game_mode != GM_EDITOR) {
		if (!CheckforTownRating(tile, flags, t, TUNNELBRIDGE_REMOVE))
			return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		byte m5;
		uint c = tile;
		uint16 new_data;

		//checks if the owner is town then decrease town rating by RATING_TUNNEL_BRIDGE_DOWN_STEP until
		// you have a "Poor" (0) town rating
		if (_map_owner[tile] == OWNER_TOWN && _game_mode != GM_EDITOR)
			ChangeTownRating(t, RATING_TUNNEL_BRIDGE_DOWN_STEP, RATING_TUNNEL_BRIDGE_MINIMUM);

		do {
			m5 = _map5[c];

			if (m5 & 0x40) {
				if (m5 & 0x20) {
					static const uint16 _new_data_table[] = {0x1002, 0x1001, 0x2005, 0x200A, 0, 0, 0, 0};
					new_data = _new_data_table[((m5 & 0x18) >> 2) | (m5&1)];
				}	else {
					if (!(m5 & 0x18)) goto clear_it;
					new_data = 0x6000;
				}

				SetTileType(c, new_data >> 12);
				_map5[c] = (byte)new_data;
				_map2[c] = 0;

				MarkTileDirtyByTile(c);

			} else {
clear_it:;
				DoClearSquare(c);
			}
			c += direction ? TILE_XY(0,1) : TILE_XY(1,0);
		} while (c <= endtile);

		SetSignalsOnBothDir(tile, direction);
		SetSignalsOnBothDir(endtile, direction);

	}

	return ((((endtile - tile) >> (direction?8:0))&0xFF)+1) * _price.clear_bridge;
}

static int32 ClearTile_TunnelBridge(uint tile, byte flags) {
	byte m5 = _map5[tile];

	if ((m5 & 0xF0) == 0) {
		if (flags & DC_AUTO)
			return_cmd_error(STR_5006_MUST_DEMOLISH_TUNNEL_FIRST);

		return DoClearTunnel(tile, flags);
	} else if (m5 & 0x80) {
		if (flags & DC_AUTO)
			return_cmd_error(STR_5007_MUST_DEMOLISH_BRIDGE_FIRST);

		return DoClearBridge(tile, flags);
	} 
	
	return CMD_ERROR;
}

int32 DoConvertTunnelBridgeRail(uint tile, uint totype, bool exec)
{
	uint endtile;
	int length;
	Vehicle *v;

	if ((_map5[tile] & 0xFC) == 0x00) {
		// railway tunnel
		if (!CheckTileOwnership(tile)) return CMD_ERROR;

		if ( (uint)(_map3_lo[tile] & 0xF) == totype) return CMD_ERROR;

		endtile = CheckTunnelBusy(tile, &length);
		if (endtile == (uint)-1) return CMD_ERROR;

		if (exec) {
			_map3_lo[tile] = (_map3_lo[tile] & 0xF0) + totype;
			_map3_lo[endtile] = (_map3_lo[endtile] & 0xF0) + totype;
			MarkTileDirtyByTile(tile);
			MarkTileDirtyByTile(endtile);
		}
		return (length + 1) * (_price.build_rail >> 1);
	} else if ((_map5[tile] & 0xF8) == 0xE0) {
		// bridge middle part with rail below
		// only check for train under bridge
		if (!CheckTileOwnership(tile) || !EnsureNoVehicleZ(tile, TilePixelHeight(tile)))
			return CMD_ERROR;

		// tile is already of requested type?
		if ( (uint)(_map3_lo[tile] & 0xF) == totype) return CMD_ERROR;
		// change type.
		if (exec) {
			_map3_lo[tile] = (_map3_lo[tile] & 0xF0) + totype;
			MarkTileDirtyByTile(tile);
		}
		return _price.build_rail >> 1;
	} else if ((_map5[tile]&0xC6) == 0x80) {
		uint starttile;
		int32 cost;
		uint z = TilePixelHeight(tile);

		z += 8;

		if (!CheckTileOwnership(tile)) return CMD_ERROR;

		// railway bridge
		starttile = tile = FindEdgesOfBridge(tile, &endtile);
		// Make sure there's no vehicle on the bridge
		if ((v=FindVehicleBetween(tile, endtile, z)) != NULL) {
			VehicleInTheWayErrMsg(v);
			return CMD_ERROR;
		}

		if (!EnsureNoVehicle(starttile) || !EnsureNoVehicle(endtile)) {
			_error_message = STR_8803_TRAIN_IN_THE_WAY;
			return CMD_ERROR;
		}

		if ( (uint)(_map3_lo[tile] & 0xF) == totype) return CMD_ERROR;
		cost = 0;
		do {
			if (exec) {
				if (tile == starttile || tile == endtile) {
					_map3_lo[tile] = (_map3_lo[tile] & 0xF0) + totype;
				} else {
					_map3_lo[tile] = (_map3_lo[tile] & 0x0F) + (totype << 4);
				}
				MarkTileDirtyByTile(tile);
			}
			cost += (_price.build_rail>>1);
			tile += _map5[tile]&1 ? TILE_XY(0,1) : TILE_XY(1,0);
		} while (tile <= endtile);

		return cost;
	} else
		return CMD_ERROR;
}


// fast routine for getting the height of a middle bridge tile. 'tile' MUST be a middle bridge tile.
uint GetBridgeHeight(const TileInfo *ti)
{
	uint delta;
	TileInfo ti_end;
	uint tile = ti->tile;
	uint z_correction = 0;

	// find the end tile of the bridge.
	delta = (_map5[tile] & 1) ? TILE_XY(0,1) : TILE_XY(1,0);
	do {
		assert((_map5[tile] & 0xC0) == 0xC0);	// bridge and middle part
		tile += delta;
	} while (_map5[tile] & 0x40);	// while bridge middle parts

	// if the end of the bridge is on a tileh 7, the z coordinate is 1 tile too low
	// correct it.
	FindLandscapeHeightByTile(&ti_end, tile);
	if (HASBIT(1 << 7, ti_end.tileh))
		z_correction += 8;

	// return the height there (the height of the NORTH CORNER)
	return TilePixelHeight(tile) + z_correction;
}

static const byte _bridge_foundations[2][16] = {
// 0 1  2  3  4 5 6 7  8 9 10 11 12 13 14 15
	{1,16,18,3,20,5,0,7,22,0,10,11,12,13,14},
	{1,15,17,0,19,5,6,7,21,9,10,11, 0,13,14},
};

extern const byte _track_sloped_sprites[14];
extern const byte _road_sloped_sprites[14];

#include "table/bridge_land.h"
#include "table/tunnel_land.h"

static void DrawBridgePillars(TileInfo *ti, int x, int y, int z)
{
	const uint32 *b;
	uint32 image;
	int piece;

	b = _bridge_poles_table[_map2[ti->tile]>>4];

	// Draw first piece
	// (necessary for cantilever bridges)
	image = b[12 + (ti->map5&0x01)];
	piece = _map2[ti->tile]&0xF;
	if (image != 0 && piece != 0) {
		if (_display_opt & DO_TRANS_BUILDINGS) image = (image & 0x3FFF) | 0x03224000;
		DrawGroundSpriteAt(image, x, y, z);
	}

	image = b[(ti->map5&0x01)*6 + piece];

	if (image != 0) {
		int back_height, front_height, i=z;
		const byte *p;

		static const byte _tileh_bits[4][8] = {
			{2,1,8,4,  16,11,0,9},
			{1,8,4,2,  11,16,9,0},
			{4,8,1,2,  16,11,0,9},
			{2,4,8,1,  11,16,9,0},
		};

		if (_display_opt & DO_TRANS_BUILDINGS) image = (image & 0x3FFF) | 0x03224000;

		p = _tileh_bits[(image & 1) * 2 + (ti->map5&0x01)];
		front_height = ti->z + ((ti->tileh & p[0])?8:0);
		back_height = ti->z + ((ti->tileh & p[1])?8:0);
		if (ti->tileh & 0x10) {
			if (!(ti->tileh & p[2])) front_height += 8;
			if (!(ti->tileh & p[3])) back_height += 8;
		}

		for(; z>=front_height || z>=back_height; z=z-8) {
			if (z>=front_height) AddSortableSpriteToDraw(image, x,y, p[4], p[5], 0x28, z); // front facing pillar
			if (z>=back_height && z<i-8) AddSortableSpriteToDraw(image, x - p[6], y - p[7], p[4], p[5], 0x28, z); // back facing pillar
		}
	}
}

uint GetBridgeFoundation(uint tileh, byte direction) {
	int i;
	// normal level sloped building (7, 11, 13, 14)
	if (BRIDGE_FULL_LEVELED_FOUNDATION & (1 << tileh))
		return tileh;

	// inclined sloped building
	if (	((i=0, tileh == 1) || (i+=2, tileh == 2) || (i+=2, tileh == 4) || (i+=2, tileh == 8)) &&
				( direction == 0 || (i++, direction == 1)) )
		return i + 15;

	return 0;
}

static void DrawTile_TunnelBridge(TileInfo *ti)
{
	uint32 image;
	uint tmp;
	const uint32 *b;
	bool ice = _map3_hi[ti->tile] & 0x80;

	// draw tunnel?
	if ( (byte)(ti->map5&0xF0) == 0) {
		/* railway type */
		image = (_map3_lo[ti->tile] & 0xF) * 8;

		/* ice? */
		if (ice)
			image += 32;

		image += _draw_tunnel_table_1[(ti->map5 >> 2) & 0x3];
		image += (ti->map5 & 3) << 1;
		DrawGroundSprite(image);

		AddSortableSpriteToDraw(image+1, ti->x + 15, ti->y + 15, 1, 1, 8, (byte)ti->z);
	// draw bridge?
	} else if ((byte)ti->map5 & 0x80) {
		// get type of track on the bridge.
		tmp = _map3_lo[ti->tile];
		if (ti->map5 & 0x40) tmp >>= 4;
		tmp &= 0xF;

		// 0 = rail bridge
		// 1 = road bridge
		// 2 = monorail bridge
		// 3 = maglev bridge

		// add direction and fix stuff.
		if (tmp != 0) tmp++;
		tmp = (ti->map5&3) + (tmp*2);

		if (!(ti->map5 & 0x40)) {	// bridge ramps

			if (!(BRIDGE_NO_FOUNDATION & (1 << ti->tileh))) {	// no foundations for 0, 3, 6, 9, 12
				int f = GetBridgeFoundation(ti->tileh, ti->map5 & 0x1);	// pass direction
				if (f) DrawFoundation(ti, f);

				// default sloped sprites..
				if (ti->tileh != 0) image = _track_sloped_sprites[ti->tileh - 1] + 0x3F3;
			}

			// bridge ending.
			b = _bridge_sprite_table[_map2[ti->tile]>>4][6];
			b += (tmp&(3<<1))*4; /* actually ((tmp>>2)&3)*8 */
			b += (tmp&1); // direction
			if (ti->tileh == 0) b += 4; // sloped "entrance" ?
			if (ti->map5 & 0x20) b += 2; // which side

			image = *b;

			if (!ice) {
				DrawClearLandTile(ti, 3);
			} else {
				DrawGroundSprite(0x11C6 + _tileh_to_sprite[ti->tileh]);
			}

			// draw ramp
			if (_display_opt & DO_TRANS_BUILDINGS) image = (image & 0x3FFF) | 0x03224000;
			AddSortableSpriteToDraw(image, ti->x, ti->y, 16, 16, 7, ti->z);
		} else {
			// bridge middle part.
			uint z;
			int x,y;

			image = (ti->map5 >> 3) & 3;	// type of stuff under bridge (only defined for 0,1)
			assert(image <= 1);

			if (!(ti->map5 & 0x20)) {
				// draw land under bridge
				if (ice) image += 2;					// ice too?
				DrawGroundSprite(_bridge_land_below[image] + _tileh_to_sprite[ti->tileh]);

				// draw canal water?
				if (ti->map5 & 8 && ti->z != 0) DrawCanalWater(ti->tile);
			} else {
				// draw transport route under bridge

				// draw foundation?
				if (ti->tileh) {
					int f = _bridge_foundations[ti->map5&1][ti->tileh];
					if (f) DrawFoundation(ti, f);
				}

				if (!(image&1)) {
					// railway
					image = 0x3F3 + (ti->map5 & 1);
					if (ti->tileh != 0) image = _track_sloped_sprites[ti->tileh - 1] + 0x3F3;
					image += (_map3_lo[ti->tile] & 0xF) * TRACKTYPE_SPRITE_PITCH;
					if (ice) image += 26; // ice?
				} else {
					// road
					image = 1332 + (ti->map5 & 1);
					if (ti->tileh != 0) image = _road_sloped_sprites[ti->tileh - 1] + 0x53F;
					if (ice) image += 19; // ice?
				}
				DrawGroundSprite(image);
			}
			// get bridge sprites
			b = _bridge_sprite_table[_map2[ti->tile]>>4][_map2[ti->tile]&0xF] + tmp * 4;

			z = GetBridgeHeight(ti) + 5;

			// draw rail
			image = b[0];
			if (_display_opt & DO_TRANS_BUILDINGS) image = (image & 0x3FFF) | 0x03224000;
			AddSortableSpriteToDraw(image, ti->x, ti->y, (ti->map5&1)?11:16, (ti->map5&1)?16:11, 1, z);

			x = ti->x;
			y = ti->y;
			image = b[1];
			if (_display_opt & DO_TRANS_BUILDINGS) image = (image & 0x3FFF) | 0x03224000;

			// draw roof
			if (ti->map5&1) {
				x += 12;
				if (image&0x3FFF) AddSortableSpriteToDraw(image, x,y, 1, 16, 0x28, z);
			} else {
				y += 12;
				if (image&0x3FFF) AddSortableSpriteToDraw(image, x,y, 16, 1, 0x28, z);
			}

			if (ti->z + 5 == z ) {
				// draw poles below for small bridges
				image = b[2];
				if (image) {
					if (_display_opt & DO_TRANS_BUILDINGS) image = (image & 0x3FFF) | 0x03224000;
					DrawGroundSpriteAt(image, x, y, z);
				}
			} else if (_patches.bridge_pillars) {
				// draw pillars below for high bridges
				DrawBridgePillars(ti, x, y, z);
			}
		}
	}
}

static uint GetSlopeZ_TunnelBridge(TileInfo *ti) {
	uint z = ti->z;
	uint x = ti->x & 0xF;
	uint y = ti->y & 0xF;

	// swap directions if Y tunnel/bridge to let the code handle the X case only.
	if (ti->map5 & 1) intswap(x,y);

	// to the side of the tunnel/bridge?
	if (IS_INT_INSIDE(y, 5, 10+1)) {
		// tunnel?
		if ( (ti->map5 & 0xF0) == 0)
			return z;

		// bridge?
		if ( ti->map5 & 0x80 ) {
			// bridge ending?
			if (!(ti->map5 & 0x40)) {
				if (BRIDGE_FULL_LEVELED_FOUNDATION & (1 << ti->tileh))	// 7, 11, 13, 14
					z += 8;

				// no ramp for bridge ending
				if ((BRIDGE_PARTLY_LEVELED_FOUNDATION & (1 << ti->tileh) || BRIDGE_NO_FOUNDATION & (1 << ti->tileh)) && ti->tileh != 0) {
					return z + 8;

				} else if (!(ti->map5 & 0x20)) { // northern / southern ending
					// ramp
					return (z + (x>>1) + 1);
				} else {
					// ramp in opposite dir
					return (z + ((x^0xF)>>1));
				}

			// bridge middle part
			} else {
				// build on slopes?
				if (ti->tileh) z+=8;

				// keep the same elevation because we're on the bridge?
				if (_get_z_hint >= z + 8)
					return _get_z_hint;

				// actually on the bridge, but not yet in the shared area.
				if (!IS_INT_INSIDE(x, 5, 10+1))
					return GetBridgeHeight(ti) + 8;

				// in the shared area, assume that we're below the bridge, cause otherwise the hint would've caught it.
				// if rail or road below then it means it's possibly build on slope below the bridge.
				if (ti->map5 & 0x20) {
					uint f = _bridge_foundations[ti->map5&1][ti->tileh];
					// make sure that the slope is not inclined foundation
					if (IS_BYTE_INSIDE(f, 1, 15)) return z;

					// change foundation type?
					if (f) ti->tileh = _inclined_tileh[f - 15];
				}

				// no transport route, fallback to default
			}
		}
	} else {
		// if it's a bridge middle with transport route below, then we need to compensate for build on slopes
		if ( (ti->map5 & (0x80 + 0x40 + 0x20)) == (0x80 + 0x40 + 0x20)) {
			uint f;
			if (ti->tileh) z += 8;
			f = _bridge_foundations[ti->map5&1][ti->tileh];
			if (IS_BYTE_INSIDE(f, 1, 15)) return z;
			if (f) ti->tileh = _inclined_tileh[f - 15];
		}
	}

	// default case
	z = ti->z;
	return GetPartialZ(ti->x&0xF, ti->y&0xF, ti->tileh) + z;
}

static uint GetSlopeTileh_TunnelBridge(TileInfo *ti) {
	// not accurate, but good enough for slope graphics drawing
	return 0;
}


static void GetAcceptedCargo_TunnelBridge(uint tile, AcceptedCargo ac)
{
	/* not used */
}

static const StringID _bridge_tile_str[(MAX_BRIDGES + 3) + (MAX_BRIDGES + 3)] = {
	STR_501F_WOODEN_RAIL_BRIDGE,
	STR_5020_CONCRETE_RAIL_BRIDGE,
	STR_501C_STEEL_GIRDER_RAIL_BRIDGE,
	STR_501E_REINFORCED_CONCRETE_SUSPENSION,
	STR_501B_STEEL_SUSPENSION_RAIL_BRIDGE,
	STR_501B_STEEL_SUSPENSION_RAIL_BRIDGE,
	STR_501D_STEEL_CANTILEVER_RAIL_BRIDGE,
	STR_501D_STEEL_CANTILEVER_RAIL_BRIDGE,
	STR_501D_STEEL_CANTILEVER_RAIL_BRIDGE,
	STR_501C_STEEL_GIRDER_RAIL_BRIDGE,
	STR_5027_TUBULAR_RAIL_BRIDGE,
	STR_5027_TUBULAR_RAIL_BRIDGE,
	STR_5027_TUBULAR_RAIL_BRIDGE,
	0,0,0,

	STR_5025_WOODEN_ROAD_BRIDGE,
	STR_5026_CONCRETE_ROAD_BRIDGE,
	STR_5022_STEEL_GIRDER_ROAD_BRIDGE,
	STR_5024_REINFORCED_CONCRETE_SUSPENSION,
	STR_5021_STEEL_SUSPENSION_ROAD_BRIDGE,
	STR_5021_STEEL_SUSPENSION_ROAD_BRIDGE,
	STR_5023_STEEL_CANTILEVER_ROAD_BRIDGE,
	STR_5023_STEEL_CANTILEVER_ROAD_BRIDGE,
	STR_5023_STEEL_CANTILEVER_ROAD_BRIDGE,
	STR_5022_STEEL_GIRDER_ROAD_BRIDGE,
	STR_5028_TUBULAR_ROAD_BRIDGE,
	STR_5028_TUBULAR_ROAD_BRIDGE,
	STR_5028_TUBULAR_ROAD_BRIDGE,
	0,0,0,
};

static void GetTileDesc_TunnelBridge(uint tile, TileDesc *td)
{
	int delta;

	if ((_map5[tile] & 0x80) == 0) {
		td->str = STR_5017_RAILROAD_TUNNEL + ((_map5[tile] >> 2) & 3);
	} else {
		td->str = _bridge_tile_str[ (_map2[tile] >> 4) + (((_map5[tile]>>1)&3)<<4) ];

		/* scan to the end of the bridge, that's where the owner is stored */
		if (_map5[tile] & 0x40) {
			delta = _map5[tile] & 1 ? TILE_XY(0,-1) : TILE_XY(-1,0);
			do tile += delta; while (_map5[tile] & 0x40);
		}
	}
	td->owner = _map_owner[tile];
}


static void AnimateTile_TunnelBridge(uint tile)
{
	/* not used */
}

static void TileLoop_TunnelBridge(uint tile)
{
	if (_opt.landscape == LT_HILLY) {
		if ( GetTileZ(tile) > _opt.snow_line) {
			if (!(_map3_hi[tile] & 0x80)) {
				_map3_hi[tile] |= 0x80;
				MarkTileDirtyByTile(tile);
			}
		} else {
			if (_map3_hi[tile] & 0x80) {
				_map3_hi[tile] &= ~0x80;
				MarkTileDirtyByTile(tile);
			}
		}
	} else if (_opt.landscape == LT_DESERT) {
		if (GetMapExtraBits(tile) == 1 && !(_map3_hi[tile]&0x80)) {
			_map3_hi[tile] |= 0x80;
			MarkTileDirtyByTile(tile);
		}
	}

	// if it's a bridge with water below, call tileloop_water on it.
	if ((_map5[tile] & 0xF8) == 0xC8) TileLoop_Water(tile);
}

static void ClickTile_TunnelBridge(uint tile)
{
	/* not used */
}


static uint32 GetTileTrackStatus_TunnelBridge(uint tile, TransportType mode)
{
	uint32 result;
	byte m5 = _map5[tile];

	if ((m5 & 0xF0) == 0) {
		/* This is a tunnel */
		if (((m5 & 0xCU) >> 2) == mode) {
			/* Tranport in the tunnel is compatible */
			return m5&1 ? 0x202 : 0x101;
		}
	} else if (m5 & 0x80) {
		/* This is a bridge */
		result = 0;
		if (((m5 & 0x6U) >> 1) == mode) {
			/* Transport over the bridge is compatible */
			result = m5&1 ? 0x202 : 0x101;
		}
		if (m5 & 0x40) {
			/* Bridge middle part */
			if (!(m5 & 0x20)) {
				/* Clear ground or water underneath */
				if ((m5 & 0x18) != 8)
					/* Clear ground */
					return result;
				else
					if (mode != TRANSPORT_WATER)
						return result;
			} else {
				/* Transport underneath */
				if ((m5 & 0x18U) >> 3 != mode)
					/* Incompatible transport underneath */
					return result;
			}
			/* If we've not returned yet, there is a compatible
			 * transport or water beneath, so we can add it to
			 * result */
			/* Why is this xor'd ? Can't it just be or'd? */
			result ^= m5&1 ? 0x101 : 0x202;
		}
		return result;
	} else {
		assert(0); /* This should never occur */
	}
	return 0;
}

static void ChangeTileOwner_TunnelBridge(uint tile, byte old_player, byte new_player)
{
	if (_map_owner[tile] != old_player)
		return;

	if (new_player != 255) {
		_map_owner[tile] = new_player;
	}	else {
		if((_map5[tile] & 0xC0)==0xC0) {
			// the stuff BELOW the middle part is owned by the deleted player.
			if (!(_map5[tile] & (1 << 4 | 1 << 3))) {
				// convert railway into grass.
				_map5[tile] &= ~(1 << 5 | 1 << 4 | 1 << 3); // no transport route under bridge anymore..
			} else {
				// for road, change the owner of the road to local authority
				_map_owner[tile] = OWNER_NONE;
			}
		} else {
			DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	}
}


static const byte _tunnel_fractcoord_1[4] = {0x8E,0x18,0x81,0xE8};
static const byte _tunnel_fractcoord_2[4] = {0x81,0x98,0x87,0x38};
static const byte _tunnel_fractcoord_3[4] = {0x82,0x88,0x86,0x48};
static const byte _exit_tunnel_track[4] = {1,2,1,2};

static const byte _road_exit_tunnel_state[4] = {8, 9, 0, 1};
static const byte _road_exit_tunnel_frame[4] = {2, 7, 9, 4};

static const byte _tunnel_fractcoord_4[4] = {0x52, 0x85, 0x98, 0x29};
static const byte _tunnel_fractcoord_5[4] = {0x92, 0x89, 0x58, 0x25};
static const byte _tunnel_fractcoord_6[4] = {0x92, 0x89, 0x56, 0x45};
static const byte _tunnel_fractcoord_7[4] = {0x52, 0x85, 0x96, 0x49};

static uint32 VehicleEnter_TunnelBridge(Vehicle *v, uint tile, int x, int y)
{
	int z;
	int dir, vdir;
	byte fc;
	int h;

	if ((_map5[tile] & 0xF0) == 0) {
		z = GetSlopeZ(x, y) - v->z_pos;
		if (myabs(z) > 2)
			return 8;

		if (v->type == VEH_Train) {
			fc = (x&0xF)+(y<<4);

			dir = _map5[tile] & 3;
			vdir = v->direction >> 1;

			if (v->u.rail.track != 0x40 && dir == vdir) {
				if (v->subtype == TS_Front_Engine && fc == _tunnel_fractcoord_1[dir]) {
					if (v->spritenum < 4)
						SndPlayVehicleFx(SND_05_TRAIN_THROUGH_TUNNEL, v);
					return 0;
				}
				if (fc == _tunnel_fractcoord_2[dir]) {
					v->tile = tile;
					v->u.rail.track = 0x40;
					v->vehstatus |= VS_HIDDEN;
					return 4;
				}
			}

			if (dir == (vdir^2) && fc == _tunnel_fractcoord_3[dir] && z == 0) {
				/* We're at the tunnel exit ?? */
				v->tile = tile;
				v->u.rail.track = _exit_tunnel_track[dir];
				v->vehstatus &= ~VS_HIDDEN;
				return 4;
			}
		} else if (v->type == VEH_Road) {
			fc = (x&0xF)+(y<<4);
			dir = _map5[tile] & 3;
			vdir = v->direction >> 1;

			// Enter tunnel?
			if (v->u.road.state != 0xFF && dir == vdir) {
				if (fc == _tunnel_fractcoord_4[dir] ||
						fc == _tunnel_fractcoord_5[dir]) {

					v->tile = tile;
					v->u.road.state = 0xFF;
					v->vehstatus |= VS_HIDDEN;
					return 4;
				} else {
					return 0;
				}
			}

			if (dir == (vdir^2) && (
				/* We're at the tunnel exit ?? */
					fc == _tunnel_fractcoord_6[dir] ||
					fc == _tunnel_fractcoord_7[dir]) &&
					z == 0) {
				v->tile = tile;
				v->u.road.state = _road_exit_tunnel_state[dir];
				v->u.road.frame = _road_exit_tunnel_frame[dir];
				v->vehstatus &= ~VS_HIDDEN;
				return 4;
			}
		}
	} else if (_map5[tile] & 0x80) {
		if (v->type == VEH_Road || (v->type == VEH_Train && v->subtype == TS_Front_Engine)) {
			if (GetTileSlope(tile, &h) != 0)
				h += 8; // Compensate for possible foundation
			if (!(_map5[tile] & 0x40) || // start/end tile of bridge
					myabs(h - v->z_pos) > 2) { // high above the ground -> on the bridge
				/* modify speed of vehicle */
				uint16 spd = _bridge_speeds[_map2[tile] >> 4];
				if (v->type == VEH_Road) spd<<=1;
				if (spd < v->cur_speed)
					v->cur_speed = spd;
			}
		}
	}
	return 0;
}

uint GetVehicleOutOfTunnelTile(Vehicle *v)
{
	uint tile = v->tile;
	int delta_tile;
	byte z;

	/* locate either ending of the tunnel */
	delta_tile = (v->direction&2) ? TILE_XY(0,1) : TILE_XY(1,0);
	z = v->z_pos;
	for(;;) {
		TileInfo ti;
		FindLandscapeHeightByTile(&ti, tile);

		if (ti.type == MP_TUNNELBRIDGE && (ti.map5 & 0xF0)==0 && (byte)ti.z == z)
			break;

		tile += delta_tile;
	}
	return tile;
}

const TileTypeProcs _tile_type_tunnelbridge_procs = {
	DrawTile_TunnelBridge,					/* draw_tile_proc */
	GetSlopeZ_TunnelBridge,					/* get_slope_z_proc */
	ClearTile_TunnelBridge,					/* clear_tile_proc */
	GetAcceptedCargo_TunnelBridge,	/* get_accepted_cargo_proc */
	GetTileDesc_TunnelBridge,				/* get_tile_desc_proc */
	GetTileTrackStatus_TunnelBridge,/* get_tile_track_status_proc */
	ClickTile_TunnelBridge,					/* click_tile_proc */
	AnimateTile_TunnelBridge,				/* animate_tile_proc */
	TileLoop_TunnelBridge,					/* tile_loop_clear */
	ChangeTileOwner_TunnelBridge,		/* change_tile_owner_clear */
	NULL,														/* get_produced_cargo_proc */
	VehicleEnter_TunnelBridge,			/* vehicle_enter_tile_proc */
	NULL,														/* vehicle_leave_tile_proc */
	GetSlopeTileh_TunnelBridge,			/* get_slope_tileh_proc */
};
