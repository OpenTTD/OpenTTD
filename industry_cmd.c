#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "map.h"
#include "viewport.h"
#include "command.h"
#include "industry.h"
#include "town.h"
#include "vehicle.h"
#include "news.h"
#include "saveload.h"
#include "economy.h"
#include "sound.h"

byte _industry_sound_ctr;
TileIndex _industry_sound_tile;

void ShowIndustryViewWindow(int industry);
void BuildOilRig(uint tile);
void DeleteOilRig(uint tile);

typedef struct DrawIndustryTileStruct {
	uint32 sprite_1;
	uint32 sprite_2;

	byte subtile_xy;
	byte width_height;
	byte dz;
	byte proc;
} DrawIndustryTileStruct;


typedef struct DrawIndustrySpec1Struct {
	byte x;
	byte image_1;
	byte image_2;
	byte image_3;
} DrawIndustrySpec1Struct;

typedef struct DrawIndustrySpec4Struct {
	byte image_1;
	byte image_2;
	byte image_3;
} DrawIndustrySpec4Struct;



#include "table/industry_land.h"


typedef struct IndustryTileTable {
	TileIndexDiff ti;
	byte map5;
} IndustryTileTable;

typedef struct IndustrySpec {
	const IndustryTileTable * const * table;
	byte num_table;
	byte a,b,c;
	byte produced_cargo[2];
	byte production_rate[2];
	byte accepts_cargo[3];
	byte check_proc;
} IndustrySpec;

#include "table/build_industry.h"

static const byte _industry_close_mode[37] = {
	1,0,2,1,2,1,2,2,2,1,1,1,0,2,2,1,0,1,1,1,1,1,0,2,1,2,1,2,1,1,0,2,1,2,1,1,1,
};

static const StringID _industry_prod_up_strings[] = {
	STR_4836_NEW_COAL_SEAM_FOUND_AT,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4837_NEW_OIL_RESERVES_FOUND,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4838_IMPROVED_FARMING_METHODS,
	STR_4835_INCREASES_PRODUCTION,
	STR_4837_NEW_OIL_RESERVES_FOUND,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4838_IMPROVED_FARMING_METHODS,
	STR_4838_IMPROVED_FARMING_METHODS,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4838_IMPROVED_FARMING_METHODS,
	STR_4835_INCREASES_PRODUCTION,
	STR_4838_IMPROVED_FARMING_METHODS,
	STR_4835_INCREASES_PRODUCTION,
	STR_4838_IMPROVED_FARMING_METHODS,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
	STR_4835_INCREASES_PRODUCTION,
};

static const StringID _industry_prod_down_strings[] = {
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_483A_INSECT_INFESTATION_CAUSES,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_483A_INSECT_INFESTATION_CAUSES,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_483A_INSECT_INFESTATION_CAUSES,
	STR_483A_INSECT_INFESTATION_CAUSES,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_483A_INSECT_INFESTATION_CAUSES,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_483A_INSECT_INFESTATION_CAUSES,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
	STR_4839_PRODUCTION_DOWN_BY_50,
};

static const StringID _industry_close_strings[] = {
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4834_LACK_OF_NEARBY_TREES_CAUSES,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4833_SUPPLY_PROBLEMS_CAUSE_TO,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE,
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE	,
};


static void IndustryDrawTileProc1(TileInfo *ti)
{
	const DrawIndustrySpec1Struct *d;
	uint32 image;

	if (!(_map_owner[ti->tile] & 0x80))
		return;

	d = &_draw_industry_spec1[_map3_lo[ti->tile]];

	AddChildSpriteScreen(0x12A7 + d->image_1, d->x, 0);

	if ( (image = d->image_2) != 0)
		AddChildSpriteScreen(0x12B0 + image - 1, 8, 41);

	if ( (image = d->image_3) != 0)
		AddChildSpriteScreen(0x12AC + image - 1,
			_drawtile_proc1_x[image-1], _drawtile_proc1_y[image-1]);
}

static void IndustryDrawTileProc2(TileInfo *ti)
{
	int x = 0;

	if (_map_owner[ti->tile] & 0x80) {
		x = _industry_anim_offs[_map3_lo[ti->tile]];
		if ( (byte)x == 0xFF)
			x = 0;
	}

	AddChildSpriteScreen(0x129F, 22-x, x+24);
	AddChildSpriteScreen(0x129E, 6, 0xE);
}

static void IndustryDrawTileProc3(TileInfo *ti)
{
	if (_map_owner[ti->tile] & 0x80) {
		AddChildSpriteScreen(0x128B, 5,
			_industry_anim_offs_2[_map3_lo[ti->tile]]);
	}
	AddChildSpriteScreen(4746, 3, 67);
}

static void IndustryDrawTileProc4(TileInfo *ti)
{
	const DrawIndustrySpec4Struct *d;

	d = &_industry_anim_offs_3[_map3_lo[ti->tile]];

	if (d->image_1 != 0xFF) {
		AddChildSpriteScreen(0x126F, 0x32 - d->image_1 * 2, 0x60 + d->image_1);
	}

	if (d->image_2 != 0xFF) {
		AddChildSpriteScreen(0x1270, 0x10 - d->image_2*2, 100 + d->image_2);
	}

	AddChildSpriteScreen(0x126E, 7, d->image_3);
	AddChildSpriteScreen(0x126D, 0, 42);
}

static void DrawCoalPlantSparkles(TileInfo *ti)
{
	int image = _map_owner[ti->tile];
	if (image & 0x80) {
		image = (image >> 2) & 0x1F;
		if (image != 0 && image < 7) {
			AddChildSpriteScreen(image + 0x806,
				_coal_plant_sparkles_x[image-1],
				_coal_plant_sparkles_y[image-1]
			);
		}
	}
}

typedef void IndustryDrawTileProc(TileInfo *ti);
static IndustryDrawTileProc * const _industry_draw_tile_procs[5] = {
	IndustryDrawTileProc1,
	IndustryDrawTileProc2,
	IndustryDrawTileProc3,
	IndustryDrawTileProc4,
	DrawCoalPlantSparkles,
};

static void DrawTile_Industry(TileInfo *ti)
{
	Industry *ind;
	const DrawIndustryTileStruct *dits;
	byte z;
	uint32 image, ormod;

	/* Pointer to industry */
	ind = DEREF_INDUSTRY(_map2[ti->tile]);
	ormod = (ind->color_map+0x307)<<16;

	/* Retrieve pointer to the draw industry tile struct */
	dits = &_industry_draw_tile_data[(ti->map5<<2) | (_map_owner[ti->tile]&3)];

	image = dits->sprite_1;
	if (image&0x8000 && (image & 0xFFFF0000) == 0)
		image |= ormod;

	z = ti->z;
	/* Add bricks below the industry? */
	if (ti->tileh & 0xF) {
		AddSortableSpriteToDraw((ti->tileh & 0xF) + 0x3DD, ti->x, ti->y, 16, 16, 7, z);
		AddChildSpriteScreen(image, 0x1F, 1);
		z += 8;
	} else {
		/* Else draw regular ground */
		DrawGroundSprite(image);
	}

	/* Add industry on top of the ground? */
	if ((image = dits->sprite_2) != 0) {

		if (image&0x8000 && (image & 0xFFFF0000) == 0)
			image |= ormod;

		if (_display_opt & DO_TRANS_BUILDINGS)
			image = (image & 0x3FFF) | 0x3224000;

		AddSortableSpriteToDraw(image,
			ti->x | (dits->subtile_xy>>4),
			ti->y | (dits->subtile_xy&0xF),
			(dits->width_height>>4)+1,
			(dits->width_height&0xF)+1,
			dits->dz,
			z);

		if (_display_opt & DO_TRANS_BUILDINGS)
			return;
	}

	/* TTDBUG: strange code here, return if AddSortableSpriteToDraw failed?  */
		{
		int proc;
		if ((proc=dits->proc-1) >= 0 )
			_industry_draw_tile_procs[proc](ti);
	}
}


static uint GetSlopeZ_Industry(TileInfo *ti) {
	return GetPartialZ(ti->x&0xF, ti->y&0xF, ti->tileh) + ti->z;
}

static uint GetSlopeTileh_Industry(TileInfo *ti) {
	return 0;
}

static void GetAcceptedCargo_Industry(uint tile, AcceptedCargo ac)
{
	int m5 = _map5[tile];
	int a;

	a = _industry_map5_accepts_1[m5];
	if (a >= 0) ac[a] = (a == 0) ? 1 : 8;

	a = _industry_map5_accepts_2[m5];
	if (a >= 0) ac[a] = 8;

	a = _industry_map5_accepts_3[m5];
	if (a >= 0) ac[a] = 8;
}

static void GetTileDesc_Industry(uint tile, TileDesc *td)
{
	Industry *i = DEREF_INDUSTRY(_map2[tile]);

	td->owner = i->owner;
	td->str = STR_4802_COAL_MINE + i->type;
	if ((_map_owner[tile] & 0x80) == 0) {
		SetDParamX(td->dparam, 0, td->str);
		td->str = STR_2058_UNDER_CONSTRUCTION;
	}
}

static int32 ClearTile_Industry(uint tile, byte flags)
{
	Industry *i = DEREF_INDUSTRY(_map2[tile]);

	/*	* water can destroy industries
			* in editor you can bulldoze industries
			* with magic_bulldozer cheat you can destroy industries
			* (area around OILRIG is water, so water shouldn't flood it
	*/
	if ((_current_player != OWNER_WATER && _game_mode != GM_EDITOR &&
			!_cheats.magic_bulldozer.value) ||
			(_current_player == OWNER_WATER && i->type == IT_OIL_RIG) ) {
 		SetDParam(0, STR_4802_COAL_MINE + i->type);
		return_cmd_error(STR_4800_IN_THE_WAY);
	}

	if (flags & DC_EXEC) {
 		DeleteIndustry(i);
	}
	return 0;
}

/* p1 index of industry to destroy */
/* Destroy Industry button costing money removed per request of dominik
int32 CmdDestroyIndustry(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile = TILE_FROM_XY(x,y);
	Industry *i = DEREF_INDUSTRY((uint16)p1);
  Town *t = ClosestTownFromTile(tile, (uint)-1); // find closest town to penaltize (ALWAYS penaltize)

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	// check if you're allowed to remove the industry. Minimum amount
	// of ratings to remove the industry depends on difficulty setting
	if (!CheckforTownRating(tile, flags, t, INDUSTRY_REMOVE))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		DeleteIndustry(i);
		CreateEffectVehicleAbove(x + 8,y + 8, 2, EV_DEMOLISH);
		ChangeTownRating(t, -1500, -1000);	// penalty is 1500
	}

	return (_price.build_industry >> 5) * _industry_type_costs[i->type]*2;
}
*/

static const byte _industry_min_cargo[] = {
	5, 5, 5, 30, 5, 5, 5, 5,
	5, 5, 5, 5, 2, 5, 5, 5,
	5, 5, 5, 15, 15, 5, 5, 5,
	5, 5, 30, 5, 30, 5, 5, 5,
	5, 5, 5, 5, 5,
};

static void TransportIndustryGoods(uint tile)
{
	Industry *i;
	int type;
	uint cw, am;
	byte m5;

	i = DEREF_INDUSTRY(_map2[tile]);

	type = i->type;
	cw = min(i->cargo_waiting[0], 255);
	if (cw > _industry_min_cargo[type]/* && i->produced_cargo[0] != 0xFF*/) {
		i->cargo_waiting[0] -= cw;

		/* fluctuating economy? */
		if (_economy.fluct <= 0) cw = (cw + 1) >> 1;

		i->last_mo_production[0] += cw;

		am = MoveGoodsToStation(i->xy, i->width, i->height, i->produced_cargo[0], cw);
		i->last_mo_transported[0] += am;
		if (am != 0 && (m5 = _industry_produce_map5[_map5[tile]]) != 0xFF) {
			_map5[tile] = m5;
			_map_owner[tile] = 0x80;
			MarkTileDirtyByTile(tile);
		}
	}

	type = i->type;
	cw = min(i->cargo_waiting[1], 255);
	if (cw > _industry_min_cargo[type]) {
		i->cargo_waiting[1] -= cw;

		if (_economy.fluct <= 0) cw = (cw + 1) >> 1;

		i->last_mo_production[1] += cw;

		am = MoveGoodsToStation(i->xy, i->width, i->height, i->produced_cargo[1], cw);
		i->last_mo_transported[1] += am;
	}
}


static void AnimateTile_Industry(uint tile)
{
	byte m,n;

	switch(_map5[tile]) {
	case 174:
		if ((_tick_counter & 1) == 0) {
			m = _map3_lo[tile] + 1;

			switch(m & 7) {
			case 2:	SndPlayTileFx(SND_2D_RIP_2, tile); break;
			case 6: SndPlayTileFx(SND_29_RIP, tile); break;
			}

			if (m >= 96) {
				m = 0;
				DeleteAnimatedTile(tile);
			}
			_map3_lo[tile] = m;

			MarkTileDirtyByTile(tile);
		}
		break;

	case 165:
		if ((_tick_counter & 3) == 0) {
			m = _map3_lo[tile];

			if (_industry_anim_offs[m] == 0xFF) {
				SndPlayTileFx(SND_30_CARTOON_SOUND, tile);
			}

			if (++m >= 70) {
				m = 0;
				DeleteAnimatedTile(tile);
			}
			_map3_lo[tile] = m;

			MarkTileDirtyByTile(tile);
		}
		break;

	case 162:
		if ((_tick_counter&1) == 0) {
			m = _map3_lo[tile];

			if (++m >= 40) {
				m = 0;
				DeleteAnimatedTile(tile);
			}
			_map3_lo[tile] = m;

			MarkTileDirtyByTile(tile);
		}
		break;

	// Sparks on a coal plant
	case 10:
		if ((_tick_counter & 3) == 0) {
			m = _map_owner[tile];
			if ((m & (31<<2)) == (6 << 2)) {
				_map_owner[tile] = m&~(31<<2);
				DeleteAnimatedTile(tile);
			} else {
				_map_owner[tile] = m + (1<<2);
				MarkTileDirtyByTile(tile);
			}
		}
		break;

	case 143:
		if ((_tick_counter & 1) == 0) {
			m = _map3_lo[tile] + 1;

			if (m == 1) {
				SndPlayTileFx(SND_2C_MACHINERY, tile);
			} else if (m == 23) {
				SndPlayTileFx(SND_2B_COMEDY_HIT, tile);
			} else if (m == 28) {
				SndPlayTileFx(SND_2A_EXTRACT_AND_POP, tile);
			}

			if (m >= 50 && (m=0,++_map3_hi[tile] >= 8)) {
				_map3_hi[tile] = 0;
				DeleteAnimatedTile(tile);
			}
			_map3_lo[tile] = m;
			MarkTileDirtyByTile(tile);
		}
		break;

	case 148: case 149: case 150: case 151:
	case 152: case 153: case 154: case 155:
		if ((_tick_counter & 3) == 0) {
			m = _map5[tile]	+ 1;
			if (m == 155+1) m = 148;
			_map5[tile] = m;

			MarkTileDirtyByTile(tile);
		}
		break;

	case 30: case 31: case 32:
		if ((_tick_counter & 7) == 0) {
			bool b = CHANCE16(1,7);
			m = _map_owner[tile];
			m = (m & 3) + 1;
			n = _map5[tile];
			if (m == 4 && (m=0,++n) == 32+1 && (n=30,b)) {
				_map_owner[tile] = 0x83;
				_map5[tile] = 29;
				DeleteAnimatedTile(tile);
			} else {
				_map5[tile] = n;
				_map_owner[tile] = (_map_owner[tile] & ~3) | m;
				MarkTileDirtyByTile(tile);
			}
		}
		break;

	case 88:
	case 48:
	case 1: {
			int state = _tick_counter & 0x7FF;

			if ((state -= 0x400) < 0)
				return;

			if (state < 0x1A0) {
				if (state < 0x20 || state >= 0x180) {
					if (!	(_map_owner[tile] & 0x40)) {
						_map_owner[tile] |= 0x40;
						SndPlayTileFx(SND_0B_MINING_MACHINERY, tile);
					}
					if (state & 7)
						return;
				} else {
					if (state & 3)
						return;
				}
				m = (_map_owner[tile] + 1) | 0x40;
				if (m > 0xC2) m = 0xC0;
				_map_owner[tile] = m;
				MarkTileDirtyByTile(tile);
			} else if (state >= 0x200 && state < 0x3A0) {
				int i;
				i = (state < 0x220 || state >= 0x380) ? 7 : 3;
				if (state & i)
					return;

				m = (_map_owner[tile] & 0xBF) - 1;
				if (m < 0x80) m = 0x82;
				_map_owner[tile] = m;
				MarkTileDirtyByTile(tile);
			}
		} break;
	}
}

static void MakeIndustryTileBiggerCase8(uint tile)
{
	TileInfo ti;
	FindLandscapeHeight(&ti, GET_TILE_X(tile)*16, GET_TILE_Y(tile)*16);
	CreateEffectVehicle(ti.x + 15, ti.y + 14, ti.z + 59 + (ti.tileh != 0 ? 8 : 0), EV_INDUSTRYSMOKE);
}

static void MakeIndustryTileBigger(uint tile, byte size)
{
	byte b = (byte)((size + (1<<2)) & (3<<2));

	if (b != 0) {
		_map_owner[tile] = b | (size & 3);
		return;
	}

	size = (size + 1) & 3;
	if (size == 3) size |= 0x80;
	_map_owner[tile] = size | b;

	MarkTileDirtyByTile(tile);

	if (!(_map_owner[tile] & 0x80))
		return;

	switch(_map5[tile]) {
	case 8:
		MakeIndustryTileBiggerCase8(tile);
		break;

	case 24:
		if (_map5[tile + TILE_XY(0,1)] == 24) {
			BuildOilRig(tile);
		}
		break;

	case 143:
	case 162:
	case 165:
		_map3_lo[tile] = 0;
		_map3_hi[tile] = 0;
		break;

	case 148: case 149: case 150: case 151:
	case 152: case 153: case 154: case 155:
		AddAnimatedTile(tile);
		break;
	}
}



static void TileLoopIndustryCase161(uint tile)
{
	int dir;
	Vehicle *v;
	static const int8 _tileloop_ind_case_161[12] = {
		11, 0, -4, -14,
		-4, -10, -4, 1,
		49, 59, 60, 65,
	};

	SndPlayTileFx(SND_2E_EXTRACT_AND_POP, tile);

	dir = Random() & 3;

	v = CreateEffectVehicleAbove(
		GET_TILE_X(tile)*16 + _tileloop_ind_case_161[dir + 0],
		GET_TILE_Y(tile)*16 + _tileloop_ind_case_161[dir + 4],
		_tileloop_ind_case_161[dir + 8],
		EV_INDUSTRY_SMOKE
	);

	if (v != NULL)
		v->u.special.unk2 = dir;
}

static void TileLoop_Industry(uint tile)
{
	byte n;

	if (!(_map_owner[tile] & 0x80)) {
		MakeIndustryTileBigger(tile, _map_owner[tile]);
		return;
	}

	if (_game_mode == GM_EDITOR)
		return;

	TransportIndustryGoods(tile);

	n = _industry_map5_animation_next[_map5[tile]];
	if (n != 255) {
		_map5[tile] = n;
		_map_owner[tile] = 0;
		MarkTileDirtyByTile(tile);
		return;
	}

#define SET_AND_ANIMATE(tile,a,b) { _map5[tile]=a; _map_owner[tile]=b; AddAnimatedTile(tile); }
#define SET_AND_UNANIMATE(tile,a,b) { _map5[tile]=a; _map_owner[tile]=b; DeleteAnimatedTile(tile); }

	switch(_map5[tile]) {
	case 0x18: // coast line at oilrigs
	case 0x19:
	case 0x1A:
	case 0x1B:
	case 0x1C:
		TileLoop_Water(tile);
		break;

	case 0:
		if (!(_tick_counter & 0x400) && CHANCE16(1,2))
			SET_AND_ANIMATE(tile,1,0x80);
		break;

	case 47:
		if (!(_tick_counter & 0x400) && CHANCE16(1,2))
			SET_AND_ANIMATE(tile,0x30,0x80);
		break;

	case 79:
		if (!(_tick_counter & 0x400) && CHANCE16(1,2))
			SET_AND_ANIMATE(tile,0x58,0x80);
		break;

	case 29:
		if (CHANCE16(1,6))
			SET_AND_ANIMATE(tile,0x1E,0x80);
		break;

	case 1:
		if (!(_tick_counter & 0x400))
			SET_AND_UNANIMATE(tile, 0, 0x83);
		break;

	case 48:
		if (!(_tick_counter & 0x400))
			SET_AND_UNANIMATE(tile, 0x2F, 0x83);
		break;

	case 88:
		if (!(_tick_counter & 0x400))
			SET_AND_UNANIMATE(tile, 0x4F, 0x83);
		break;

	case 10:
		if (CHANCE16(1,3)) {
			SndPlayTileFx(SND_0C_ELECTRIC_SPARK, tile);
			AddAnimatedTile(tile);
		}
		break;

	case 49: {
		CreateEffectVehicleAbove(GET_TILE_X(tile)*16 + 6, GET_TILE_Y(tile)*16 + 6, 43, EV_SMOKE_3);
	} break;


	case 143: {
			Industry *i = DEREF_INDUSTRY(_map2[tile]);
			if (i->was_cargo_delivered) {
				i->was_cargo_delivered = false;
				if ((_map3_hi[tile]|_map3_lo[tile]) != 0)
					_map3_hi[tile] = 0;
				AddAnimatedTile(tile);
			}
		}
		break;

	case 161:
		TileLoopIndustryCase161(tile);
		break;

	case 165:
		AddAnimatedTile(tile);
		break;

	case 174:
		if (CHANCE16(1,3)) {
			AddAnimatedTile(tile);
		}
		break;
	}
}


static void ClickTile_Industry(uint tile)
{
	ShowIndustryViewWindow(_map2[tile]);
}

static uint32 GetTileTrackStatus_Industry(uint tile, TransportType mode)
{
	return 0;
}

static void GetProducedCargo_Industry(uint tile, byte *b)
{
	Industry *i = DEREF_INDUSTRY(_map2[tile]);
	b[0] = i->produced_cargo[0];
	b[1] = i->produced_cargo[1];
}

static void ChangeTileOwner_Industry(uint tile, byte old_player, byte new_player)
{
	/* not used */
}

void DeleteIndustry(Industry *i)
{
	int index = i - _industries;

	BEGIN_TILE_LOOP(tile_cur,	i->width, i->height, i->xy);
		if (IS_TILETYPE(tile_cur, MP_INDUSTRY)) {
			if (_map2[tile_cur] == (uint16)index) {
				DoClearSquare(tile_cur);
			}
		} else if (IS_TILETYPE(tile_cur, MP_STATION) && _map5[tile_cur] == 0x4B) {
			DeleteOilRig(tile_cur);
		}
	END_TILE_LOOP(tile_cur,	i->width, i->height, i->xy);

	i->xy = 0;
	_industry_sort_dirty = true;
	DeleteSubsidyWithIndustry(index);
	DeleteWindowById(WC_INDUSTRY_VIEW, index);
	InvalidateWindow(WC_INDUSTRY_DIRECTORY, 0);
}

static const byte _plantfarmfield_type[] = {1, 1, 1, 1, 1, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6};

static bool IsBadFarmFieldTile(uint tile)
{
	if (IS_TILETYPE(tile,MP_CLEAR)) {
		byte m5 = _map5[tile] & 0x1C;
		if (m5 == 0xC || m5 == 0x10)
			return true;
		return false;
	} else if (IS_TILETYPE(tile,MP_TREES)) {
		return false;
	} else {
		return true;
	}
}

static bool IsBadFarmFieldTile2(uint tile)
{
	if (IS_TILETYPE(tile,MP_CLEAR)) {
		byte m5 = _map5[tile] & 0x1C;
		if (m5 == 0x10)
			return true;
		return false;
	} else if (IS_TILETYPE(tile,MP_TREES)) {
		return false;
	} else {
		return true;
	}
}

static void SetupFarmFieldFence(uint tile, int size, byte type, int direction)
{
	byte or, and;

	do {
		tile = TILE_MASK(tile);

		if (IS_TILETYPE(tile, MP_CLEAR) || IS_TILETYPE(tile, MP_TREES)) {

			or = type;
			if (or == 1 && (uint16)Random() <= 9362) or = 2;

			or <<= 2;
			and = (byte)~0x1C;
			if (direction) {
				or <<= 3;
				and = (byte)~0xE0;
			}
			_map3_hi[tile] = (_map3_hi[tile] & and) | or;
		}

		tile += direction ? TILE_XY(0,1) : TILE_XY(1,0);
	} while (--size);
}

static void PlantFarmField(uint tile)
{
	uint size_x, size_y;
	uint32 r;
	int count;
	int type, type2;

	if (_opt.landscape == LT_HILLY) {
		if (GetTileZ(tile) >= (_opt.snow_line - 16))
			return;
	}

	/* determine field size */
	r = (Random() & 0x303) + 0x404;
	if (_opt.landscape == LT_HILLY) r += 0x404;
	size_x = (byte)r;
	size_y = r >> 8;

	/* offset tile to match size */
	tile = tile - TILE_XY(size_x>>1, size_y>>1);

	/* check the amount of bad tiles */
	count = 0;
	BEGIN_TILE_LOOP(cur_tile, size_x, size_y, tile)
		cur_tile = TILE_MASK(cur_tile);
		count += IsBadFarmFieldTile(cur_tile);
	END_TILE_LOOP(cur_tile, size_x, size_y, tile)
	if ((uint)(count * 2) >= size_x * size_y)
		return;

	/* determine type of field */
	r = Random();
	type = ((r & 0xE0) | 0xF);
	type2 = ((byte)(r >> 8) * 9) >> 8;

	/* make field */
	BEGIN_TILE_LOOP(cur_tile, size_x, size_y, tile)
		cur_tile = TILE_MASK(cur_tile);
		if (!IsBadFarmFieldTile2(cur_tile)) {
			ModifyTile(cur_tile,
				MP_SETTYPE(MP_CLEAR) |
				MP_MAP2_CLEAR | MP_MAP3LO | MP_MAP3HI_CLEAR | MP_MAPOWNER | MP_MAP5,
				type2,			/* map3_lo */
				OWNER_NONE,	/* map_owner */
				type);			/* map5 */
		}
	END_TILE_LOOP(cur_tile, size_x, size_y, tile)

	type = 3;
	if (_opt.landscape != LT_HILLY && _opt.landscape != LT_DESERT) {
		type = _plantfarmfield_type[Random() & 0xF];
	}

	SetupFarmFieldFence(tile-TILE_XY(1,0), size_y, type, 1);
	SetupFarmFieldFence(tile-TILE_XY(0,1), size_x, type, 0);
	SetupFarmFieldFence(tile+TILE_XY(1,0) * (size_x-1), size_y, type, 1);
	SetupFarmFieldFence(tile+TILE_XY(0,1) * (size_y-1), size_x, type, 0);
}

static void MaybePlantFarmField(Industry *i)
{
	uint tile;
	if (CHANCE16(1,8)) {
		int x = (i->width>>1) + Random() % 31 - 16;
		int y = (i->height>>1) + Random() % 31 - 16;
		tile = TileAddWrap(i->xy, x, y);
		if (tile != TILE_WRAPPED)
			PlantFarmField(tile);
	}
}

static void ChopLumberMillTrees(Industry *i)
{
	static const TileIndexDiff _chop_dir[4] = { TILE_XY(0,1), TILE_XY(1,0), TILE_XY(0,-1), TILE_XY(-1,0) };

	uint tile = i->xy;
	int dir, a, j;

	if ((_map_owner[tile] & 0x80) == 0)
		return;

	/* search outwards as a rectangular spiral */
	for(a=1; a!=41; a+=2) {
		for(dir=0; dir!=4; dir++) {
			j = a;
			do {
				tile = TILE_MASK(tile);
				if (IS_TILETYPE(tile, MP_TREES)) {
					uint old_player = _current_player;
					/* found a tree */

					_current_player = OWNER_NONE;
					_industry_sound_ctr = 1;
					_industry_sound_tile = tile;
					SndPlayTileFx(SND_38_CHAINSAW, tile);

					DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
					SetMapExtraBits(tile, 0);

					i->cargo_waiting[0] = min(0xffff, i->cargo_waiting[0] + 45);

					_current_player = old_player;
					return;
				}
				tile += _chop_dir[dir];
			} while (--j);
		}
		tile -= TILE_XY(1,1);
	}
}

static const byte _industry_sounds[37][2] = {
	{0},
	{0},
	{1, SND_28_SAWMILL},
	{0},
	{0},
	{0},
	{1, SND_03_FACTORY_WHISTLE},
	{1, SND_03_FACTORY_WHISTLE},
	{0},
	{3, SND_24_SHEEP},
	{0},
	{0},
	{0},
	{0},
	{1, SND_28_SAWMILL},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{1, SND_03_FACTORY_WHISTLE},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{0},
	{1, SND_33_PLASTIC_MINE},
	{0},
	{0},
	{0},
	{0},
};


static void ProduceIndustryGoods(Industry *i)
{
	uint32 r;
	uint num;

	/* play a sound? */
	if ((i->counter & 0x3F) == 0) {
		if (CHANCE16R(1,14,r) && (num=_industry_sounds[i->type][0]) != 0) {
			SndPlayTileFx(
				_industry_sounds[i->type][1] + (((r >> 16) * num) >> 16),
				i->xy);
		}
	}

	i->counter--;

	/* produce some cargo */
	if ((i->counter & 0xFF) == 0) {
		i->cargo_waiting[0] = min(0xffff, i->cargo_waiting[0] + i->production_rate[0]);
		i->cargo_waiting[1] = min(0xffff, i->cargo_waiting[1] + i->production_rate[1]);

		if (i->type == IT_FARM)
			MaybePlantFarmField(i);
		else if (i->type == IT_LUMBER_MILL && (i->counter & 0x1FF) == 0)
			ChopLumberMillTrees(i);
	}
}

void OnTick_Industry()
{
	Industry *i;

	if (_industry_sound_ctr != 0) {
		_industry_sound_ctr++;

		if (_industry_sound_ctr == 75) {
			SndPlayTileFx(SND_37_BALLOON_SQUEAK, _industry_sound_tile);
		} else if (_industry_sound_ctr == 160) {
			_industry_sound_ctr = 0;
			SndPlayTileFx(SND_36_CARTOON_CRASH, _industry_sound_tile);
		}
	}

	if (_game_mode == GM_EDITOR)
		return;

	FOR_ALL_INDUSTRIES(i) {
		if (i->xy != 0)
			ProduceIndustryGoods(i);
	}
}


static bool CheckNewIndustry_NULL(uint tile, int type)
{
	return true;
}

static bool CheckNewIndustry_Forest(uint tile, int type)
{
	if (_opt.landscape == LT_HILLY) {
		if (GetTileZ(tile) < (_opt.snow_line + 16) ) {
			_error_message = STR_4831_FOREST_CAN_ONLY_BE_PLANTED;
			return false;
		}
	}
	return true;
}

extern bool _ignore_restrictions;

static bool CheckNewIndustry_Oilwell(uint tile, int type)
{
	int x,y;

	if(_ignore_restrictions && _game_mode == GM_EDITOR)
		return true;

	if (type != IT_OIL_RIG && _game_mode == GM_EDITOR)
		return true;

	x = GET_TILE_X(tile);
	y = GET_TILE_Y(tile);

	if ( x < 15 || y < 15 || x > 238 || y > 238)
		return true;

	_error_message = STR_483B_CAN_ONLY_BE_POSITIONED;
	return false;
}

static bool CheckNewIndustry_Farm(uint tile, int type)
{
	if (_opt.landscape == LT_HILLY) {
		if (GetTileZ(tile) >= (_opt.snow_line - 16)) {
			_error_message = STR_0239_SITE_UNSUITABLE;
			return false;
		}
	}
	return true;
}

static bool CheckNewIndustry_Plantation(uint tile, int type)
{
	if (GetMapExtraBits(tile) == 1) {
		_error_message = STR_0239_SITE_UNSUITABLE;
		return false;
	}

	return true;
}

static bool CheckNewIndustry_Water(uint tile, int type)
{
	if (GetMapExtraBits(tile) != 1) {
		_error_message = STR_0318_CAN_ONLY_BE_BUILT_IN_DESERT;
		return false;
	}

	return true;
}

static bool CheckNewIndustry_Lumbermill(uint tile, int type)
{
	if (GetMapExtraBits(tile) != 2) {
		_error_message = STR_0317_CAN_ONLY_BE_BUILT_IN_RAINFOREST;
		return false;
	}
	return true;
}

static bool CheckNewIndustry_BubbleGen(uint tile, int type)
{
	if (GetTileZ(tile) > 32) {
		return false;
	}
	return true;
}

typedef bool CheckNewIndustryProc(uint tile, int type);
static CheckNewIndustryProc * const _check_new_industry_procs[] = {
	CheckNewIndustry_NULL,
	CheckNewIndustry_Forest,
	CheckNewIndustry_Oilwell,
	CheckNewIndustry_Farm,
	CheckNewIndustry_Plantation,
	CheckNewIndustry_Water,
	CheckNewIndustry_Lumbermill,
	CheckNewIndustry_BubbleGen,
};

static bool CheckSuitableIndustryPos(uint tile)
{
	uint x = GET_TILE_X(tile);
	uint y = GET_TILE_Y(tile);

	if ( x < 2 || y < 2 || x > MapMaxX() - 3 || y > MapMaxY() - 3) {
		_error_message = STR_0239_SITE_UNSUITABLE;
		return false;
	}

	return true;
}

static Town *CheckMultipleIndustryInTown(uint tile, int type)
{
	Town *t;
	Industry *i;

	t = ClosestTownFromTile(tile, (uint)-1);

	if (_patches.multiple_industry_per_town)
		return t;

	FOR_ALL_INDUSTRIES(i) {
		if (i->xy != 0 &&
				i->type == (byte)type &&
				i->town == t) {
			_error_message = STR_0287_ONLY_ONE_ALLOWED_PER_TOWN;
			return NULL;
		}
	}

	return t;
}

static const byte _industry_map5_bits[] = {
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 4, 2, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 4, 2, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16,
};

static bool CheckIfIndustryTilesAreFree(uint tile, const IndustryTileTable *it, int type, Town *t)
{
	TileInfo ti;
	uint cur_tile;

	_error_message = STR_0239_SITE_UNSUITABLE;

	do {
		cur_tile = tile + it->ti;
		if (!IsValidTile(cur_tile)) {
			if (it->map5 == 0xff)
				continue;
			return false;
		}

		FindLandscapeHeightByTile(&ti, cur_tile);

		if (it->map5 == 0xFF) {
			if (ti.type != MP_WATER || ti.tileh != 0)
				return false;
		} else {
			if (!EnsureNoVehicle(cur_tile))
				return false;

			if (type == IT_OIL_RIG)  {
				if (ti.type != MP_WATER || ti.map5 != 0)
					return false;
			} else {
				if (ti.type == MP_WATER && ti.map5 == 0)
					return false;
				if (ti.tileh & 0x10)
					return false;

				if (ti.tileh != 0) {
					int t;
					byte bits = _industry_map5_bits[it->map5];

					if (bits & 0x10)
						return false;

					t = ~ti.tileh;

					if (bits & 1 && (t & (1+8)))
						return false;

					if (bits & 2 && (t & (4+8)))
						return false;

					if (bits & 4 && (t & (1+2)))
						return false;

					if (bits & 8 && (t & (2+4)))
						return false;
				}

				if (type == IT_BANK) {
					if (ti.type != MP_HOUSE || t->population < 1200) {
						_error_message = STR_029D_CAN_ONLY_BE_BUILT_IN_TOWNS;
						return false;
					}
				} else if (type == IT_BANK_2) {
					if (ti.type != MP_HOUSE) {
						_error_message = STR_030D_CAN_ONLY_BE_BUILT_IN_TOWNS;
						return false;
					}
				} else if (type == IT_TOY_SHOP) {
					if (GetTileDist1D(t->xy, cur_tile) > 9)
						return false;
					if (ti.type != MP_HOUSE) goto do_clear;
				} else if (type == IT_WATER_TOWER) {
					if (ti.type != MP_HOUSE) {
						_error_message = STR_0316_CAN_ONLY_BE_BUILT_IN_TOWNS;
						return false;
					}
				} else {
do_clear:
					if (DoCommandByTile(cur_tile, 0, 0, DC_AUTO, CMD_LANDSCAPE_CLEAR) == CMD_ERROR)
						return false;
				}
			}
		}
	} while ( (++it)->ti != -0x8000);

	return true;
}

static bool CheckIfTooCloseToIndustry(uint tile, int type)
{
	Industry *i;
	const IndustrySpec *spec;
	spec = &_industry_spec[type];

	// accepting industries won't be close, not even with patch
	if (_patches.same_industry_close && (spec->accepts_cargo[0] == 0xFF) )
		return true;

	FOR_ALL_INDUSTRIES(i) {
		// check if an industry that accepts the same goods is nearby
		if (i->xy != 0 &&
				(GetTileDist1D(tile, i->xy) <= 14) &&
				spec->accepts_cargo[0] != 0xFF &&
				spec->accepts_cargo[0] == i->accepts_cargo[0]) {
				_error_message = STR_INDUSTRY_TOO_CLOSE;
				return false;
				}

		// check "not close to" field.
		if (i->xy != 0 &&
				(i->type == spec->a || i->type == spec->b || i->type == spec->c) &&
				GetTileDist1D(tile, i->xy) <= 14) {
			_error_message = STR_INDUSTRY_TOO_CLOSE;
			return false;
		}
	}
	return true;
}

static Industry *AllocateIndustry()
{
	Industry *i;

	FOR_ALL_INDUSTRIES(i) {
		if (i->xy == 0) {
			int index = i - _industries;
		    if (index > _total_industries) _total_industries = index;
			return i;
		}
	}
	return NULL;
}

static void DoCreateNewIndustry(Industry *i, uint tile, int type, const IndustryTileTable *it, Town *t, byte owner)
{
	const IndustrySpec *spec;
	uint32 r;
	uint cur_tile;
	int j;

	i->xy = tile;
	i->width = i->height = 0;
	i->type = type;

	spec = &_industry_spec[type];

	i->produced_cargo[0] = spec->produced_cargo[0];
	i->produced_cargo[1] = spec->produced_cargo[1];
	i->accepts_cargo[0] = spec->accepts_cargo[0];
	i->accepts_cargo[1] = spec->accepts_cargo[1];
	i->accepts_cargo[2] = spec->accepts_cargo[2];
	i->production_rate[0] = spec->production_rate[0];
	i->production_rate[1] = spec->production_rate[1];

	if (_patches.smooth_economy) {
		i->production_rate[0] = min((RandomRange(256) + 128) * i->production_rate[0] >> 8 , 255);
		i->production_rate[1] = min((RandomRange(256) + 128) * i->production_rate[1] >> 8 , 255);
	}

	i->town = t;
	i->owner = owner;

	r = Random() & 0xFFF;
	i->color_map = (byte)(r >> 8);
	i->counter = (uint16)r;
	i->cargo_waiting[0] = 0;
	i->cargo_waiting[1] = 0;
	i->last_mo_production[0] = 0;
	i->last_mo_production[1] = 0;
	i->last_mo_transported[0] = 0;
	i->last_mo_transported[1] = 0;
	i->pct_transported[0] = 0;
	i->pct_transported[1] = 0;
	i->total_transported[0] = 0;
	i->total_transported[1] = 0;
	i->was_cargo_delivered = false;
	i->last_prod_year = _cur_year;
	i->total_production[0] = i->production_rate[0] * 8;
	i->total_production[1] = i->production_rate[1] * 8;

	if (_generating_world == 0)
		i->total_production[0] = i->total_production[1] = 0;

	i->prod_level = 0x10;

	do {
		cur_tile = tile + it->ti;

		if (it->map5 != 0xFF) {
			byte size;

			size = GET_TILE_X((TileIndex)it->ti);
			if (size > i->width) i->width = size;
			size = GET_TILE_Y((TileIndex)it->ti);
			if (size > i->height)i->height = size;

			DoCommandByTile(cur_tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);

			_map_type_and_height[cur_tile] = (_map_type_and_height[cur_tile]&~0xF0) | (MP_INDUSTRY<<4);
			_map5[cur_tile] = it->map5;
			_map2[cur_tile] = i - _industries;
			_map_owner[cur_tile] = _generating_world ? 0x1E : 0; /* maturity */
		}
	} while ( (++it)->ti != -0x8000);

	i->width++;
	i->height++;

	if (i->type == IT_FARM || i->type == IT_FARM_2) {
		tile = i->xy + TILE_XY((i->width >> 1), (i->height >> 1));
		for(j=0; j!=50; j++) {
			int x = Random() % 31 - 16;
			int y = Random() % 31 - 16;
			uint new_tile = TileAddWrap(tile, x, y);
			if (new_tile != TILE_WRAPPED)
				PlantFarmField(new_tile);
		}
	}
	_industry_sort_dirty = true;
	InvalidateWindow(WC_INDUSTRY_DIRECTORY, 0);
}

/* p1 = industry type 0-36 */
int32 CmdBuildIndustry(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	uint tile = TILE_FROM_XY(x,y);
	Town *t;
	int num;
	const IndustryTileTable * const *itt;
	const IndustryTileTable *it;
	Industry *i;
	const IndustrySpec *spec;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	if (!CheckSuitableIndustryPos(tile))
		return CMD_ERROR;

	spec = &_industry_spec[p1];

	if (!_check_new_industry_procs[spec->check_proc](tile, p1))
		return CMD_ERROR;

	if ((t=CheckMultipleIndustryInTown(tile, p1)) == NULL)
		return CMD_ERROR;

	num = spec->num_table;
	itt = spec->table;

	do {
		if (--num < 0)
			return_cmd_error(STR_0239_SITE_UNSUITABLE);
	} while (!CheckIfIndustryTilesAreFree(tile, it=itt[num], p1, t));


	if (!CheckIfTooCloseToIndustry(tile, p1))
		return CMD_ERROR;

	if ( (i = AllocateIndustry()) == NULL)
		return CMD_ERROR;

	if (flags & DC_EXEC)
		DoCreateNewIndustry(i, tile, p1, it, t, 0x10);

	return (_price.build_industry >> 5) * _industry_type_costs[p1];
}


Industry *CreateNewIndustry(uint tile, int type)
{
	Town *t;
	const IndustryTileTable *it;
	Industry *i;

	const IndustrySpec *spec;

	if (!CheckSuitableIndustryPos(tile))
		return NULL;

	spec = &_industry_spec[type];

	if (!_check_new_industry_procs[spec->check_proc](tile, type))
		return NULL;

	if (!(t=CheckMultipleIndustryInTown(tile, type)))
		return NULL;

	/* pick a random layout */
	it = spec->table[(spec->num_table * (uint16)Random()) >> 16];

	if (!CheckIfIndustryTilesAreFree(tile, it, type, t))
		return NULL;

	if (!CheckIfTooCloseToIndustry(tile, type))
		return NULL;

	if ( (i = AllocateIndustry()) == NULL)
		return NULL;

	DoCreateNewIndustry(i, tile, type, it, t, 0x10);

	return i;
}

static const byte _numof_industry_table[4][12] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
	{0, 2, 3, 4, 6, 7, 8, 9, 10, 10, 10},
};

static void PlaceInitialIndustry(byte type, int amount)
{
	int num = _numof_industry_table[_opt.diff.number_industries][amount];

	if (_opt.diff.number_industries != 0)
	{
		byte old_player = _current_player;
		_current_player = OWNER_NONE;
		assert(num > 0);

		do {
			int i = 2000;
			do {
				if (CreateNewIndustry(TILE_MASK(Random()), type) != NULL)
					break;
			} while (--i != 0);
		} while (--num);

		_current_player = old_player;
	}
}

void GenerateIndustries()
{
	const byte *b;

	b = _industry_create_table[_opt.landscape];
	do {
		PlaceInitialIndustry(b[1], b[0]);
	} while ( (b+=2)[0] != 0);
}

static void ExtChangeIndustryProduction(Industry *i)
{
	bool closeit;
	int j;

	if (_industry_close_mode[i->type] == 0) return;

	closeit = true;

	if (_industry_close_mode[i->type] == 2) {
		if ( (byte)(_cur_year - i->last_prod_year) < 5 || !CHANCE16(1,180))
			closeit = false;
	} else {
		for (j=0; j != 2 && i->produced_cargo[j]!=255; j++){
			uint32 r;
			int change,percent,old;
			int mag;

			change = old = i->production_rate[j];
			if (CHANCE16R(20,1024,r))change -= ((RandomRange(50) + 10)*old) >> 8;
			if (CHANCE16I(20+(i->pct_transported[j]*20>>8),1024,r>>16)) change += ((RandomRange(50) + 10)*old) >> 8;

			// make sure it doesn't exceed 255 or goes below 0
			change = clamp(change, 0, 255);
			if (change == old) {
				closeit = false;
				continue;
			}

			percent = change*100/old - 100;
			i->production_rate[j] = change;

			if (change >= _industry_spec[i->type].production_rate[j]/4)
				closeit = false;

			mag = abs(percent);
			if (mag >= 10) {
				SetDParam(3, mag);
				SetDParam(0,_cargoc.names_s[i->produced_cargo[j]]);
				SetDParam(1, i->town->index);
				SetDParam(2, i->type + STR_4802_COAL_MINE);
				AddNewsItem(percent>=0 ? STR_INDUSTRY_PROD_GOUP : STR_INDUSTRY_PROD_GODOWN,
						NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_TILE, NT_ECONOMY, 0),
						i->xy + TILE_XY(1,1), 0);
			}
		}
	}

	if (closeit) {
		i->prod_level = 0;
		SetDParam(1, i->type + STR_4802_COAL_MINE);
		SetDParam(0, i->town->index);
		AddNewsItem(_industry_close_strings[i->type], NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_TILE, NT_ECONOMY, 0), i->xy + TILE_XY(1,1), 0);
	}
}


static void UpdateIndustryStatistics(Industry *i)
{
	byte pct;

	if (i->produced_cargo[0] != 0xFF) {
		pct = 0;
		if (i->last_mo_production[0] != 0) {
			i->last_prod_year = _cur_year;
			pct = min(i->last_mo_transported[0] * 256 / i->last_mo_production[0],255);
		}
		i->pct_transported[0] = pct;

		i->total_production[0] = i->last_mo_production[0];
		i->last_mo_production[0] = 0;

		i->total_transported[0] = i->last_mo_transported[0];
		i->last_mo_transported[0] = 0;
	}

	if (i->produced_cargo[1] != 0xFF) {
		pct = 0;
		if (i->last_mo_production[1] != 0) {
			i->last_prod_year = _cur_year;
			pct = min(i->last_mo_transported[1] * 256 / i->last_mo_production[1],255);
		}
		i->pct_transported[1] = pct;

		i->total_production[1] = i->last_mo_production[1];
		i->last_mo_production[1] = 0;

		i->total_transported[1] = i->last_mo_transported[1];
		i->last_mo_transported[1] = 0;
	}


	if ( i->produced_cargo[0] != 0xFF || i->produced_cargo[1] != 0xFF )
		InvalidateWindow(WC_INDUSTRY_VIEW, i - _industries);

	if (i->prod_level == 0)
		DeleteIndustry(i);
	else if (_patches.smooth_economy)
		ExtChangeIndustryProduction(i);
}

static const byte _new_industry_rand[4][32] = {
	{12,12,12,12,12,12,12, 0, 0, 6, 6, 9, 9, 3, 3, 3,18,18, 4, 4, 2, 2, 5, 5, 5, 5, 5, 5, 1, 1, 8, 8},
	{16,16,16, 0, 0, 0, 9, 9, 9, 9,13,13, 3, 3, 3, 3,15,15,15, 4, 4,11,11,11,11,11,14,14, 1, 1, 7, 7},
	{21,21,21,24,22,22,22,22,23,23,12,12,12, 4, 4,19,19,19,13,13,20,20,20,11,11,11,17,17,17,10,10,10},
	{30,30,30,36,36,31,31,31,27,27,27,28,28,28,26,26,26,34,34,34,35,35,35,29,29,29,32,32,32,33,33,33},
};

static void MaybeNewIndustry(uint32 r)
{
	int type;
	int j;
	Industry *i;

	type = _new_industry_rand[_opt.landscape][(r >> 16) & 0x1F];
	if (type == IT_OIL_WELL && _date > 10958)
		return;

	if (type == IT_OIL_RIG && _date < 14610)
		return;

	j = 2000;
	for(;;) {
		i = CreateNewIndustry(TILE_MASK(Random()), type);
		if (i != NULL)
			break;
		if (--j == 0)
			return;
	}

	SetDParam(0, type + STR_4802_COAL_MINE);
	SetDParam(1, i->town->index);
	AddNewsItem(	STR_482D_NEW_UNDER_CONSTRUCTION + (type == IT_FOREST), NEWS_FLAGS(NM_THIN,NF_VIEWPORT|NF_TILE,NT_ECONOMY,0), i->xy, 0);
}

static void MaybeCloseIndustry(Industry *i)
{
	uint32 r;
	StringID str;
	int type = i->type;

	if (_industry_close_mode[type] == 1) {
		/* decrease or increase */
		if (type == IT_OIL_WELL && _opt.landscape == LT_NORMAL) goto decrease_production;
		if (CHANCE16I(1,3,r=Random())) {
			if ((i->pct_transported[0] > 153) ^ CHANCE16I(1,3,r>>16)) {

/* Increase production */
				if (i->prod_level != 0x80) {
					byte b;

					i->prod_level <<= 1;

					b = i->production_rate[0]*2;
					if (i->production_rate[0] >= 128) b=255;
					i->production_rate[0] = b;
					b = i->production_rate[1]*2;
					if (i->production_rate[1] >= 128) b=255;
					i->production_rate[1] = b;

					str = _industry_prod_up_strings[type];
					goto add_news;
				}
			} else {
decrease_production:

/* Decrease production */
				if (i->prod_level == 4) goto close_industry;
				i->prod_level>>=1;
				i->production_rate[0] = (i->production_rate[0]+1) >> 1;
				i->production_rate[1] = (i->production_rate[1]+1) >> 1;

				str = _industry_prod_down_strings[type];
				goto add_news;
			}
		}
	} else if (_industry_close_mode[type] > 1) {
		/* maybe close */
		if ( (byte)(_cur_year - i->last_prod_year) >= 5 && CHANCE16(1,2)) {
close_industry:
			i->prod_level = 0;
			str = _industry_close_strings[type];
add_news:
			SetDParam(1, type + STR_4802_COAL_MINE);
			SetDParam(0, i->town->index);
			AddNewsItem(str, NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_TILE, NT_ECONOMY, 0), i->xy + TILE_XY(1,1), 0);
		}
	}
}

void IndustryMonthlyLoop()
{
	Industry *i;
	byte old_player = _current_player;
	_current_player = OWNER_NONE;

	FOR_ALL_INDUSTRIES(i) {
		if (i->xy != 0)
			UpdateIndustryStatistics(i);
	}

	i = DEREF_INDUSTRY(RandomRange(lengthof(_industries)));

	if (i->xy == 0) {
		uint32 r;
		if (CHANCE16I(1,9,r=Random()))
			MaybeNewIndustry(r);
	} else if (!_patches.smooth_economy) {
		MaybeCloseIndustry(i);
	}

	_current_player = old_player;

	// production-change
	_industry_sort_dirty = true;
	InvalidateWindow(WC_INDUSTRY_DIRECTORY, 0);
}


void InitializeIndustries()
{
	memset(_industries, 0, sizeof(_industries));
	_total_industries = 0;
	_industry_sort_dirty = true;
}

const TileTypeProcs _tile_type_industry_procs = {
	DrawTile_Industry,					/* draw_tile_proc */
	GetSlopeZ_Industry,					/* get_slope_z_proc */
	ClearTile_Industry,					/* clear_tile_proc */
	GetAcceptedCargo_Industry,	/* get_accepted_cargo_proc */
	GetTileDesc_Industry,				/* get_tile_desc_proc */
	GetTileTrackStatus_Industry,/* get_tile_track_status_proc */
	ClickTile_Industry,					/* click_tile_proc */
	AnimateTile_Industry,				/* animate_tile_proc */
	TileLoop_Industry,					/* tile_loop_proc */
	ChangeTileOwner_Industry,		/* change_tile_owner_proc */
	GetProducedCargo_Industry,  /* get_produced_cargo_proc */
	NULL,												/* vehicle_enter_tile_proc */
	NULL,												/* vehicle_leave_tile_proc */
	GetSlopeTileh_Industry,			/* get_slope_tileh_proc */
};

static const byte _industry_desc[] = {
	SLE_VAR(Industry,xy,							SLE_UINT16),
	SLE_VAR(Industry,width,						SLE_UINT8),
	SLE_VAR(Industry,height,					SLE_UINT8),
	SLE_REF(Industry,town,						REF_TOWN),
	SLE_ARR(Industry,produced_cargo,  SLE_UINT8, 2),
	SLE_ARR(Industry,cargo_waiting,   SLE_UINT16, 2),
	SLE_ARR(Industry,production_rate, SLE_UINT8, 2),
	SLE_ARR(Industry,accepts_cargo,		SLE_UINT8, 3),
	SLE_VAR(Industry,prod_level,			SLE_UINT8),
	SLE_ARR(Industry,last_mo_production,SLE_UINT16, 2),
	SLE_ARR(Industry,last_mo_transported,SLE_UINT16, 2),
	SLE_ARR(Industry,pct_transported,SLE_UINT8, 2),
	SLE_ARR(Industry,total_production,SLE_UINT16, 2),
	SLE_ARR(Industry,total_transported,SLE_UINT16, 2),

	SLE_VAR(Industry,counter,					SLE_UINT16),

	SLE_VAR(Industry,type,						SLE_UINT8),
	SLE_VAR(Industry,owner,						SLE_UINT8),
	SLE_VAR(Industry,color_map,				SLE_UINT8),
	SLE_VAR(Industry,last_prod_year,	SLE_UINT8),
	SLE_VAR(Industry,was_cargo_delivered,SLE_UINT8),

	// reserve extra space in savegame here. (currently 32 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 4, 2, 255),

	SLE_END()
};

static void Save_INDY()
{
	Industry *ind;
	int i = 0;
	// Write the vehicles
	FOR_ALL_INDUSTRIES(ind) {
		if (ind->xy != 0) {
			SlSetArrayIndex(i);
			SlObject(ind, _industry_desc);
		}
		i++;
	}
}

static void Load_INDY()
{
	int index;
	_total_industries = 0;
	while ((index = SlIterateArray()) != -1) {
		SlObject(DEREF_INDUSTRY(index), _industry_desc);
		if (index > _total_industries) _total_industries = index;
	}
}

const ChunkHandler _industry_chunk_handlers[] = {
	{ 'INDY', Save_INDY, Load_INDY, CH_ARRAY | CH_LAST},
};

