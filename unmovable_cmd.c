#include "stdafx.h"
#include "ttd.h"
#include "command.h"
#include "viewport.h"
#include "player.h"
#include "gui.h"
#include "economy.h"
#include "town.h"

typedef struct DrawTileUnmovableStruct {
	uint16 image;
	byte subcoord_x;
	byte subcoord_y;
	byte width;
	byte height;
	byte z_size;
	byte unused;
} DrawTileUnmovableStruct;

typedef struct DrawTileSeqStruct {
	int8 delta_x;
	int8 delta_y;
	int8 delta_z;
	byte width,height;
	byte unk;
	SpriteID image;
} DrawTileSeqStruct;

#include "table/unmovable_land.h"

static void DrawTile_Unmovable(TileInfo *ti)
{
	uint32 image, ormod;
	
	if (!(ti->map5 & 0x80)) {
		if (ti->map5 == 2) {
			
			// statue
			DrawGroundSprite(0x58C);

			image = PLAYER_SPRITE_COLOR(_map_owner[ti->tile]);
			image += 0x8A48;
			if (!(_display_opt & DO_TRANS_BUILDINGS))
				image = (image & 0x3FFF) | 0x3224000;
			AddSortableSpriteToDraw(image, ti->x, ti->y, 16, 16, 25, ti->z);
		} else if (ti->map5 == 3) {
			
			// "owned by" sign
			DrawClearLandTile(ti, 0);
			
			AddSortableSpriteToDraw(
				PLAYER_SPRITE_COLOR(_map_owner[ti->tile]) + 0x92B6,
				ti->x+8, ti->y+8,
				1, 1, 
				10, 
				GetSlopeZ(ti->x+8, ti->y+8)
			);
		} else {
			// lighthouse or transmitter
			
			const DrawTileUnmovableStruct *dtus;

			if (ti->tileh) DrawFoundation(ti, ti->tileh);
			DrawClearLandTile(ti, 2);
			
			dtus = &_draw_tile_unmovable_data[ti->map5];		

			image = dtus->image;
			if (!(_display_opt & DO_TRANS_BUILDINGS))
				image = (image & 0x3FFF) | 0x3224000;
			
			AddSortableSpriteToDraw(image, 
				ti->x | dtus->subcoord_x,
				ti->y | dtus->subcoord_y,
				dtus->width, dtus->height,
				dtus->z_size, ti->z);
		}
	} else {
		const DrawTileSeqStruct *dtss;
		const byte *t;

		if (ti->tileh) DrawFoundation(ti, ti->tileh);

		ormod = PLAYER_SPRITE_COLOR(_map_owner[ti->tile]);

		t = _unmovable_display_datas[ti->map5 & 0x7F];
		DrawGroundSprite(*(uint16*)t | ormod);

		t += sizeof(uint16);
			
		for(dtss = (DrawTileSeqStruct *)t; (byte)dtss->delta_x != 0x80; dtss++) {
			image =	dtss->image;
			if (_display_opt & DO_TRANS_BUILDINGS) {
				image |= ormod;
			} else {
				image = (image & 0x3FFF) | 0x03224000;
			}
			AddSortableSpriteToDraw(image, ti->x + dtss->delta_x, ti->y + dtss->delta_y,
				dtss->width, dtss->height, dtss->unk, ti->z + dtss->delta_z);
		}
	}
}

static uint GetSlopeZ_Unmovable(TileInfo *ti) 
{
	return GetPartialZ(ti->x&0xF, ti->y&0xF, ti->tileh) + ti->z;
}

static uint GetSlopeTileh_Unmovable(TileInfo *ti) 
{
	return 0;
}

static int32 ClearTile_Unmovable(uint tile, byte flags)
{
	byte m5 = _map5[tile];
	//Town *t;
		
	if (m5 & 0x80) {
		if (_current_player == OWNER_WATER) 
			return DoCommandByTile(tile, OWNER_WATER, 0, flags, CMD_DESTROY_COMPANY_HQ);
		return_cmd_error(STR_5804_COMPANY_HEADQUARTERS_IN);
	}	

	if (m5 == 3)	// company owned land
		return DoCommandByTile(tile, 0, 0, flags, CMD_SELL_LAND_AREA);

	//t = ClosestTownFromTile(tile, _patches.dist_local_authority + 20); 	// needed for town penalty

	// checks if you're allowed to remove unmovable things. no remove under rating "%difficulty setting"
	if (_game_mode != GM_EDITOR) {	
		if (flags & DC_AUTO || !_cheats.magic_bulldozer.value)
			return_cmd_error(STR_5800_OBJECT_IN_THE_WAY);

		/*if (!CheckforTownRating(tile, flags, t, UNMOVEABLE_REMOVE)) 
			return CMD_ERROR;
		*/

	}
	
	if (flags & DC_EXEC) {	
		DoClearSquare(tile);
		// decreases the town rating by 250;
		/*if (_game_mode != GM_EDITOR) 
			ChangeTownRating(t, -250, -100);
		*/
	}

	// return _price.build_industry*0.34; 
	return 0;
}

static void GetAcceptedCargo_Unmovable(uint tile, AcceptedCargo *ac)
{
	byte m5 = _map5[tile];
	uint level; // HQ level (depends on company performance) in the range 1..5.

	if (!(m5 & 0x80)) {
		/* not used */
		return;
	}

	/* HQ accepts passenger and mail; but we have to divide the values
	 * between 4 tiles it occupies! */

	level = (m5 & ~0x80) / 4 + 1;

	// Top town building generates 10, so to make HQ interesting, the top
	// type makes 20.
	ac->type_1 = CT_PASSENGERS;
	ac->amount_1 = level;
	if (!ac->amount_1) ac->amount_1 = 1;

	// Top town building generates 4, HQ can make up to 8. The
	// proportion passengers:mail is different because such a huge
	// commercial building generates unusually high amount of mail
	// correspondence per physical visitor.
	ac->type_2 = CT_MAIL;
	ac->amount_2 = level / 2;
	if (!ac->amount_2) ac->amount_2 = 1;
}

static const StringID _unmovable_tile_str[] = {
	STR_5803_COMPANY_HEADQUARTERS,
	STR_5801_TRANSMITTER,
	STR_5802_LIGHTHOUSE,
	STR_2016_STATUE,
	STR_5805_COMPANY_OWNED_LAND,
};	

static void GetTileDesc_Unmovable(uint tile, TileDesc *td)
{
	int i = _map5[tile];
	if (i & 0x80) i = -1;
	td->str = _unmovable_tile_str[i + 1];
	td->owner = _map_owner[tile];
}

static void AnimateTile_Unmovable(uint tile)
{
	/* not used */
}

static void TileLoop_Unmovable(uint tile)
{
	byte m5 = _map5[tile];
	byte level; // HQ level (depends on company performance) in the range 1..5.
	uint32 r;

	if (!(m5 & 0x80)) {
		/* not used */
		return;
	}

	/* HQ accepts passenger and mail; but we have to divide the values
	 * between 4 tiles it occupies! */

	level = (m5 & ~0x80) / 4 + 1;
	assert(level < 6);

	r = Random();
	// Top town buildings generate 250, so the top HQ type makes 256.
	if ((byte) r < (256 / 4 /  (6 - level))) {
		uint amt = ((byte) r >> 3) / 4 + 1;
		if (_economy.fluct <= 0) amt = (amt + 1) >> 1;
		MoveGoodsToStation(tile, 2, 2, CT_PASSENGERS, amt);
	}

	r >>= 8;
	// Top town building generates 90, HQ can make up to 196. The
	// proportion passengers:mail is about the same as in the acceptance
	// equations.
	if ((byte) r < (196 / 4 / (6 - level))) {
		uint amt = ((byte) r >> 3) / 4 + 1;
		if (_economy.fluct <= 0) amt = (amt + 1) >> 1;
		MoveGoodsToStation(tile, 2, 2, CT_MAIL, amt);
	}
}


static uint32 GetTileTrackStatus_Unmovable(uint tile, int mode)
{
	return 0;
}

static void ClickTile_Unmovable(uint tile)
{
	if (_map5[tile] & 0x80) {
		ShowPlayerCompany(_map_owner[tile]);
	}
}

static const TileIndexDiff _tile_add[4] = {
	TILE_XY(1,0),
	TILE_XY(0,1),
	TILE_XY(-1,0),
	TILE_XY(0,-1),
};

/* checks, if a radio tower is within a 9x9 tile square around tile */
bool checkRadioTowerNearby(uint tile)
{
	uint tile_s;

	tile_s = TILE_XY( (int) GET_TILE_X(tile)-4, (int) GET_TILE_Y(tile)-4 );

	BEGIN_TILE_LOOP(tile, 9, 9, tile_s)
		// already a radio tower here?
		if (IS_TILETYPE(tile, MP_UNMOVABLE) && _map5[tile] == 0)
			return false;
	END_TILE_LOOP(tile, 9, 9, tile_s)
	return true;
}

void GenerateUnmovables()
{
	int i,j;
	uint tile;
	uint32 r;
	int dir;
	int h;

	if (_opt.landscape == LT_CANDY)
		return;

	/* add radio tower */
	i = 1000;
	j = 40; // limit of 40 radio towers per world.
	do {
		r = Random();
		tile = r % (TILES_X*TILES_Y);
//		TILE_MASK seems to be not working correctly. Radio masts accumulate in one area.
//		tile = TILE_MASK(r);
		if (IS_TILETYPE(tile, MP_CLEAR) && GetTileSlope(tile, &h) == 0 && h >= 32) {
			if(!checkRadioTowerNearby(tile))
				continue;
			_map_type_and_height[tile] |= MP_UNMOVABLE << 4;
			_map5[tile] = 0;
			_map_owner[tile] = OWNER_NONE;
			if (--j == 0)
				break;
		}
	} while (--i);

	if (_opt.landscape == LT_DESERT)
		return;
		
	/* add lighthouses */
	i = (Random()&3) + 7;
	do {
restart:
		r = Random();
		dir = r >> 30;
		r = r%((dir==0 || dir== 2)?TILE_Y_MAX:TILE_X_MAX);
		tile = 
          (dir==0)?TILE_XY(0,r):0 +             // left
          (dir==1)?TILE_XY(r,0):0 +             // top
          (dir==2)?TILE_XY(TILE_X_MAX,r):0 +    // right
          (dir==3)?TILE_XY(r,TILE_Y_MAX):0;     // bottom
		j = 20;
		do {
			if (--j == 0)
				goto restart;
			tile = TILE_MASK(tile + _tile_add[dir]);
		} while (!(IS_TILETYPE(tile, MP_CLEAR) && GetTileSlope(tile, &h) == 0 && h <= 16));
		
		assert(tile == TILE_MASK(tile));

		_map_type_and_height[tile] |= MP_UNMOVABLE << 4;
		_map5[tile] = 1;
		_map_owner[tile] = OWNER_NONE;
	} while (--i);
}

extern int32 CheckFlatLandBelow(uint tile, uint w, uint h, uint flags, uint invalid_dirs, int *);

/* p1				= relocate HQ
	 p1&0xFF	= player whose HQ is up for relocation
*/
int32 CmdBuildCompanyHQ(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile = TILE_FROM_XY(x,y);
	Player *p = DEREF_PLAYER(_current_player);
	int score;
	int32 cost = 0;
	
	if (CheckFlatLandBelow(tile, 2, 2, flags, 0, NULL) == CMD_ERROR)
		return CMD_ERROR;

	if (p1)
		cost = DoCommand(GET_TILE_X(p->location_of_house)*16, GET_TILE_Y(p->location_of_house)*16, p1&0xFF, 0, flags, CMD_DESTROY_COMPANY_HQ);

	if (flags & DC_EXEC) {
		score = UpdateCompanyRatingAndValue(p, false);

		p->location_of_house = tile;

		ModifyTile(tile + TILE_XY(0,0),
			MP_SETTYPE(MP_UNMOVABLE) | MP_MAPOWNER_CURRENT | MP_MAP5,
			0x80
		);

		ModifyTile(tile + TILE_XY(0,1),
			MP_SETTYPE(MP_UNMOVABLE) | MP_MAPOWNER_CURRENT | MP_MAP5,
			0x81
		);

		ModifyTile(tile + TILE_XY(1,0),
			MP_SETTYPE(MP_UNMOVABLE) | MP_MAPOWNER_CURRENT | MP_MAP5,
			0x82
		);

		ModifyTile(tile + TILE_XY(1,1),
			MP_SETTYPE(MP_UNMOVABLE) | MP_MAPOWNER_CURRENT | MP_MAP5,
			0x83
		);
		UpdatePlayerHouse(p, score);
		InvalidateWindow(WC_COMPANY, (int)p->index);
	}

	return cost;
}

/*	p1 = owner of the HQ */
int32 CmdDestroyCompanyHQ(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile = TILE_FROM_XY(x,y);
	Player *p;

	if ((int)p1 != OWNER_WATER)	// destruction was initiated by player
		p = DEREF_PLAYER((byte)p1);
	else {	// find player that has HQ flooded, and reset their location_of_house
		bool dodelete = false;
		FOR_ALL_PLAYERS(p) {
			if (p->location_of_house == tile) {
				dodelete = true;
				break;
			}
		}
		if (!dodelete)
			return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		p->location_of_house = 0; // reset HQ position
		DoClearSquare(tile + TILE_XY(0,0));
		DoClearSquare(tile + TILE_XY(0,1));
		DoClearSquare(tile + TILE_XY(1,0));
		DoClearSquare(tile + TILE_XY(1,1));
		InvalidateWindow(WC_COMPANY, (int)p->index);
	}

	// cost of relocating company is 1% of company value
	return CalculateCompanyValue(p) / 100;
}


static void ChangeTileOwner_Unmovable(uint tile, byte old_player, byte new_player)
{
	if (_map_owner[tile] != old_player)
		return;
	
	if (_map5[tile]==3 && new_player != 255) {
		_map_owner[tile] = new_player;
	}	else {
		DoClearSquare(tile);
	}
}

const TileTypeProcs _tile_type_unmovable_procs = {
	DrawTile_Unmovable,						/* draw_tile_proc */
	GetSlopeZ_Unmovable,					/* get_slope_z_proc */
	ClearTile_Unmovable,					/* clear_tile_proc */
	GetAcceptedCargo_Unmovable,		/* get_accepted_cargo_proc */
	GetTileDesc_Unmovable,				/* get_tile_desc_proc */
	GetTileTrackStatus_Unmovable,	/* get_tile_track_status_proc */
	ClickTile_Unmovable,					/* click_tile_proc */
	AnimateTile_Unmovable,				/* animate_tile_proc */
	TileLoop_Unmovable,						/* tile_loop_clear */
	ChangeTileOwner_Unmovable,		/* change_tile_owner_clear */
	NULL,													/* get_produced_cargo_proc */
	NULL,													/* vehicle_enter_tile_proc */
	NULL,													/* vehicle_leave_tile_proc */
	GetSlopeTileh_Unmovable,			/* get_slope_tileh_proc */
};
