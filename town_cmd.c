#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "map.h"
#include "viewport.h"
#include "town.h"
#include "command.h"
#include "pathfind.h"
#include "gfx.h"
#include "industry.h"
#include "station.h"
#include "player.h"
#include "news.h"
#include "saveload.h"
#include "economy.h"
#include "gui.h"
#include "network.h"

enum {
	TOWN_HAS_CHURCH     = 0x02,
	TOWN_HAS_STADIUM    = 0x04
};

// Local
static int _grow_town_result;

static bool BuildTownHouse(Town *t, uint tile);
static void ClearTownHouse(Town *t, uint tile);
static void DoBuildTownHouse(Town *t, uint tile);

typedef struct DrawTownTileStruct {
	uint32 sprite_1;
	uint32 sprite_2;

	byte subtile_x:4;
	byte subtile_y:4;
	byte width:4;
	byte height:4;
	byte dz;
	byte proc;
} DrawTownTileStruct;

#include "table/town_land.h"


static void TownDrawTileProc1(TileInfo *ti)
{
	AddChildSpriteScreen(0x5A3, 0xE, 0x3C - (_map_owner[ti->tile]&0x7F));
}

typedef void TownDrawTileProc(TileInfo *ti);
static TownDrawTileProc * const _town_draw_tile_procs[1] = {
	TownDrawTileProc1
};


static void DrawTile_Town(TileInfo *ti)
{
	const DrawTownTileStruct *dcts;
	byte z;
	uint32 image;

	/* Retrieve pointer to the draw town tile struct */
	{
		/* this "randomizes" on the (up to) 4 variants of a building */
		byte gfx   = (byte)_map2[ti->tile];
		byte stage = _map3_lo[ti->tile] >> 6;
		uint variant;
		variant  = ti->x >> 4;
		variant ^= ti->x >> 6;
		variant ^= ti->y >> 4;
		variant -= ti->y >> 6;
		variant &= 3;
		dcts = &_town_draw_tile_data[gfx << 4 | variant << 2 | stage];
	}

	z = ti->z;

	/* Add bricks below the house? */
	if (ti->tileh) {
		AddSortableSpriteToDraw(0x3DD + ti->tileh, ti->x, ti->y, 16, 16, 7, z);
		AddChildSpriteScreen(dcts->sprite_1, 0x1F, 1);
		z += 8;
	} else {
		/* Else draw regular ground */
		DrawGroundSprite(dcts->sprite_1);
	}

	/* Add a house on top of the ground? */
	if ((image = dcts->sprite_2) != 0) {
		if (_display_opt & DO_TRANS_BUILDINGS)
			image = (image & 0x3FFF) | 0x3224000;

		AddSortableSpriteToDraw(image,
			ti->x + dcts->subtile_x,
			ti->y + dcts->subtile_y,
			dcts->width + 1,
			dcts->height + 1,
			dcts->dz,
			z);

		if (_display_opt & DO_TRANS_BUILDINGS)
			return;
	}

	{
		int proc;
		if ((proc=dcts->proc-1) >= 0 )
			_town_draw_tile_procs[proc](ti);
	}
}

static uint GetSlopeZ_Town(TileInfo *ti)
{
	uint z = GetPartialZ(ti->x&0xF, ti->y&0xF, ti->tileh) + ti->z;
	if (ti->tileh != 0) z = (z & ~7) + 4;
	return (uint16) z;
}

static uint GetSlopeTileh_Town(TileInfo *ti)
{
	return ti->tileh;
}

static void AnimateTile_Town(uint tile)
{
	int old;
	int i;
	int a,b;

	if (_tick_counter & 3)
		return;

	if (_map2[tile] != 4 && _map2[tile] != 5)
		return;

	if (!((old=_map_owner[tile])&0x80)) {
		_map_owner[tile] |= 0x80;

		do {
			i = (Random()&7) - 1;
		} while (i < 0 || i == 1 || i*6==old);

		_map5[tile] = (_map5[tile] & ~0x3F) | i;
	}

	a = _map_owner[tile]&0x7F;
	b = (_map5[tile]&0x3F) * 6;
	a += (a < b) ? 1 : -1;
	_map_owner[tile] = (_map_owner[tile]&0x80)|a;

	if (a == b) {
		_map5[tile] &= 0x40;
		_map_owner[tile] &= 0x7F;
		DeleteAnimatedTile(tile);
	}

	MarkTileDirtyByTile(tile);
}

static void UpdateTownRadius(Town *t);

static bool IsCloseToTown(uint tile, uint dist)
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		if (t->xy != 0 && GetTileDist(tile, t->xy) < dist)
			return true;
	}
	return false;
}

static void MarkTownSignDirty(Town *t)
{
	MarkAllViewportsDirty(
		t->sign.left-6,
		t->sign.top-3,
		t->sign.left+t->sign.width_1*4+12,
		t->sign.top + 45
	);
}

void UpdateTownVirtCoord(Town *t)
{
	Point pt;

	MarkTownSignDirty(t);
	pt = RemapCoords2(GET_TILE_X(t->xy)*16, GET_TILE_Y(t->xy)*16);
	SetDParam(0, t->townnametype);
	SetDParam(1, t->townnameparts);
	SetDParam(2, t->population);
	UpdateViewportSignPos(&t->sign, pt.x, pt.y - 24, _patches.population_in_label ? STR_TOWN_LABEL_POP : STR_2001);
	MarkTownSignDirty(t);
}

static void ChangePopulation(Town *t, int mod)
{
	t->population += mod;
	InvalidateWindow(WC_TOWN_VIEW, t->index);
	UpdateTownVirtCoord(t);

	if (_town_sort_order & 2) _town_sort_dirty = true;
}

static void MakeSingleHouseBigger(uint tile)
{
	byte b;

	assert(IS_TILETYPE(tile, MP_HOUSE));

	b = _map5[tile];
	if (b & 0x80)
		return;

	_map5[tile] = (b & 0xC0) | ((b+1)&7);

	if ((_map5[tile]&7) != 0)
		return;

	_map3_lo[tile] = _map3_lo[tile] + 0x40;

	if ( (_map3_lo[tile] & 0xC0) == 0xC0) {
		Town *t = ClosestTownFromTile(tile, (uint)-1);
		ChangePopulation(t, _housetype_population[_map2[tile]]);
	}
	MarkTileDirtyByTile(tile);
}

static void MakeTownHouseBigger(uint tile)
{
	uint flags = _house_more_flags[_map2[tile]];
	if (flags & 8) MakeSingleHouseBigger(TILE_ADDXY(tile, 0, 0));
	if (flags & 4) MakeSingleHouseBigger(TILE_ADDXY(tile, 0, 1));
	if (flags & 2) MakeSingleHouseBigger(TILE_ADDXY(tile, 1, 0));
	if (flags & 1) MakeSingleHouseBigger(TILE_ADDXY(tile, 1, 1));
}

static void TileLoop_Town(uint tile)
{
	int house;
	Town *t;
	uint32 r;

	if ((_map3_lo[tile] & 0xC0) != 0xC0) {
		MakeTownHouseBigger(tile);
		return;
	}

	house = _map2[tile];
	if (_housetype_extra_flags[house] & 0x20 &&
			!(_map5[tile] & 0x80) &&
			CHANCE16(1,2) &&
			AddAnimatedTile(tile)) {
		_map5[tile] = (_map5[tile] & 0x40)|0x80;
	}

	t = ClosestTownFromTile(tile, (uint)-1);

	r = Random();

	if ( (byte)r < _housetype_population[house] ) {
		uint amt = ((byte)r >> 3) + 1, moved;
		if (_economy.fluct <= 0) amt = (amt + 1) >> 1;
		t->new_max_pass += amt;
		moved = MoveGoodsToStation(tile, 1, 1, CT_PASSENGERS, amt);
		t->new_act_pass += moved;
	}

	if ( (byte)(r>>8) < _housetype_mailamount[house] ) {
		uint amt = ((byte)(r>>8) >> 3) + 1, moved;
		if (_economy.fluct <= 0) amt = (amt + 1) >> 1;
		t->new_max_mail += amt;
		moved = MoveGoodsToStation(tile, 1, 1, CT_MAIL, amt);
		t->new_act_mail += moved;
	}

	if (_house_more_flags[house]&8 && (t->flags12&1) && --t->time_until_rebuild == 0) {
		r>>=16;
		t->time_until_rebuild = (r & 63) + 130;

		_current_player = OWNER_TOWN;

		ClearTownHouse(t, tile);

		// rebuild with another house?
		if ( (byte) (r >> 8) >= 12) {
			DoBuildTownHouse(t, tile);
		}

		_current_player = OWNER_NONE;
	}
}

static void ClickTile_Town(uint tile)
{
	/* not used */
}

static int32 ClearTile_Town(uint tile, byte flags)
{
	int house, rating;
	int32 cost;
	Town *t;

	// safety checks
	if (!EnsureNoVehicle(tile)) return CMD_ERROR;
	if (flags&DC_AUTO && !(flags&DC_AI_BUILDING)) return_cmd_error(STR_2004_BUILDING_MUST_BE_DEMOLISHED);

	house = _map2[tile];
	cost = _price.remove_house * _housetype_remove_cost[house] >> 8;

	rating = _housetype_remove_ratingmod[house];
	_cleared_town_rating += rating;
	_cleared_town = t = ClosestTownFromTile(tile, (uint)-1);

	if (_current_player < MAX_PLAYERS) {
		if (rating > t->ratings[_current_player] && !(flags & DC_NO_TOWN_RATING) && !_cheats.magic_bulldozer.value) {
			SetDParam(0, t->index);
			return_cmd_error(STR_2009_LOCAL_AUTHORITY_REFUSES);
		}
	}

	if (flags & DC_EXEC) {
		ChangeTownRating(t, -rating, -1000);
		ClearTownHouse(t, tile);
	}

	return cost;
}

static void GetAcceptedCargo_Town(uint tile, AcceptedCargo ac)
{
	int type = _map2[tile];

	ac[CT_PASSENGERS] = _housetype_cargo_passengers[type];
	ac[CT_MAIL] = _housetype_cargo_mail[type];
	ac[CT_GOODS] = _housetype_cargo_goods[type];
	ac[CT_FOOD] = _housetype_cargo_food[type];
}

static void GetTileDesc_Town(uint tile, TileDesc *td)
{
	td->str = _town_tile_names[_map2[tile]];
	if ((_map3_lo[tile] & 0xC0) != 0xC0) {
		SetDParamX(td->dparam, 0, td->str);
		td->str = STR_2058_UNDER_CONSTRUCTION;
	}

	td->owner = OWNER_TOWN;
}

static uint32 GetTileTrackStatus_Town(uint tile, TransportType mode)
{
	/* not used */
	return 0;
}

static void ChangeTileOwner_Town(uint tile, byte old_player, byte new_player)
{
	/* not used */
}


static const TileIndexDiffC _roadblock_tileadd[] = {
	{ 0, -1},
	{ 1,  0},
	{ 0,  1},
	{-1,  0},

	// Store the first 3 elements again.
	// Lets us rotate without using &3.
	{ 0, -1},
	{ 1,  0},
	{ 0,  1}
};

static void TownTickHandler(Town *t)
{
	if (t->flags12&1) {
		int i = t->grow_counter - 1;
		if (i < 0) {
			if (GrowTown(t)) {
				i = t->growth_rate;
			} else {
				i = 0;
			}
		}
		t->grow_counter = i;
	}

	UpdateTownRadius(t);
}

void OnTick_Town()
{
	uint i;
	Town *t;

	if (_game_mode == GM_EDITOR)
		return;

	i = _cur_town_ctr;
	t = GetTown(i);
	if (++i == _towns_size) i = 0;
	_cur_town_ctr = i;

	if (t->xy != 0)
		TownTickHandler(t);

}

static byte GetTownRoadMask(TileIndex tile)
{
	byte b = GetRoadBitsByTile(tile);
	byte r=0;
	if (b&1) r|=10;
	if (b&2) r|=5;
	if (b&4) r|=9;
	if (b&8) r|=6;
	if (b&16) r|=3;
	if (b&32) r|=12;
	return r;
}

static bool IsRoadAllowedHere(uint tile, int dir)
{
	uint k;
	uint slope;

	// If this assertion fails, it might be because the world contains
	//  land at the edges. This is not ok.
	TILE_ASSERT(tile);

	for(;;) {
		// Check if there already is a road at this point?
		if (GetRoadBitsByTile(tile) == 0) {
			// No, try to build one in the direction.
			// if that fails clear the land, and if that fails exit.
			// This is to make sure that we can build a road here later.
			if (DoCommandByTile(tile, (dir&1)?0xA:0x5, 0, DC_AUTO, CMD_BUILD_ROAD) == CMD_ERROR &&
					DoCommandByTile(tile, 0, 0, DC_AUTO, CMD_LANDSCAPE_CLEAR) == CMD_ERROR)
				return false;
		}

		slope = GetTileSlope(tile, NULL);
		if (slope == 0) {
no_slope:
			// Tile has no slope
			// Disallow the road if any neighboring tile has a road.
			if (HASBIT(GetTownRoadMask(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[dir+1]))), dir^2) ||
					HASBIT(GetTownRoadMask(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[dir+3]))), dir^2) ||
					HASBIT(GetTownRoadMask(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[dir+1]) + ToTileIndexDiff(_roadblock_tileadd[dir+2]))), dir) ||
					HASBIT(GetTownRoadMask(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[dir+3]) + ToTileIndexDiff(_roadblock_tileadd[dir+2]))), dir))
				return false;

			// Otherwise allow
			return true;
		}

		// If the tile is not a slope in the right direction, then
		// maybe terraform some.
		if ((k = (dir&1)?0xC:0x9) != slope && (k^0xF) != slope) {
			uint32 r = Random();

			if (CHANCE16I(1,8, r) && !_generating_world) {
				int32 res;

				if (CHANCE16I(1,16,r))
					res = DoCommandByTile(tile, slope, 0, DC_EXEC | DC_AUTO | DC_NO_WATER,
					                      CMD_TERRAFORM_LAND);
				else
					res = DoCommandByTile(tile, slope^0xF, 1, DC_EXEC | DC_AUTO | DC_NO_WATER,
					                      CMD_TERRAFORM_LAND);
				if (res == CMD_ERROR && CHANCE16I(1,3,r))
					// We can consider building on the slope, though.
					goto no_slope;
			}
			return false;
		}

		/* Can somebody explain for what this is needed? :s */
		// tile = TILE_ADD(tile, _roadblock_tileadd[dir]);
		return true;
	}
}

static bool TerraformTownTile(uint tile, int edges, int dir)
{
	int32 r;

	TILE_ASSERT(tile);

	r = DoCommandByTile(tile, edges, dir, DC_AUTO | DC_NO_WATER, CMD_TERRAFORM_LAND);
	if (r == CMD_ERROR || r >= 126*16)
		return false;
	DoCommandByTile(tile, edges, dir, DC_AUTO | DC_NO_WATER | DC_EXEC, CMD_TERRAFORM_LAND);
	return true;
}

static void LevelTownLand(uint tile)
{
	TileInfo ti;

	TILE_ASSERT(tile);

	// Don't terraform if land is plain or if there's a house there.
	FindLandscapeHeightByTile(&ti, tile);
	if (ti.tileh == 0 || ti.type == MP_HOUSE)
		return;

	// First try up, then down
	if (!TerraformTownTile(tile, ~ti.tileh & 0xF, 1)) {
		TerraformTownTile(tile, ti.tileh & 0xF, 0);
	}
}

#define IS_WATER_TILE(t) (IS_TILETYPE((t), MP_WATER) && _map5[(t)] == 0)

static void GrowTownInTile(uint *tile_ptr, uint mask, int block, Town *t1)
{
	Town *t2;
	uint16 r;
	int a,b,rcmd;
	uint tmptile;
	TileInfo ti;
	int i;
	int j;
	uint tile = *tile_ptr;

	TILE_ASSERT(tile);

	if (mask == 0) {
		// Tile has no road. First reset the status counter
		// to say that this is the last iteration.
		_grow_town_result = 0;

		// Then check if the tile we are at belongs to the town,
		// if not, bail out.
		t2 = ClosestTownFromTile(tile, (uint)-1);
		if (t2 != t1)
			return;

		// Remove hills etc
		LevelTownLand(tile);

		// Is a road allowed here?
		if (!IsRoadAllowedHere(tile, block))
			return;

		// Randomize new road block numbers
		a = block;
		b = block ^ 2;
		r = (uint16)Random();
		if (r <= 0x4000) do {
			a = (int)Random() & 3;
		} while(a == b);

		if (!IsRoadAllowedHere(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[a])), a)) {
			// A road is not allowed to continue the randomized road,
			//   return if the road we're trying to build is curved.
			if ( a != (b^2))
				return;

			// Return if neither side of the new road is a house
			if (!IS_TILETYPE(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[a + 1])), MP_HOUSE) &&
					!IS_TILETYPE(TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[a + 3])), MP_HOUSE))
				return;

			// That means that the road is only allowed if there is a house
			//  at any side of the new road.
		}
		rcmd = (1 << a) + (1 << b);

	} else if (block < 5 && !HASBIT(mask,block^2)) {
		// Continue building on a partial road.
		// Always OK.
		_grow_town_result = 0;
		rcmd = 1 << (block^2);
	} else {

		// Reached a tunnel? Then continue at the other side of it.
		if (IS_TILETYPE(tile, MP_TUNNELBRIDGE) && (_map5[tile]&~3)==4) {
			FindLengthOfTunnelResult flotr = FindLengthOfTunnel(tile, _map5[tile]&3);
			*tile_ptr = flotr.tile;
			return;
		}

		// For any other kind of tunnel/bridge, bail out.
		if (IS_TILETYPE(tile, MP_TUNNELBRIDGE))
			return;

		// Possibly extend the road in a direction.
		// Randomize a direction and if it has a road, bail out.
		i = (int)Random() & 3;
		if (HASBIT(mask, i))
			return;

		// This is the tile we will reach if we extend to this direction.
		tmptile = TILE_ADD(tile, ToTileIndexDiff(_roadblock_tileadd[i]));

		// Don't do it if it reaches to water.
		if (IS_WATER_TILE(tmptile))
			return;

		// If the new tile belongs to another town,
		//  then stop the search altogether.
		if (ClosestTownFromTile(tmptile, (uint)-1) != t1) {
			_grow_town_result = 0;
			return;
		}

		// Build a house at the edge. 60% chance or
		//  always ok if no road allowed.
		if (!IsRoadAllowedHere(tmptile, i) || CHANCE16(6,10)) {
			// But not if there already is a house there.
			if (!IS_TILETYPE(tmptile, MP_HOUSE)) {
				// Level the land if possible
				LevelTownLand(tmptile);

				// And build a house.
				// Set result to -1 if we managed to build it.
				if (BuildTownHouse(t1, tmptile))
					_grow_town_result = -1;
			}
			return;
		}

		_grow_town_result = 0;
		rcmd = 1 << i;
	}

	FindLandscapeHeightByTile(&ti, tile);

	// Return if a water tile
	if (ti.type == MP_WATER && ti.map5==0)
		return;

	// Determine direction of slope,
	//  and build a road if not a special slope.
	if ((i=0,ti.tileh != 3) &&
			(i++,ti.tileh != 9) &&
			(i++,ti.tileh != 12) &&
			(i++,ti.tileh != 6)) {
build_road_and_exit:
		if (DoCommandByTile(tile, rcmd, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD) != CMD_ERROR)
			_grow_town_result = -1;
		return;
	}

	tmptile = tile;

	// Now it contains the direction of the slope
	j = -11;	// max 11 tile long bridges
	do {
		if (++j == 0)
			goto build_road_and_exit;
		tmptile = TILE_MASK(tmptile + TileOffsByDir(i));
	} while (IS_WATER_TILE(tmptile));

	// no water tiles in between?
	if (j == -10)
		goto build_road_and_exit;

	// Quit if it selecting an appropiate bridge type fails a large number of times.
	j = 22;
	{
		int32 bridge_len = GetBridgeLength(tile, tmptile);
		do {
			byte bridge_type = RandomRange(MAX_BRIDGES - 1);
			if (CheckBridge_Stuff(bridge_type, bridge_len)) {
				if (DoCommandByTile(tile, tmptile, 0x8000 + bridge_type, DC_EXEC | DC_AUTO, CMD_BUILD_BRIDGE) != CMD_ERROR)
					_grow_town_result = -1;

				// obviously, if building any bridge would fail, there is no need to try other bridge-types
				return;
			}
		} while(--j != 0);
	}
}
#undef IS_WATER_TILE


// Returns true if a house was built, or no if the build failed.
static int GrowTownAtRoad(Town *t, uint tile)
{
	uint mask;
	int block = 5; // special case

	TILE_ASSERT(tile);

	// Number of times to search.
	_grow_town_result = 10 + t->num_houses * 4 / 9;

	do {
		// Get a bitmask of the road blocks on a tile
		mask = GetTownRoadMask(tile);

		// Try to grow the town from this point
		GrowTownInTile(&tile,mask,block,t);

		// Exclude the source position from the bitmask
		// and return if no more road blocks available
		CLRBIT(mask, (block ^ 2));
		if (mask == 0)
			return _grow_town_result;

		// Select a random bit from the blockmask, walk a step
		// and continue the search from there.
		do block = Random() & 3; while (!HASBIT(mask,block));
		tile += ToTileIndexDiff(_roadblock_tileadd[block]);

		// Max number of times is checked.
	} while (--_grow_town_result >= 0);

	return (_grow_town_result == -2);
}

// Generate a random road block
// The probability of a straight road
// is somewhat higher than a curved.
static int GenRandomRoadBits()
{
	uint32 r = Random();
	int a = r&3, b = (r >> 8) & 3;
	if (a == b) b ^= 2;
	return (1<<a)+(1<<b);
}

// Grow the town
// Returns true if a house was built, or no if the build failed.
bool GrowTown(Town *t)
{
	uint tile;
	const TileIndexDiffC *ptr;
	TileInfo ti;
	byte old_player;

	static const TileIndexDiffC _town_coord_mod[] = {
		{-1,  0},
		{ 1,  1},
		{ 1, -1},
		{-1, -1},
		{-1,  0},
		{ 0,  2},
		{ 2,  0},
		{ 0, -2},
		{-1, -1},
		{-2,  2},
		{ 2,  2},
		{ 2, -2},
		{ 0,  0}
	};

	// Current player is a town
	old_player = _current_player;
	_current_player = OWNER_TOWN;

	// Find a road that we can base the construction on.
	tile = t->xy;
	for (ptr = _town_coord_mod; ptr != endof(_town_coord_mod); ++ptr) {
		if (GetRoadBitsByTile(tile) != 0) {
			int r = GrowTownAtRoad(t, tile);
			_current_player = old_player;
			return r;
		}
		tile = TILE_ADD(tile, ToTileIndexDiff(*ptr));
	}

	// No road available, try to build a random road block by
	// clearing some land and then building a road there.
	tile = t->xy;
	for (ptr = _town_coord_mod; ptr != endof(_town_coord_mod); ++ptr) {
		FindLandscapeHeightByTile(&ti, tile);

		// Only work with plain land that not already has a house with map5=0
		if (ti.tileh == 0 && !(ti.type==MP_HOUSE && ti.map5==0)) {
			if (DoCommandByTile(tile, 0, 0, DC_AUTO, CMD_LANDSCAPE_CLEAR) != CMD_ERROR) {
				DoCommandByTile(tile, GenRandomRoadBits(), 0, DC_EXEC | DC_AUTO, CMD_BUILD_ROAD);
				_current_player = old_player;
				return true;
			}
		}
		tile = TILE_ADD(tile, ToTileIndexDiff(*ptr));
	}

	_current_player = old_player;
	return false;
}

static void UpdateTownRadius(Town *t)
{
	static const uint16 _town_radius_data[23][5] = {
		{ 4,  0,  0,  0,  0}, // 0
		{16,  0,  0,  0,  0},
		{25,  0,  0,  0,  0},
		{36,  0,  0,  0,  0},
		{49,  0,  4,  0,  0},
		{64,  0,  4,  0,  0}, // 20
		{64,  0,  9,  0,  1},
		{64,  0,  9,  0,  4},
		{64,  0, 16,  0,  4},
		{81,  0, 16,  0,  4},
		{81,  0, 16,  0,  4}, // 40
		{81,  0, 25,  0,  9},
		{81, 36, 25,  0,  9},
		{81, 36, 25, 16,  9},
		{81, 49,  0, 25,  9},
		{81, 64,  0, 25,  9}, // 60
		{81, 64,  0, 36,  9},
		{81, 64,  0, 36, 16},
		{100, 81,  0, 49, 16},
		{100, 81,  0, 49, 25},
		{121, 81,  0, 49, 25}, // 80
		{121, 81,  0, 49, 25},
		{121, 81,  0, 49, 36}, // 88
	};

	if (t->num_houses < 92) {
		memcpy(t->radius, _town_radius_data[t->num_houses / 4], sizeof(t->radius));
	} else {
		int mass = t->num_houses / 8;
		// At least very roughly extrapolate. Empirical numbers dancing between
		// overwhelming by cottages and skyscrapers outskirts.
		t->radius[0] = mass * mass;
		// Actually we are proportional to sqrt() but that's right because
		// we are covering an area.
		t->radius[1] = mass * 7;
		t->radius[2] = 0;
		t->radius[3] = mass * 4;
		t->radius[4] = mass * 3;
		//debug("%d (->%d): %d %d %d %d\n", t->num_houses, mass, t->radius[0], t->radius[1], t->radius[3], t->radius[4]);
	}
}

static void CreateTownName(Town *t1)
{
	Town *t2;
	char buf1[64];
	char buf2[64];
	uint32 r;

	t1->townnametype = SPECSTR_TOWNNAME_START + _opt.town_name;

	for(;;) {
restart:
		r = Random();

		SetDParam(0, r);
		GetString(buf1, t1->townnametype);

		// Check size and width
		if (strlen(buf1) >= 31 || GetStringWidth(buf1) > 130)
			continue;

		FOR_ALL_TOWNS(t2) {
			if (t2->xy != 0) {
				SetDParam(0, t2->townnameparts);
				GetString(buf2, t2->townnametype);
				if (str_eq(buf1, buf2))
					goto restart;
			}
		}
		t1->townnameparts = r;

		return;
	}
}

static void UpdateTownMaxPass(Town *t)
{
	t->max_pass = t->population >> 3;
	t->max_mail = t->population >> 4;
}

static void DoCreateTown(Town *t, TileIndex tile)
{
	int x, i;

	// clear the town struct
	i = t->index;
	memset(t, 0, sizeof(Town));
	t->index = i;

	t->xy = tile;
	t->num_houses = 0;
	t->time_until_rebuild = 10;
	UpdateTownRadius(t);
	t->flags12 = 0;
	t->population = 0;
	t->grow_counter = 0;
	t->growth_rate = 250;
	t->new_max_pass = 0;
	t->new_max_mail = 0;
	t->new_act_pass = 0;
	t->new_act_mail = 0;
	t->max_pass = 0;
	t->max_mail = 0;
	t->act_pass = 0;
	t->act_mail = 0;

	t->pct_pass_transported = 0;
	t->pct_mail_transported = 0;
	t->fund_buildings_months = 0;
	t->new_act_food = 0;
	t->new_act_water = 0;
	t->act_food = 0;
	t->act_water = 0;

	for(i = 0; i != MAX_PLAYERS; i++)
		t->ratings[i] = 500;

	t->have_ratings = 0;
	t->exclusivity = (byte)-1;
	t->exclusive_counter = 0;
	t->statues = 0;

	CreateTownName(t);

	UpdateTownVirtCoord(t);
	_town_sort_dirty = true;

	x = (Random() & 0xF) + 8;
	if (_game_mode == GM_EDITOR)
		x = _new_town_size * 16 + 3;

	t->num_houses += x;
	UpdateTownRadius(t);

	i = x * 4;
	do {
		GrowTown(t);
	} while (--i);

	t->num_houses -= x;
	UpdateTownRadius(t);
	UpdateTownMaxPass(t);
}

static Town *AllocateTown()
{
	Town *t;
	FOR_ALL_TOWNS(t) {
		if (t->xy == 0) {
			if (t->index > _total_towns) _total_towns = t->index;
			return t;
		}
	}
	return NULL;
}

int32 CmdBuildTown(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile = TILE_FROM_XY(x,y);
	TileInfo ti;
	Town *t;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	// Check if too close to the edge of map
	if (!CheckDistanceFromEdge(tile, 12))
		return_cmd_error(STR_0237_TOO_CLOSE_TO_EDGE_OF_MAP);

	// Can only build on clear flat areas.
	FindLandscapeHeightByTile(&ti, tile);
	if (ti.type != MP_CLEAR || ti.tileh != 0)
		return_cmd_error(STR_0239_SITE_UNSUITABLE);

	// Check distance to all other towns.
	if (IsCloseToTown(tile, 20))
		return_cmd_error(STR_0238_TOO_CLOSE_TO_ANOTHER_TOWN);

	// Allocate town struct
	t = AllocateTown();
	if (t == NULL)
		return_cmd_error(STR_023A_TOO_MANY_TOWNS);

	// Create the town
	if (flags & DC_EXEC) {
		_generating_world = true;
		DoCreateTown(t, tile);
		_generating_world = false;
	}
	return 0;
}

Town *CreateRandomTown()
{
	uint tile;
	TileInfo ti;
	Town *t;
	int n;

	// Try 20 times.
	n = 20;
	do {
		// Generate a tile index not too close from the edge
		tile = TILE_MASK(Random());
		if (!CheckDistanceFromEdge(tile, 20))
			continue;

		// Make sure the tile is plain
		FindLandscapeHeightByTile(&ti, tile);
		if (ti.type != MP_CLEAR || ti.tileh != 0)
			continue;

		// Check not too close to a town
		if (IsCloseToTown(tile, 20))
			continue;

		// Allocate a town struct
		t = AllocateTown();
		if (t == NULL)
			break;

		DoCreateTown(t, tile);
		return t;
	} while (--n);
	return NULL;
}

static const byte _num_initial_towns[3] = {
	11, 23, 46
};

void GenerateTowns()
{
	uint n;
	n = _num_initial_towns[_opt.diff.number_towns] + (Random()&7);
	do CreateRandomTown(); while (--n);
}

static bool CheckBuildHouseMode(Town *t1, uint tile, uint tileh, int mode) {
	Town *t2 = ClosestTownFromTile(tile, (uint)-1);
	int b;
	uint slope;

	static const byte _masks[8] = {
		0xC,0x3,0x9,0x6,
		0x3,0xC,0x6,0x9,
	};

	if (t2 != t1)
		return false;

	slope = GetTileSlope(tile, NULL);
	if (slope & 0x10)
		return false;

	b = 0;
	if ((slope & 0xF && ~slope & _masks[mode])) b = ~b;
	if ((tileh & 0xF && ~tileh & _masks[mode+4])) b = ~b;
	if (b)
		return false;

	return DoCommandByTile(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR) != CMD_ERROR;
}

int GetTownRadiusGroup(Town *t, uint tile)
{
	uint dist;
	int i,smallest;

	dist = GetTileDistAdv(tile, t->xy);
	if (t->fund_buildings_months && dist <= 25)
		return 4;

	smallest = 0;
	for(i=0; i!=lengthof(t->radius); i++) {
		if (dist < t->radius[i])
			smallest = i;
	}

	return smallest;
}

static bool CheckFree2x2Area(Town *t1, uint tile)
{
	Town *t;
	int i;

	static const TileIndexDiffC _tile_add[] = {
		{0    , 0    },
		{0 - 0, 1 - 0},
		{1 - 0, 0 - 1},
		{1 - 1, 1 - 0}
	};

	for(i=0; i!=4; i++) {
		tile += ToTileIndexDiff(_tile_add[i]);

		t = ClosestTownFromTile(tile, (uint)-1);
		if (t1 != t)
			return false;

		if (GetTileSlope(tile, NULL))
			return false;

		if (DoCommandByTile(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER | DC_FORCETEST, CMD_LANDSCAPE_CLEAR) == CMD_ERROR)
			return false;
	}

	return true;
}

static void DoBuildTownHouse(Town *t, uint tile)
{
	int i;
	uint bitmask;
	int house;
	uint slope;
	int z;
	uint oneof;

	// Above snow?
	slope = GetTileSlope(tile, &z);

	// Get the town zone type
	{
		uint rad = GetTownRadiusGroup(t, tile);

		int land = _opt.landscape;
		if (land == LT_HILLY && z >= _opt.snow_line)
			land = -1;

		bitmask = (1 << rad) + (1 << (land + 12));
	}

	// bits 0-4 are used
	// bits 11-15 are used
	// bits 5-10 are not used.
	{
		byte houses[lengthof(_housetype_flags)];
		int num = 0;

		// Generate a list of all possible houses that can be built.
		for(i=0; i!=lengthof(_housetype_flags); i++) {
			if ((~_housetype_flags[i] & bitmask) == 0)
				houses[num++] = (byte)i;
		}

		for(;;) {
			house = houses[RandomRange(num)];

			if (_cur_year < _housetype_years[house].min || _cur_year > _housetype_years[house].max)
				continue;

			// Special houses that there can be only one of.
			switch (house) {
				case HOUSE_TEMP_CHURCH:
				case HOUSE_ARCT_CHURCH:
				case HOUSE_SNOW_CHURCH:
				case HOUSE_TROP_CHURCH:
				case HOUSE_TOY_CHURCH:
					oneof = TOWN_HAS_CHURCH;
					break;
				case HOUSE_STADIUM:
				case HOUSE_MODERN_STADIUM:
					oneof = TOWN_HAS_STADIUM;
					break;
				default:
					oneof = 0;
					break;
			}

			if (t->flags12 & oneof)
				continue;

			// Make sure there is no slope?
			if (_housetype_extra_flags[house]&0x12 && slope)
				continue;

			if (_housetype_extra_flags[house]&0x10) {
				if (CheckFree2x2Area(t,tile) ||
						CheckFree2x2Area(t,(tile+=TILE_XY(-1,0))) ||
						CheckFree2x2Area(t,(tile+=TILE_XY(0,-1))) ||
						CheckFree2x2Area(t,(tile+=TILE_XY(1,0))))
							break;
				tile += TILE_XY(0,1);
			} else if (_housetype_extra_flags[house]&4) {
				if (CheckBuildHouseMode(t, tile+TILE_XY(1,0), slope, 0))
					break;

				if (CheckBuildHouseMode(t, tile+TILE_XY(-1,0), slope, 1)) {
					tile += TILE_XY(-1,0);
					break;
				}
			} else if (_housetype_extra_flags[house]&8) {
				if (CheckBuildHouseMode(t, tile+TILE_XY(0,1), slope, 2))
					break;

				if (CheckBuildHouseMode(t, tile+TILE_XY(0,-1), slope, 3)) {
					tile += TILE_XY(0,-1);
					break;
				}
			} else
				break;
		}
	}

	t->num_houses++;

	// Special houses that there can be only one of.
	t->flags12 |= oneof;

	{
		int m3lo,m5,eflags;

		// ENDING_2
		m3lo = 0;
		m5 = 0;
		if (_generating_world) {
			uint32 r = Random();

			// Value for map3lo
			m3lo = 0xC0;
			if ((byte)r >= 220) m3lo &= (r>>8);

			if (m3lo == 0xC0)
				ChangePopulation(t, _housetype_population[house]);

			// Initial value for map5.
			m5 = (r >> 16) & 0x3F;
		}

		assert(IS_TILETYPE(tile, MP_CLEAR));

		ModifyTile(tile,
			MP_SETTYPE(MP_HOUSE) | MP_MAP2 | MP_MAP3LO | MP_MAP3HI_CLEAR | MP_MAP5 | MP_MAPOWNER,
			house, /* map2 */
			m3lo,  /* map3_lo */
			0,     /* map_owner */
			m5		 /* map5 */
		);

		eflags = _housetype_extra_flags[house];

		if (eflags&0x18) {
			assert(IS_TILETYPE(tile + TILE_XY(0,1), MP_CLEAR));
			ModifyTile(tile + TILE_XY(0,1),
				MP_SETTYPE(MP_HOUSE) | MP_MAP2 | MP_MAP3LO | MP_MAP3HI_CLEAR | MP_MAP5 | MP_MAPOWNER,
				++house,	/* map2 */
				m3lo,			/* map3_lo */
				0,				/* map_owner */
				m5				/* map5 */
			);
		}

		if (eflags&0x14) {
			assert(IS_TILETYPE(tile + TILE_XY(1,0), MP_CLEAR));
			ModifyTile(tile + TILE_XY(1,0),
				MP_SETTYPE(MP_HOUSE) | MP_MAP2 | MP_MAP3LO | MP_MAP3HI_CLEAR | MP_MAP5 | MP_MAPOWNER,
				++house,	/* map2 */
				m3lo,			/* map3_lo */
				0,				/* map_owner */
				m5				/* map5 */
			);
		}

		if (eflags&0x10) {
			assert(IS_TILETYPE(tile + TILE_XY(1,1), MP_CLEAR));
			ModifyTile(tile + TILE_XY(1,1),
				MP_SETTYPE(MP_HOUSE) | MP_MAP2 | MP_MAP3LO | MP_MAP3HI_CLEAR | MP_MAP5 | MP_MAPOWNER,
				++house,	/* map2 */
				m3lo,			/* map3_lo */
				0,				/* map_owner */
				m5				/* map5 */
			);
		}
	}

	// ENDING
}

static bool BuildTownHouse(Town *t, uint tile)
{
	int32 r;

	// make sure it's possible
	if (!EnsureNoVehicle(tile)) return false;
	if (GetTileSlope(tile, NULL) & 0x10) return false;

	r = DoCommandByTile(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_LANDSCAPE_CLEAR);
	if (r == CMD_ERROR) return false;

	DoBuildTownHouse(t, tile);
	return true;
}


static void DoClearTownHouseHelper(uint tile)
{
	assert(IS_TILETYPE(tile, MP_HOUSE));
	DoClearSquare(tile);
	DeleteAnimatedTile(tile);
}

static void ClearTownHouse(Town *t, uint tile) {
	uint house = _map2[tile];
	uint eflags;

	assert(IS_TILETYPE(tile, MP_HOUSE));

	// need to align the tile to point to the upper left corner of the house
	if (house >= 3) { // house id 0,1,2 MUST be single tile houses, or this code breaks.
		if (_housetype_extra_flags[house-1] & 0x04) {
			house--;
			tile += TILE_XY(-1,0);
		} else if (_housetype_extra_flags[house-1] & 0x18) {
			house--;
			tile += TILE_XY(0,-1);
		} else if (_housetype_extra_flags[house-2] & 0x10) {
			house-=2;
			tile += TILE_XY(-1,0);
		} else if (_housetype_extra_flags[house-3] & 0x10) {
			house-=3;
			tile += TILE_XY(-1,-1);
		}
	}

	// Remove population from the town if the
	// house is finished.
	if ((~_map3_lo[tile] & 0xC0) == 0) {
		ChangePopulation(t, -_housetype_population[house]);
	}

	t->num_houses--;

	// Clear flags for houses that only may exist once/town.
	switch (house) {
		case HOUSE_TEMP_CHURCH:
		case HOUSE_ARCT_CHURCH:
		case HOUSE_SNOW_CHURCH:
		case HOUSE_TROP_CHURCH:
		case HOUSE_TOY_CHURCH:
			t->flags12 &= ~TOWN_HAS_CHURCH;
			break;
		case HOUSE_STADIUM:
		case HOUSE_MODERN_STADIUM:
			t->flags12 &= ~TOWN_HAS_STADIUM;
			break;
		default:
			break;
	}

	// Do the actual clearing of tiles
	eflags = _housetype_extra_flags[house];
	DoClearTownHouseHelper(tile);
	if (eflags & 0x14) DoClearTownHouseHelper(tile + TILE_XY(1,0));
	if (eflags & 0x18) DoClearTownHouseHelper(tile + TILE_XY(0,1));
	if (eflags & 0x10) DoClearTownHouseHelper(tile + TILE_XY(1,1));
}

int32 CmdRenameTown(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;
	Town *t = GetTown(p1);

	str = AllocateNameUnique((byte*)_decode_parameters, 4);
	if (str == 0)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		StringID old_str = t->townnametype;
		t->townnametype = str;
		DeleteName(old_str);

		UpdateTownVirtCoord(t);
		_town_sort_dirty = true;
		UpdateAllStationVirtCoord();
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}
	return 0;
}

// Called from GUI
void DeleteTown(Town *t)
{
	Industry *i;
	uint tile;

	// Delete town authority window
	//  and remove from list of sorted towns
	DeleteWindowById(WC_TOWN_VIEW, t->index);
	_town_sort_dirty = true;

	// Delete all industries belonging to the town
	FOR_ALL_INDUSTRIES(i) {
		if (i->xy && i->town == t)
			DeleteIndustry(i);
	}

	// Go through all tiles and delete those belonging to the town
	tile = 0;
	do {
		if (IS_TILETYPE(tile, MP_HOUSE)) {
			if (ClosestTownFromTile(tile, (uint)-1) == t) {
				DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
			}
		} else if (IS_TILETYPE(tile, MP_TUNNELBRIDGE) || IS_TILETYPE(tile, MP_STREET)) {
			if (_map_owner[tile] == OWNER_TOWN && ClosestTownFromTile(tile, (uint)-1) == t) {
				DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
			}
		}
	} while (++tile != MapSize());

	t->xy = 0;
	DeleteName(t->townnametype);

	MarkWholeScreenDirty();
}

// Called from GUI
void ExpandTown(Town *t)
{
	int amount, n;

	_generating_world = true;

	/* The more houses, the faster we grow */
	amount = RandomRange(t->num_houses / 10) + 3;
	t->num_houses += amount;
	UpdateTownRadius(t);

	n = amount * 10;
	do GrowTown(t); while (--n);

	t->num_houses -= amount;
	UpdateTownRadius(t);

	UpdateTownMaxPass(t);
	_generating_world = false;
}

const byte _town_action_costs[8] = {
	2, 4, 9, 35, 48, 53, 117, 175
};

typedef void TownActionProc(Town *t, int action);

static void TownActionAdvertise(Town *t, int action)
{
	static const byte _advertising_amount[3] = {0x40, 0x70, 0xA0};
	static const byte _advertising_radius[3] = {10,15,20};
	ModifyStationRatingAround(t->xy, _current_player,
		_advertising_amount[action],
		_advertising_radius[action]);
}

static void TownActionRoadRebuild(Town *t, int action)
{
	Player *p;

	t->road_build_months = 6;

	SetDParam(0, t->index);

	p = DEREF_PLAYER(_current_player);
	SetDParam(1, p->name_1);
	SetDParam(2, p->name_2);

	AddNewsItem(STR_2055_TRAFFIC_CHAOS_IN_ROAD_REBUILDING,
		NEWS_FLAGS(NM_NORMAL, NF_TILE, NT_GENERAL, 0), t->xy, 0);
}

static bool DoBuildStatueOfCompany(uint tile)
{
	TileInfo ti;
	byte old;
	int32 r;

	FindLandscapeHeightByTile(&ti, tile);
	if (ti.tileh != 0)
		return false;

	if (ti.type != MP_HOUSE && ti.type != MP_CLEAR && ti.type != MP_TREES)
		return false;


	old = _current_player;
	_current_player = OWNER_NONE;
	r = DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
	_current_player = old;

	if (r == CMD_ERROR)
		return false;

	ModifyTile(tile, MP_SETTYPE(MP_UNMOVABLE) | MP_MAPOWNER_CURRENT | MP_MAP5,
		2 /* map5 */
	);

	return true;
}

static void TownActionBuildStatue(Town *t, int action)
{
	// Layouted as an outward spiral
	static const TileIndexDiffC _statue_tiles[] = {
		{-1, 0},
		{ 0, 1},
		{ 1, 0}, { 1, 0},
		{ 0,-1}, { 0,-1},
		{-1, 0}, {-1, 0}, {-1, 0},
		{ 0, 1}, { 0, 1}, { 0, 1},
		{ 1, 0}, { 1, 0}, { 1, 0}, { 1, 0},
		{ 0,-1}, { 0,-1}, { 0,-1}, { 0,-1},
		{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0},
		{ 0, 1}, { 0, 1}, { 0, 1}, { 0, 1}, { 0, 1},
		{ 1, 0}, { 1, 0}, { 1, 0}, { 1, 0}, { 1, 0}, { 1, 0},
		{ 0,-1}, { 0,-1}, { 0,-1}, { 0,-1}, { 0,-1}, { 0,-1},
		{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0},
		{ 0, 1}, { 0, 1}, { 0, 1}, { 0, 1}, { 0, 1}, { 0, 1}, { 0, 1},
		{ 1, 0}, { 1, 0}, { 1, 0}, { 1, 0}, { 1, 0}, { 1, 0}, { 1, 0}, { 1, 0},
		{ 0,-1}, { 0,-1}, { 0,-1}, { 0,-1}, { 0,-1}, { 0,-1}, { 0,-1}, { 0,-1},
		{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0},
		{ 0, 0}
	};
	uint tile = t->xy;
	const TileIndexDiffC *p;

	SETBIT(t->statues, _current_player);

	for (p = _statue_tiles; p != endof(_statue_tiles); ++p) {
		if (DoBuildStatueOfCompany(tile))
			return;
		tile = TILE_ADD(tile, ToTileIndexDiff(*p));
	}
}

static void TownActionFundBuildings(Town *t, int action)
{
	t->grow_counter = 1;
	t->flags12 |= 1;
	t->fund_buildings_months = 3;
}

static void TownActionBuyRights(Town *t, int action)
{
	t->exclusive_counter = 12;
	t->exclusivity = _current_player;

	ModifyStationRatingAround(t->xy, _current_player, 130, 17);
}

static void TownActionBribe(Town *t, int action)
{
	if (!RandomRange(15)) {
		GoodsEntry *ge;
		Station *st;
		int i, rating;

		// set as unwanted for 6 months
		t->unwanted[_current_player] = 6;

		// set all close by station ratings to 0
		FOR_ALL_STATIONS(st) {
			if (st->town == t && st->owner == _current_player) {
				for (i=0, ge = st->goods; i != NUM_CARGO; i++, ge++)
					ge->rating = 0;
			}
		}

		// only show errormessage to the executing player. All errors are handled command.c
		// but this is special, because it can only 'fail' on a DC_EXEC
		if (!_networking || (_current_player == _local_player))
			ShowErrorMessage(STR_BRIBE_FAILED_2, STR_BRIBE_FAILED, 0, 0);

		/*	decrease by a lot!
		 *	ChangeTownRating is only for stuff in demolishing. Bribe failure should
		 *	be independent of any cheat settings
		 *	ChangeTownRating(c, -1000, -50);
		 */
		rating = t->ratings[_current_player];
		if (rating > -50)
			t->ratings[_current_player] = -50;
	} else {
		ChangeTownRating(t, 200, 800);
	}
}

static TownActionProc * const _town_action_proc[] = {
	TownActionAdvertise,
	TownActionAdvertise,
	TownActionAdvertise,
	TownActionRoadRebuild,
	TownActionBuildStatue,
	TownActionFundBuildings,
	TownActionBuyRights,
	TownActionBribe
};

// p1 = town
// p2 = action
int32 CmdDoTownAction(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	cost = (_price.build_industry >> 8) * _town_action_costs[p2];

	if (flags & DC_EXEC) {
		_town_action_proc[p2](GetTown(p1), p2);
		InvalidateWindow(WC_TOWN_AUTHORITY, p1);
	}

	return cost;
}

static void UpdateTownGrowRate(Town *t)
{
	int n;
	Station *st;
	byte m;
	Player *p;

	// Reset player ratings if they're low
	FOR_ALL_PLAYERS(p) {
		if (p->is_active && t->ratings[p->index] <= 200) {
			t->ratings[p->index] += 5;
		}
	}

	n = 0;
	FOR_ALL_STATIONS(st) {
		if (GetTileDistAdv(st->xy, t->xy) <= t->radius[0]) {
			if (st->time_since_load <= 20 || st->time_since_unload <= 20) {
				n++;
				if (st->owner < MAX_PLAYERS && t->ratings[st->owner] <= 1000-12)
					t->ratings[st->owner] += 12;
			} else {
				if (st->owner < MAX_PLAYERS && t->ratings[st->owner] >= -1000+15)
					t->ratings[st->owner] -= 15;
			}
		}
	}

	t->flags12 &= ~1;

	if (t->fund_buildings_months != 0) {
		static const byte _grow_count_values[6] = {
			60, 60, 60, 50, 40, 30
		};
		m = _grow_count_values[min(n, 5)];
		t->fund_buildings_months--;
	} else if (n == 0) {
		m = 160;
		if (!CHANCE16(1, 12))
			return;
	} else {
		static const byte _grow_count_values[5] = {
			210, 150, 110, 80, 50
		};
		m = _grow_count_values[min(n, 5) - 1];
	}

	if (_opt.landscape == LT_HILLY) {
 		if (GET_TILEHEIGHT(t->xy) >= _opt.snow_line && t->act_food == 0 && t->population > 90)
			return;
	} else if (_opt.landscape == LT_DESERT) {
 		if (GetMapExtraBits(t->xy) == 1 && (t->act_food==0 || t->act_water==0) && t->population > 60)
			return;
	}

  	t->growth_rate = m / (t->num_houses / 50 + 1);
	if (m <= t->grow_counter)
		t->grow_counter = m;

	t->flags12 |= 1;
}

static void UpdateTownAmounts(Town *t)
{
	// Using +1 here to prevent overflow and division by zero
	t->pct_pass_transported = t->new_act_pass * 256 / (t->new_max_pass + 1);

	t->max_pass = t->new_max_pass; t->new_max_pass = 0;
	t->act_pass = t->new_act_pass; t->new_act_pass = 0;
	t->act_food = t->new_act_food; t->new_act_food = 0;
	t->act_water = t->new_act_water; t->new_act_water = 0;

	// Using +1 here to prevent overflow and division by zero
	t->pct_mail_transported = t->new_act_mail * 256 / (t->new_max_mail + 1);
	t->max_mail = t->new_max_mail; t->new_max_mail = 0;
	t->act_mail = t->new_act_mail; t->new_act_mail = 0;

	InvalidateWindow(WC_TOWN_VIEW, t->index);
}

static void UpdateTownUnwanted(Town *t)
{
	Player *p;

	FOR_ALL_PLAYERS(p) {
		if (t->unwanted[p->index] > 0)
			t->unwanted[p->index]--;
	}
}

bool CheckIfAuthorityAllows(uint tile)
{
	Town *t;

	if (_current_player >= MAX_PLAYERS)
		return true;

	t = ClosestTownFromTile(tile, _patches.dist_local_authority);
	if (t == NULL)
		return true;

	if (t->ratings[_current_player] > -200)
		return true;

	_error_message = STR_2009_LOCAL_AUTHORITY_REFUSES;
	SetDParam(0, t->index);

	return false;
}


Town *ClosestTownFromTile(uint tile, uint threshold)
{
	Town *t;
	uint dist, best = threshold;
	Town *best_town = NULL;

	FOR_ALL_TOWNS(t) {
		if (t->xy != 0) {
			dist = GetTileDist(tile, t->xy);
			if (dist < best) {
				best = dist;
				best_town = t;
			}
		}
	}

	return best_town;
}

void ChangeTownRating(Town *t, int add, int max)
{
	int rating;

	//	if magic_bulldozer cheat is active, town doesn't penaltize for removing stuff
	if (t == NULL || _current_player >= MAX_PLAYERS || (_cheats.magic_bulldozer.value && add < 0)	)
		return;

	SETBIT(t->have_ratings, _current_player);

	rating = t->ratings[_current_player];

	if (add < 0) {
		if (rating > max) {
			rating += add;
			if (rating < max) rating = max;
		}
	} else {
		if (rating < max) {
			rating += add;
			if (rating > max) rating = max;
		}
	}
	t->ratings[_current_player] = rating;
}

/*	penalty for removing town-owned stuff */
static const int _default_rating_settings [3][3] = {
	// ROAD_REMOVE, TUNNELBRIDGE_REMOVE, INDUSTRY_REMOVE
	{  0, 128, 384},	// Permissive
	{ 48, 192, 480},	// Neutral
	{ 96, 384, 768},	// Hostile
};

bool CheckforTownRating(uint tile, uint32 flags, Town *t, byte type)
{
	int modemod;

	//	if magic_bulldozer cheat is active, town doesn't restrict your destructive actions
	if (t == NULL || _current_player >= MAX_PLAYERS || _cheats.magic_bulldozer.value)
		return true;

	/*	check if you're allowed to remove the street/bridge/tunnel/industry
	 *	owned by a town	no removal if rating is lower than ... depends now on
	 *	difficulty setting. Minimum town rating selected by difficulty level
	 */
	modemod = _default_rating_settings[_opt_mod_ptr->diff.town_council_tolerance][type];

	if (t->ratings[_current_player] < 16 + modemod && !(flags & DC_NO_TOWN_RATING)) {
		SetDParam(0, t->index);
		_error_message = STR_2009_LOCAL_AUTHORITY_REFUSES;
		return false;
	}

	return true;
}

void TownsMonthlyLoop()
{
	Town *t;

	FOR_ALL_TOWNS(t) if (t->xy != 0) {
		if (t->road_build_months != 0)
			t->road_build_months--;

		if (t->exclusive_counter != 0)
			if(--t->exclusive_counter==0)
				t->exclusivity = (byte)-1;

		UpdateTownGrowRate(t);
		UpdateTownAmounts(t);
		UpdateTownUnwanted(t);
	}
}

void InitializeTowns()
{
	Subsidy *s;
	Town *t;
	int i;

	memset(_towns, 0, sizeof(_towns[0]) * _towns_size);

	i = 0;
	FOR_ALL_TOWNS(t)
		t->index = i++;

	memset(_subsidies, 0, sizeof(_subsidies));
	for (s=_subsidies; s != endof(_subsidies); s++)
		s->cargo_type = 0xFF;

	_cur_town_ctr = 0;
	_town_sort_dirty = true;
	_total_towns = 0;
}

const TileTypeProcs _tile_type_town_procs = {
	DrawTile_Town,						/* draw_tile_proc */
	GetSlopeZ_Town,						/* get_slope_z_proc */
	ClearTile_Town,						/* clear_tile_proc */
	GetAcceptedCargo_Town,		/* get_accepted_cargo_proc */
	GetTileDesc_Town,					/* get_tile_desc_proc */
	GetTileTrackStatus_Town,	/* get_tile_track_status_proc */
	ClickTile_Town,						/* click_tile_proc */
	AnimateTile_Town,					/* animate_tile_proc */
	TileLoop_Town,						/* tile_loop_clear */
	ChangeTileOwner_Town,			/* change_tile_owner_clear */
	NULL,											/* get_produced_cargo_proc */
	NULL,											/* vehicle_enter_tile_proc */
	NULL,											/* vehicle_leave_tile_proc */
	GetSlopeTileh_Town,				/* get_slope_tileh_proc */
};


// Save and load of towns.
static const byte _town_desc[] = {
	SLE_VAR(Town,xy,					SLE_UINT16),

	SLE_CONDVAR(Town,population,	SLE_FILE_U16 | SLE_VAR_U32, 0, 2),
	SLE_CONDVAR(Town,population,	SLE_UINT32, 3, 255),


	SLE_VAR(Town,num_houses,	SLE_UINT16),
	SLE_VAR(Town,townnametype,SLE_UINT16),
	SLE_VAR(Town,townnameparts,SLE_UINT32),

	SLE_VAR(Town,flags12,			SLE_UINT8),
	SLE_VAR(Town,statues,			SLE_UINT8),

	// sort_index_obsolete was stored here in savegame format 0 - 1
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 1, 0, 1),

	SLE_VAR(Town,have_ratings,SLE_UINT8),
	SLE_ARR(Town,ratings,			SLE_INT16, 8),
	// failed bribe attempts are stored since savegame format 4
	SLE_CONDARR(Town,unwanted,			SLE_INT8, 8, 4,255),

	SLE_VAR(Town,max_pass,		SLE_UINT16),
	SLE_VAR(Town,max_mail,		SLE_UINT16),
	SLE_VAR(Town,new_max_pass,SLE_UINT16),
	SLE_VAR(Town,new_max_mail,SLE_UINT16),
	SLE_VAR(Town,act_pass,		SLE_UINT16),
	SLE_VAR(Town,act_mail,		SLE_UINT16),
	SLE_VAR(Town,new_act_pass,SLE_UINT16),
	SLE_VAR(Town,new_act_mail,SLE_UINT16),

	SLE_VAR(Town,pct_pass_transported,SLE_UINT8),
	SLE_VAR(Town,pct_mail_transported,SLE_UINT8),

	SLE_VAR(Town,act_food,		SLE_UINT16),
	SLE_VAR(Town,act_water,		SLE_UINT16),
	SLE_VAR(Town,new_act_food,SLE_UINT16),
	SLE_VAR(Town,new_act_water,SLE_UINT16),

	SLE_VAR(Town,time_until_rebuild,		SLE_UINT8),
	SLE_VAR(Town,grow_counter,					SLE_UINT8),
	SLE_VAR(Town,growth_rate,						SLE_UINT8),
	SLE_VAR(Town,fund_buildings_months,	SLE_UINT8),
	SLE_VAR(Town,road_build_months,			SLE_UINT8),

	SLE_VAR(Town,exclusivity,						SLE_UINT8),
	SLE_VAR(Town,exclusive_counter,			SLE_UINT8),
	// reserve extra space in savegame here. (currently 30 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 30, 2, 255),

	SLE_END()
};

static void Save_TOWN()
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		if (t->xy != 0) {
			SlSetArrayIndex(t->index);
			SlObject(t, _town_desc);
		}
	}
}

static void Load_TOWN()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Town *t = GetTown(index);

		SlObject(t, _town_desc);
		if (index > _total_towns) _total_towns = index;
	}
}

void AfterLoadTown()
{
	Town *t;
	FOR_ALL_TOWNS(t) {
		if (t->xy != 0) {
			UpdateTownRadius(t);
			UpdateTownVirtCoord(t);
		}
	}
	_town_sort_dirty = true;
}


const ChunkHandler _town_chunk_handlers[] = {
	{ 'CITY', Save_TOWN, Load_TOWN, CH_ARRAY | CH_LAST},
};


