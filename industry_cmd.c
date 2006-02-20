/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "clear.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "functions.h"
#include "map.h"
#include "tile.h"
#include "viewport.h"
#include "command.h"
#include "industry.h"
#include "town.h"
#include "vehicle.h"
#include "news.h"
#include "saveload.h"
#include "economy.h"
#include "sound.h"
#include "variables.h"

enum {
	/* Max industries: 64000 (8 * 8000) */
	INDUSTRY_POOL_BLOCK_SIZE_BITS = 3,       /* In bits, so (1 << 3) == 8 */
	INDUSTRY_POOL_MAX_BLOCKS      = 8000,
};

/**
 * Called if a new block is added to the industry-pool
 */
static void IndustryPoolNewBlock(uint start_item)
{
	Industry *i;

	FOR_ALL_INDUSTRIES_FROM(i, start_item) i->index = start_item++;
}

/* Initialize the industry-pool */
MemoryPool _industry_pool = { "Industry", INDUSTRY_POOL_MAX_BLOCKS, INDUSTRY_POOL_BLOCK_SIZE_BITS, sizeof(Industry), &IndustryPoolNewBlock, 0, 0, NULL };

static byte _industry_sound_ctr;
static TileIndex _industry_sound_tile;

void ShowIndustryViewWindow(int industry);
void BuildOilRig(TileIndex tile);
void DeleteOilRig(TileIndex tile);

typedef struct DrawIndustryTileStruct {
	uint32 sprite_1;
	uint32 sprite_2;

	byte subtile_x:4;
	byte subtile_y:4;
	byte width:4;
	byte height:4;
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

typedef struct IndustryTileTable {
	TileIndexDiffC ti;
	byte map5;
} IndustryTileTable;

typedef struct IndustrySpec {
	const IndustryTileTable *const *table;
	byte num_table;
	byte a,b,c;
	byte produced_cargo[2];
	byte production_rate[2];
	byte accepts_cargo[3];
	byte check_proc;
} IndustrySpec;

#include "table/industry_land.h"
#include "table/build_industry.h"

typedef enum IndustryType {
	INDUSTRY_NOT_CLOSABLE,     //! Industry can never close
	INDUSTRY_PRODUCTION,       //! Industry can close and change of production
	INDUSTRY_CLOSABLE,         //! Industry can only close (no production change)
} IndustryType;


static const IndustryType _industry_close_mode[37] = {
	/* COAL_MINE */         INDUSTRY_PRODUCTION,
	/* POWER_STATION */     INDUSTRY_NOT_CLOSABLE,
	/* SAWMILL */           INDUSTRY_CLOSABLE,
	/* FOREST */            INDUSTRY_PRODUCTION,
	/* OIL_REFINERY */      INDUSTRY_CLOSABLE,
	/* OIL_RIG */           INDUSTRY_PRODUCTION,
	/* FACTORY */           INDUSTRY_CLOSABLE,
	/* PRINTING_WORKS */    INDUSTRY_CLOSABLE,
	/* STEEL_MILL */        INDUSTRY_CLOSABLE,
	/* FARM */              INDUSTRY_PRODUCTION,
	/* COPPER_MINE */       INDUSTRY_PRODUCTION,
	/* OIL_WELL */          INDUSTRY_PRODUCTION,
	/* BANK */              INDUSTRY_NOT_CLOSABLE,
	/* FOOD_PROCESS */      INDUSTRY_CLOSABLE,
	/* PAPER_MILL */        INDUSTRY_CLOSABLE,
	/* GOLD_MINE */         INDUSTRY_PRODUCTION,
	/* BANK_2,  */          INDUSTRY_NOT_CLOSABLE,
	/* DIAMOND_MINE */      INDUSTRY_PRODUCTION,
	/* IRON_MINE */         INDUSTRY_PRODUCTION,
	/* FRUIT_PLANTATION */  INDUSTRY_PRODUCTION,
	/* RUBBER_PLANTATION */ INDUSTRY_PRODUCTION,
	/* WATER_SUPPLY */      INDUSTRY_PRODUCTION,
	/* WATER_TOWER */       INDUSTRY_NOT_CLOSABLE,
	/* FACTORY_2 */         INDUSTRY_CLOSABLE,
	/* FARM_2 */            INDUSTRY_PRODUCTION,
	/* LUMBER_MILL */       INDUSTRY_CLOSABLE,
	/* COTTON_CANDY */      INDUSTRY_PRODUCTION,
	/* CANDY_FACTORY */     INDUSTRY_CLOSABLE,
	/* BATTERY_FARM */      INDUSTRY_PRODUCTION,
	/* COLA_WELLS */        INDUSTRY_PRODUCTION,
	/* TOY_SHOP */          INDUSTRY_NOT_CLOSABLE,
	/* TOY_FACTORY */       INDUSTRY_CLOSABLE,
	/* PLASTIC_FOUNTAINS */ INDUSTRY_PRODUCTION,
	/* FIZZY_DRINK_FACTORY */INDUSTRY_CLOSABLE,
	/* BUBBLE_GENERATOR */  INDUSTRY_PRODUCTION,
	/* TOFFEE_QUARRY */     INDUSTRY_PRODUCTION,
	/* SUGAR_MINE */        INDUSTRY_PRODUCTION
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
	STR_4832_ANNOUNCES_IMMINENT_CLOSURE
};


static void IndustryDrawTileProc1(const TileInfo *ti)
{
	const DrawIndustrySpec1Struct *d;
	uint32 image;

	if (!(_m[ti->tile].m1 & 0x80)) return;

	d = &_draw_industry_spec1[_m[ti->tile].m3];

	AddChildSpriteScreen(0x12A7 + d->image_1, d->x, 0);

	image = d->image_2;
	if (image != 0) AddChildSpriteScreen(0x12B0 + image - 1, 8, 41);

	image = d->image_3;
	if (image != 0) {
		AddChildSpriteScreen(0x12AC + image - 1,
			_drawtile_proc1_x[image - 1], _drawtile_proc1_y[image - 1]);
	}
}

static void IndustryDrawTileProc2(const TileInfo *ti)
{
	int x = 0;

	if (_m[ti->tile].m1 & 0x80) {
		x = _industry_anim_offs[_m[ti->tile].m3];
		if ( (byte)x == 0xFF)
			x = 0;
	}

	AddChildSpriteScreen(0x129F, 22 - x, 24 + x);
	AddChildSpriteScreen(0x129E, 6, 0xE);
}

static void IndustryDrawTileProc3(const TileInfo *ti)
{
	if (_m[ti->tile].m1 & 0x80) {
		AddChildSpriteScreen(0x128B, 5, _industry_anim_offs_2[_m[ti->tile].m3]);
	} else {
		AddChildSpriteScreen(4746, 3, 67);
	}
}

static void IndustryDrawTileProc4(const TileInfo *ti)
{
	const DrawIndustrySpec4Struct *d;

	d = &_industry_anim_offs_3[_m[ti->tile].m3];

	if (d->image_1 != 0xFF) {
		AddChildSpriteScreen(0x126F, 0x32 - d->image_1 * 2, 0x60 + d->image_1);
	}

	if (d->image_2 != 0xFF) {
		AddChildSpriteScreen(0x1270, 0x10 - d->image_2 * 2, 100 + d->image_2);
	}

	AddChildSpriteScreen(0x126E, 7, d->image_3);
	AddChildSpriteScreen(0x126D, 0, 42);
}

static void DrawCoalPlantSparkles(const TileInfo *ti)
{
	int image = _m[ti->tile].m1;
	if (image & 0x80) {
		image = GB(image, 2, 5);
		if (image != 0 && image < 7) {
			AddChildSpriteScreen(image + 0x806,
				_coal_plant_sparkles_x[image - 1],
				_coal_plant_sparkles_y[image - 1]
			);
		}
	}
}

typedef void IndustryDrawTileProc(const TileInfo *ti);
static IndustryDrawTileProc * const _industry_draw_tile_procs[5] = {
	IndustryDrawTileProc1,
	IndustryDrawTileProc2,
	IndustryDrawTileProc3,
	IndustryDrawTileProc4,
	DrawCoalPlantSparkles,
};

static void DrawTile_Industry(TileInfo *ti)
{
	const Industry* ind;
	const DrawIndustryTileStruct *dits;
	byte z;
	uint32 image, ormod;

	/* Pointer to industry */
	ind = GetIndustry(_m[ti->tile].m2);
	ormod = (ind->color_map + 0x307) << PALETTE_SPRITE_START;

	/* Retrieve pointer to the draw industry tile struct */
	dits = &_industry_draw_tile_data[(ti->map5 << 2) | GB(_m[ti->tile].m1, 0, 2)];

	image = dits->sprite_1;
	if (image & PALETTE_MODIFIER_COLOR && (image & PALETTE_SPRITE_MASK) == 0)
		image |= ormod;

	z = ti->z;
	/* Add bricks below the industry? */
	if (ti->tileh & 0xF) {
		AddSortableSpriteToDraw(SPR_FOUNDATION_BASE + (ti->tileh & 0xF), ti->x, ti->y, 16, 16, 7, z);
		AddChildSpriteScreen(image, 0x1F, 1);
		z += 8;
	} else {
		/* Else draw regular ground */
		DrawGroundSprite(image);
	}

	/* Add industry on top of the ground? */
	image = dits->sprite_2;
	if (image != 0) {
		if (image & PALETTE_MODIFIER_COLOR && (image & PALETTE_SPRITE_MASK) == 0)
			image |= ormod;

		if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);

		AddSortableSpriteToDraw(image,
			ti->x + dits->subtile_x,
			ti->y + dits->subtile_y,
			dits->width  + 1,
			dits->height + 1,
			dits->dz,
			z);

		if (_display_opt & DO_TRANS_BUILDINGS) return;
	}

	{
		int proc = dits->proc - 1;
		if (proc >= 0) _industry_draw_tile_procs[proc](ti);
	}
}


static uint GetSlopeZ_Industry(const TileInfo* ti)
{
	return GetPartialZ(ti->x & 0xF, ti->y & 0xF, ti->tileh) + ti->z;
}

static uint GetSlopeTileh_Industry(const TileInfo* ti)
{
	return 0;
}

static void GetAcceptedCargo_Industry(TileIndex tile, AcceptedCargo ac)
{
	int m5 = _m[tile].m5;
	CargoID a;

	a = _industry_map5_accepts_1[m5];
	if (a != CT_INVALID) ac[a] = (a == 0) ? 1 : 8;

	a = _industry_map5_accepts_2[m5];
	if (a != CT_INVALID) ac[a] = 8;

	a = _industry_map5_accepts_3[m5];
	if (a != CT_INVALID) ac[a] = 8;
}

static void GetTileDesc_Industry(TileIndex tile, TileDesc *td)
{
	const Industry* i = GetIndustry(_m[tile].m2);

	td->owner = i->owner;
	td->str = STR_4802_COAL_MINE + i->type;
	if ((_m[tile].m1 & 0x80) == 0) {
		SetDParamX(td->dparam, 0, td->str);
		td->str = STR_2058_UNDER_CONSTRUCTION;
	}
}

static int32 ClearTile_Industry(TileIndex tile, byte flags)
{
	Industry *i = GetIndustry(_m[tile].m2);

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

	if (flags & DC_EXEC) DeleteIndustry(i);
	return 0;
}


static const byte _industry_min_cargo[] = {
	5, 5, 5, 30, 5, 5, 5, 5,
	5, 5, 5, 5, 2, 5, 5, 5,
	5, 5, 5, 15, 15, 5, 5, 5,
	5, 5, 30, 5, 30, 5, 5, 5,
	5, 5, 5, 5, 5,
};

static void TransportIndustryGoods(TileIndex tile)
{
	Industry* i = GetIndustry(_m[tile].m2);
	uint cw, am;

	cw = min(i->cargo_waiting[0], 255);
	if (cw > _industry_min_cargo[i->type]/* && i->produced_cargo[0] != 0xFF*/) {
		byte m5;

		i->cargo_waiting[0] -= cw;

		/* fluctuating economy? */
		if (_economy.fluct <= 0) cw = (cw + 1) / 2;

		i->last_mo_production[0] += cw;

		am = MoveGoodsToStation(i->xy, i->width, i->height, i->produced_cargo[0], cw);
		i->last_mo_transported[0] += am;
		if (am != 0 && (m5 = _industry_produce_map5[_m[tile].m5]) != 0xFF) {
			_m[tile].m1 = 0x80;
			_m[tile].m5 = m5;
			MarkTileDirtyByTile(tile);
		}
	}

	cw = min(i->cargo_waiting[1], 255);
	if (cw > _industry_min_cargo[i->type]) {
		i->cargo_waiting[1] -= cw;

		if (_economy.fluct <= 0) cw = (cw + 1) / 2;

		i->last_mo_production[1] += cw;

		am = MoveGoodsToStation(i->xy, i->width, i->height, i->produced_cargo[1], cw);
		i->last_mo_transported[1] += am;
	}
}


static void AnimateTile_Industry(TileIndex tile)
{
	byte m,n;

	switch (_m[tile].m5) {
	case 174:
		if ((_tick_counter & 1) == 0) {
			m = _m[tile].m3 + 1;

			switch (m & 7) {
			case 2:	SndPlayTileFx(SND_2D_RIP_2, tile); break;
			case 6: SndPlayTileFx(SND_29_RIP, tile); break;
			}

			if (m >= 96) {
				m = 0;
				DeleteAnimatedTile(tile);
			}
			_m[tile].m3 = m;

			MarkTileDirtyByTile(tile);
		}
		break;

	case 165:
		if ((_tick_counter & 3) == 0) {
			m = _m[tile].m3;

			if (_industry_anim_offs[m] == 0xFF) {
				SndPlayTileFx(SND_30_CARTOON_SOUND, tile);
			}

			if (++m >= 70) {
				m = 0;
				DeleteAnimatedTile(tile);
			}
			_m[tile].m3 = m;

			MarkTileDirtyByTile(tile);
		}
		break;

	case 162:
		if ((_tick_counter&1) == 0) {
			m = _m[tile].m3;

			if (++m >= 40) {
				m = 0;
				DeleteAnimatedTile(tile);
			}
			_m[tile].m3 = m;

			MarkTileDirtyByTile(tile);
		}
		break;

	// Sparks on a coal plant
	case 10:
		if ((_tick_counter & 3) == 0) {
			m = _m[tile].m1;
			if (GB(m, 2, 5) == 6) {
				SB(_m[tile].m1, 2, 5, 0);
				DeleteAnimatedTile(tile);
			} else {
				_m[tile].m1 = m + (1<<2);
				MarkTileDirtyByTile(tile);
			}
		}
		break;

	case 143:
		if ((_tick_counter & 1) == 0) {
			m = _m[tile].m3 + 1;

			if (m == 1) {
				SndPlayTileFx(SND_2C_MACHINERY, tile);
			} else if (m == 23) {
				SndPlayTileFx(SND_2B_COMEDY_HIT, tile);
			} else if (m == 28) {
				SndPlayTileFx(SND_2A_EXTRACT_AND_POP, tile);
			}

			if (m >= 50 && (m=0,++_m[tile].m4 >= 8)) {
				_m[tile].m4 = 0;
				DeleteAnimatedTile(tile);
			}
			_m[tile].m3 = m;
			MarkTileDirtyByTile(tile);
		}
		break;

	case 148: case 149: case 150: case 151:
	case 152: case 153: case 154: case 155:
		if ((_tick_counter & 3) == 0) {
			m = _m[tile].m5	+ 1;
			if (m == 155+1) m = 148;
			_m[tile].m5 = m;

			MarkTileDirtyByTile(tile);
		}
		break;

	case 30: case 31: case 32:
		if ((_tick_counter & 7) == 0) {
			bool b = CHANCE16(1,7);
			m = _m[tile].m1;
			m = (m & 3) + 1;
			n = _m[tile].m5;
			if (m == 4 && (m=0,++n) == 32+1 && (n=30,b)) {
				_m[tile].m1 = 0x83;
				_m[tile].m5 = 29;
				DeleteAnimatedTile(tile);
			} else {
				SB(_m[tile].m1, 0, 2, m);
				_m[tile].m5 = n;
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
					if (!(_m[tile].m1 & 0x40)) {
						_m[tile].m1 |= 0x40;
						SndPlayTileFx(SND_0B_MINING_MACHINERY, tile);
					}
					if (state & 7)
						return;
				} else {
					if (state & 3)
						return;
				}
				m = (_m[tile].m1 + 1) | 0x40;
				if (m > 0xC2) m = 0xC0;
				_m[tile].m1 = m;
				MarkTileDirtyByTile(tile);
			} else if (state >= 0x200 && state < 0x3A0) {
				int i;
				i = (state < 0x220 || state >= 0x380) ? 7 : 3;
				if (state & i)
					return;

				m = (_m[tile].m1 & 0xBF) - 1;
				if (m < 0x80) m = 0x82;
				_m[tile].m1 = m;
				MarkTileDirtyByTile(tile);
			}
		} break;
	}
}

static void MakeIndustryTileBiggerCase8(TileIndex tile)
{
	TileInfo ti;
	FindLandscapeHeight(&ti, TileX(tile) * 16, TileY(tile) * 16);
	CreateEffectVehicle(ti.x + 15, ti.y + 14, ti.z + 59 + (ti.tileh != 0 ? 8 : 0), EV_CHIMNEY_SMOKE);
}

static void MakeIndustryTileBigger(TileIndex tile, byte size)
{
	byte b = (byte)((size + (1<<2)) & (3<<2));

	if (b != 0) {
		_m[tile].m1 = b | (size & 3);
		return;
	}

	size = (size + 1) & 3;
	if (size == 3) size |= 0x80;
	_m[tile].m1 = size | b;

	MarkTileDirtyByTile(tile);

	if (!(_m[tile].m1 & 0x80))
		return;

	switch (_m[tile].m5) {
	case 8:
		MakeIndustryTileBiggerCase8(tile);
		break;

	case 24:
		if (_m[tile + TileDiffXY(0, 1)].m5 == 24) BuildOilRig(tile);
		break;

	case 143:
	case 162:
	case 165:
		_m[tile].m3 = 0;
		_m[tile].m4 = 0;
		break;

	case 148: case 149: case 150: case 151:
	case 152: case 153: case 154: case 155:
		AddAnimatedTile(tile);
		break;
	}
}


static void TileLoopIndustryCase161(TileIndex tile)
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
		TileX(tile) * 16 + _tileloop_ind_case_161[dir + 0],
		TileY(tile) * 16 + _tileloop_ind_case_161[dir + 4],
		_tileloop_ind_case_161[dir + 8],
		EV_BUBBLE
	);

	if (v != NULL) v->u.special.unk2 = dir;
}

static void TileLoop_Industry(TileIndex tile)
{
	byte n;

	if (!(_m[tile].m1 & 0x80)) {
		MakeIndustryTileBigger(tile, _m[tile].m1);
		return;
	}

	if (_game_mode == GM_EDITOR) return;

	TransportIndustryGoods(tile);

	n = _industry_map5_animation_next[_m[tile].m5];
	if (n != 255) {
		_m[tile].m1 = 0;
		_m[tile].m5 = n;
		MarkTileDirtyByTile(tile);
		return;
	}

#define SET_AND_ANIMATE(tile, a, b)   { _m[tile].m5 = a; _m[tile].m1 = b; AddAnimatedTile(tile); }
#define SET_AND_UNANIMATE(tile, a, b) { _m[tile].m5 = a; _m[tile].m1 = b; DeleteAnimatedTile(tile); }

	switch (_m[tile].m5) {
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

	case 49:
		CreateEffectVehicleAbove(TileX(tile) * 16 + 6, TileY(tile) * 16 + 6, 43, EV_SMOKE);
		break;


	case 143: {
			Industry *i = GetIndustry(_m[tile].m2);
			if (i->was_cargo_delivered) {
				i->was_cargo_delivered = false;
				_m[tile].m4 = 0;
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
		if (CHANCE16(1, 3)) AddAnimatedTile(tile);
		break;
	}
}


static void ClickTile_Industry(TileIndex tile)
{
	ShowIndustryViewWindow(_m[tile].m2);
}

static uint32 GetTileTrackStatus_Industry(TileIndex tile, TransportType mode)
{
	return 0;
}

static void GetProducedCargo_Industry(TileIndex tile, byte *b)
{
	const Industry* i = GetIndustry(_m[tile].m2);

	b[0] = i->produced_cargo[0];
	b[1] = i->produced_cargo[1];
}

static void ChangeTileOwner_Industry(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	/* not used */
}

void DeleteIndustry(Industry *i)
{
	BEGIN_TILE_LOOP(tile_cur, i->width, i->height, i->xy);
		if (IsTileType(tile_cur, MP_INDUSTRY)) {
			if (_m[tile_cur].m2 == i->index) {
				DoClearSquare(tile_cur);
			}
		} else if (IsTileType(tile_cur, MP_STATION) && _m[tile_cur].m5 == 0x4B) {
			DeleteOilRig(tile_cur);
		}
	END_TILE_LOOP(tile_cur, i->width, i->height, i->xy);

	i->xy = 0;
	_industry_sort_dirty = true;
	DeleteSubsidyWithIndustry(i->index);
	DeleteWindowById(WC_INDUSTRY_VIEW, i->index);
	InvalidateWindow(WC_INDUSTRY_DIRECTORY, 0);
}

static const byte _plantfarmfield_type[] = {1, 1, 1, 1, 1, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6};

static bool IsBadFarmFieldTile(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_CLEAR: return IsClearGround(tile, CL_FIELDS) || IsClearGround(tile, CL_SNOW);
		case MP_TREES: return false;
		default:       return true;
	}
}

static bool IsBadFarmFieldTile2(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_CLEAR: return IsClearGround(tile, CL_SNOW);
		case MP_TREES: return false;
		default:       return true;
	}
}

static void SetupFarmFieldFence(TileIndex tile, int size, byte type, int direction)
{
	do {
		tile = TILE_MASK(tile);

		if (IsTileType(tile, MP_CLEAR) || IsTileType(tile, MP_TREES)) {
			byte or = type;

			if (or == 1 && CHANCE16(1, 7)) or = 2;

			if (direction) {
				SetFenceSW(tile, or);
			} else {
				SetFenceSE(tile, or);
			}
		}

		tile += direction ? TileDiffXY(0, 1) : TileDiffXY(1, 0);
	} while (--size);
}

static void PlantFarmField(TileIndex tile)
{
	uint size_x, size_y;
	uint32 r;
	uint count;
	uint counter;
	uint field_type;
	int type;

	if (_opt.landscape == LT_HILLY) {
		if (GetTileZ(tile) + 16 >= _opt.snow_line)
			return;
	}

	/* determine field size */
	r = (Random() & 0x303) + 0x404;
	if (_opt.landscape == LT_HILLY) r += 0x404;
	size_x = GB(r, 0, 8);
	size_y = GB(r, 8, 8);

	/* offset tile to match size */
	tile -= TileDiffXY(size_x / 2, size_y / 2);

	/* check the amount of bad tiles */
	count = 0;
	BEGIN_TILE_LOOP(cur_tile, size_x, size_y, tile)
		cur_tile = TILE_MASK(cur_tile);
		count += IsBadFarmFieldTile(cur_tile);
	END_TILE_LOOP(cur_tile, size_x, size_y, tile)
	if (count * 2 >= size_x * size_y) return;

	/* determine type of field */
	r = Random();
	counter = GB(r, 5, 3);
	field_type = GB(r, 8, 8) * 9 >> 8;

	/* make field */
	BEGIN_TILE_LOOP(cur_tile, size_x, size_y, tile)
		cur_tile = TILE_MASK(cur_tile);
		if (!IsBadFarmFieldTile2(cur_tile)) {
			SetTileType(cur_tile, MP_CLEAR);
			SetTileOwner(cur_tile, OWNER_NONE);
			SetFieldType(cur_tile, field_type);
			SetFenceSW(cur_tile, 0);
			SetFenceSE(cur_tile, 0);
			SetClearGroundDensity(cur_tile, CL_FIELDS, 3);
			SetClearCounter(cur_tile, counter);
			MarkTileDirtyByTile(cur_tile);
		}
	END_TILE_LOOP(cur_tile, size_x, size_y, tile)

	type = 3;
	if (_opt.landscape != LT_HILLY && _opt.landscape != LT_DESERT) {
		type = _plantfarmfield_type[Random() & 0xF];
	}

	SetupFarmFieldFence(tile - TileDiffXY(1, 0), size_y, type, 1);
	SetupFarmFieldFence(tile - TileDiffXY(0, 1), size_x, type, 0);
	SetupFarmFieldFence(tile + TileDiffXY(size_x - 1, 0), size_y, type, 1);
	SetupFarmFieldFence(tile + TileDiffXY(0, size_y - 1), size_x, type, 0);
}

static void MaybePlantFarmField(const Industry* i)
{
	if (CHANCE16(1, 8)) {
		int x = i->width  / 2 + Random() % 31 - 16;
		int y = i->height / 2 + Random() % 31 - 16;
		TileIndex tile = TileAddWrap(i->xy, x, y);
		if (tile != INVALID_TILE) PlantFarmField(tile);
	}
}

static void ChopLumberMillTrees(Industry *i)
{
	static const TileIndexDiffC _chop_dir[] = {
		{ 0,  1},
		{ 1,  0},
		{ 0, -1},
		{-1,  0}
	};

	TileIndex tile = i->xy;
	int a;

	if ((_m[tile].m1 & 0x80) == 0) return;

	/* search outwards as a rectangular spiral */
	for (a = 1; a != 41; a += 2) {
		uint dir;

		for (dir = 0; dir != 4; dir++) {
			int j = a;

			do {
				tile = TILE_MASK(tile);
				if (IsTileType(tile, MP_TREES)) {
					PlayerID old_player = _current_player;
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
				tile += ToTileIndexDiff(_chop_dir[dir]);
			} while (--j);
		}
		tile -= TileDiffXY(1, 1);
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

		if (i->type == IT_FARM) {
			MaybePlantFarmField(i);
		} else if (i->type == IT_LUMBER_MILL && (i->counter & 0x1FF) == 0) {
			ChopLumberMillTrees(i);
		}
	}
}

void OnTick_Industry(void)
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

	if (_game_mode == GM_EDITOR) return;

	FOR_ALL_INDUSTRIES(i) {
		if (i->xy != 0) ProduceIndustryGoods(i);
	}
}


static bool CheckNewIndustry_NULL(TileIndex tile, int type)
{
	return true;
}

static bool CheckNewIndustry_Forest(TileIndex tile, int type)
{
	if (_opt.landscape == LT_HILLY) {
		if (GetTileZ(tile) < _opt.snow_line + 16U) {
			_error_message = STR_4831_FOREST_CAN_ONLY_BE_PLANTED;
			return false;
		}
	}
	return true;
}

extern bool _ignore_restrictions;

/* Oil Rig and Oil Refinery */
static bool CheckNewIndustry_Oil(TileIndex tile, int type)
{
	if (_game_mode == GM_EDITOR && _ignore_restrictions) return true;
	if (_game_mode == GM_EDITOR && type != IT_OIL_RIG)   return true;
	if ((type != IT_OIL_RIG || TileHeight(tile) == 0) &&
			DistanceFromEdge(TILE_ADDXY(tile, 1, 1)) < 16)   return true;

	_error_message = STR_483B_CAN_ONLY_BE_POSITIONED;
	return false;
}

static bool CheckNewIndustry_Farm(TileIndex tile, int type)
{
	if (_opt.landscape == LT_HILLY) {
		if (GetTileZ(tile) + 16 >= _opt.snow_line) {
			_error_message = STR_0239_SITE_UNSUITABLE;
			return false;
		}
	}
	return true;
}

static bool CheckNewIndustry_Plantation(TileIndex tile, int type)
{
	if (GetMapExtraBits(tile) == 1) {
		_error_message = STR_0239_SITE_UNSUITABLE;
		return false;
	}

	return true;
}

static bool CheckNewIndustry_Water(TileIndex tile, int type)
{
	if (GetMapExtraBits(tile) != 1) {
		_error_message = STR_0318_CAN_ONLY_BE_BUILT_IN_DESERT;
		return false;
	}

	return true;
}

static bool CheckNewIndustry_Lumbermill(TileIndex tile, int type)
{
	if (GetMapExtraBits(tile) != 2) {
		_error_message = STR_0317_CAN_ONLY_BE_BUILT_IN_RAINFOREST;
		return false;
	}
	return true;
}

static bool CheckNewIndustry_BubbleGen(TileIndex tile, int type)
{
	return GetTileZ(tile) <= 32;
}

typedef bool CheckNewIndustryProc(TileIndex tile, int type);
static CheckNewIndustryProc * const _check_new_industry_procs[] = {
	CheckNewIndustry_NULL,
	CheckNewIndustry_Forest,
	CheckNewIndustry_Oil,
	CheckNewIndustry_Farm,
	CheckNewIndustry_Plantation,
	CheckNewIndustry_Water,
	CheckNewIndustry_Lumbermill,
	CheckNewIndustry_BubbleGen,
};

static bool CheckSuitableIndustryPos(TileIndex tile)
{
	uint x = TileX(tile);
	uint y = TileY(tile);

	if (x < 2 || y < 2 || x > MapMaxX() - 3 || y > MapMaxY() - 3) {
		_error_message = STR_0239_SITE_UNSUITABLE;
		return false;
	}

	return true;
}

static const Town* CheckMultipleIndustryInTown(TileIndex tile, int type)
{
	const Town* t;
	const Industry* i;

	t = ClosestTownFromTile(tile, (uint)-1);

	if (_patches.multiple_industry_per_town) return t;

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

static bool CheckIfIndustryTilesAreFree(TileIndex tile, const IndustryTileTable* it, int type, const Town* t)
{
	TileInfo ti;

	_error_message = STR_0239_SITE_UNSUITABLE;

	do {
		TileIndex cur_tile = tile + ToTileIndexDiff(it->ti);

		if (!IsValidTile(cur_tile)) {
			if (it->map5 == 0xff) continue;
			return false;
		}

		FindLandscapeHeightByTile(&ti, cur_tile);

		if (it->map5 == 0xFF) {
			if (ti.type != MP_WATER || ti.tileh != 0) return false;
		} else {
			if (!EnsureNoVehicle(cur_tile)) return false;

			if (type == IT_OIL_RIG)  {
				if (ti.type != MP_WATER || ti.map5 != 0) return false;
			} else {
				if (ti.type == MP_WATER && ti.map5 == 0) return false;
				if (IsSteepTileh(ti.tileh))
					return false;

				if (ti.tileh != 0) {
					int t;
					byte bits = _industry_map5_bits[it->map5];

					if (bits & 0x10) return false;

					t = ~ti.tileh;

					if (bits & 1 && (t & (1 + 8))) return false;
					if (bits & 2 && (t & (4 + 8))) return false;
					if (bits & 4 && (t & (1 + 2))) return false;
					if (bits & 8 && (t & (2 + 4))) return false;
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
					if (DistanceMax(t->xy, cur_tile) > 9) return false;
					if (ti.type != MP_HOUSE) goto do_clear;
				} else if (type == IT_WATER_TOWER) {
					if (ti.type != MP_HOUSE) {
						_error_message = STR_0316_CAN_ONLY_BE_BUILT_IN_TOWNS;
						return false;
					}
				} else {
do_clear:
					if (CmdFailed(DoCommandByTile(cur_tile, 0, 0, DC_AUTO, CMD_LANDSCAPE_CLEAR)))
						return false;
				}
			}
		}
	} while ((++it)->ti.x != -0x80);

	return true;
}

static bool CheckIfTooCloseToIndustry(TileIndex tile, int type)
{
	const IndustrySpec* spec = &_industry_spec[type];
	const Industry* i;

	// accepting industries won't be close, not even with patch
	if (_patches.same_industry_close && spec->accepts_cargo[0] == CT_INVALID)
		return true;

	FOR_ALL_INDUSTRIES(i) {
		// check if an industry that accepts the same goods is nearby
		if (i->xy != 0 &&
				DistanceMax(tile, i->xy) <= 14 &&
				spec->accepts_cargo[0] != CT_INVALID &&
				spec->accepts_cargo[0] == i->accepts_cargo[0] && (
					_game_mode != GM_EDITOR ||
					!_patches.same_industry_close ||
					!_patches.multiple_industry_per_town
				)) {
			_error_message = STR_INDUSTRY_TOO_CLOSE;
			return false;
		}

		// check "not close to" field.
		if (i->xy != 0 &&
				(i->type == spec->a || i->type == spec->b || i->type == spec->c) &&
				DistanceMax(tile, i->xy) <= 14) {
			_error_message = STR_INDUSTRY_TOO_CLOSE;
			return false;
		}
	}
	return true;
}

static Industry *AllocateIndustry(void)
{
	Industry *i;

	FOR_ALL_INDUSTRIES(i) {
		if (i->xy == 0) {
			uint index = i->index;

			if (i->index > _total_industries) _total_industries = i->index;

			memset(i, 0, sizeof(*i));
			i->index = index;

			return i;
		}
	}

	/* Check if we can add a block to the pool */
	return AddBlockToPool(&_industry_pool) ? AllocateIndustry() : NULL;
}

static void DoCreateNewIndustry(Industry* i, TileIndex tile, int type, const IndustryTileTable* it, const Town* t, byte owner)
{
	const IndustrySpec *spec;
	uint32 r;
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

	r = Random();
	i->color_map = GB(r, 8, 4);
	i->counter = GB(r, 0, 12);
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

	if (!_generating_world) i->total_production[0] = i->total_production[1] = 0;

	i->prod_level = 0x10;

	do {
		TileIndex cur_tile = tile + ToTileIndexDiff(it->ti);

		if (it->map5 != 0xFF) {
			byte size;

			size = it->ti.x;
			if (size > i->width) i->width = size;
			size = it->ti.y;
			if (size > i->height)i->height = size;

			DoCommandByTile(cur_tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);

			SetTileType(cur_tile, MP_INDUSTRY);
			_m[cur_tile].m5 = it->map5;
			_m[cur_tile].m2 = i->index;
			_m[cur_tile].m1 = _generating_world ? 0x1E : 0; /* maturity */
		}
	} while ((++it)->ti.x != -0x80);

	i->width++;
	i->height++;

	if (i->type == IT_FARM || i->type == IT_FARM_2) {
		tile = i->xy + TileDiffXY(i->width / 2, i->height / 2);
		for (j = 0; j != 50; j++) {
			int x = Random() % 31 - 16;
			int y = Random() % 31 - 16;
			TileIndex new_tile = TileAddWrap(tile, x, y);

			if (new_tile != INVALID_TILE) PlantFarmField(new_tile);
		}
	}
	_industry_sort_dirty = true;
	InvalidateWindow(WC_INDUSTRY_DIRECTORY, 0);
}

/** Build/Fund an industry
 * @param x,y coordinates where industry is built
 * @param p1 industry type @see build_industry.h and @see industry.h
 * @param p2 unused
 */
int32 CmdBuildIndustry(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	const Town* t;
	Industry *i;
	TileIndex tile = TileVirtXY(x, y);
	int num;
	const IndustryTileTable * const *itt;
	const IndustryTileTable *it;
	const IndustrySpec *spec;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	if (!CheckSuitableIndustryPos(tile)) return CMD_ERROR;

	/* Check if the to-be built/founded industry is available for this climate.
	 * Unfortunately we have no easy way of checking, except for looping the table */
	{
		const byte* i;
		bool found = false;

		for (i = &_build_industry_types[_opt_ptr->landscape][0]; i != endof(_build_industry_types[_opt_ptr->landscape]); i++) {
			if (*i == p1) {
				found = true;
				break;
			}
		}
		if (!found) return CMD_ERROR;
	}

	spec = &_industry_spec[p1];
	/* If the patch for raw-material industries is not on, you cannot build raw-material industries.
	 * Raw material industries are industries that do not accept cargo (at least for now)
	 * Exclude the lumber mill (only "raw" industry that can be built) */
	if (!_patches.build_rawmaterial_ind &&
			spec->accepts_cargo[0] == CT_INVALID &&
			spec->accepts_cargo[1] == CT_INVALID &&
			spec->accepts_cargo[2] == CT_INVALID &&
			p1 != IT_LUMBER_MILL) {
		return CMD_ERROR;
	}

	if (!_check_new_industry_procs[spec->check_proc](tile, p1)) return CMD_ERROR;

	t = CheckMultipleIndustryInTown(tile, p1);
	if (t == NULL) return CMD_ERROR;

	num = spec->num_table;
	itt = spec->table;

	do {
		if (--num < 0) return_cmd_error(STR_0239_SITE_UNSUITABLE);
	} while (!CheckIfIndustryTilesAreFree(tile, it = itt[num], p1, t));


	if (!CheckIfTooCloseToIndustry(tile, p1)) return CMD_ERROR;

	i = AllocateIndustry();
	if (i == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) DoCreateNewIndustry(i, tile, p1, it, t, OWNER_NONE);

	return (_price.build_industry >> 5) * _industry_type_costs[p1];
}


Industry *CreateNewIndustry(TileIndex tile, int type)
{
	const Town* t;
	const IndustryTileTable *it;
	Industry *i;

	const IndustrySpec *spec;

	if (!CheckSuitableIndustryPos(tile)) return NULL;

	spec = &_industry_spec[type];

	if (!_check_new_industry_procs[spec->check_proc](tile, type)) return NULL;

	t = CheckMultipleIndustryInTown(tile, type);
	if (t == NULL) return NULL;

	/* pick a random layout */
	it = spec->table[RandomRange(spec->num_table)];

	if (!CheckIfIndustryTilesAreFree(tile, it, type, t)) return NULL;
	if (!CheckIfTooCloseToIndustry(tile, type)) return NULL;

	i = AllocateIndustry();
	if (i == NULL) return NULL;

	DoCreateNewIndustry(i, tile, type, it, t, OWNER_NONE);

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

	if (type == IT_OIL_REFINERY || type == IT_OIL_RIG) {
		// These are always placed next to the coastline, so we scale by the perimeter instead.
		num = ScaleByMapSize1D(num);
	} else {
		num = ScaleByMapSize(num);
	}

	if (_opt.diff.number_industries != 0) {
		PlayerID old_player = _current_player;
		_current_player = OWNER_NONE;
		assert(num > 0);

		do {
			uint i;

			for (i = 0; i < 2000; i++) {
				if (CreateNewIndustry(RandomTile(), type) != NULL) break;
			}
		} while (--num);

		_current_player = old_player;
	}
}

void GenerateIndustries(void)
{
	const byte *b;

	b = _industry_create_table[_opt.landscape];
	do {
		PlaceInitialIndustry(b[1], b[0]);
	} while ( (b+=2)[0] != 0);
}

static void ExtChangeIndustryProduction(Industry *i)
{
	bool closeit = true;
	int j;

	switch (_industry_close_mode[i->type]) {
		case INDUSTRY_NOT_CLOSABLE:
			return;

		case INDUSTRY_CLOSABLE:
			if ((byte)(_cur_year - i->last_prod_year) < 5 || !CHANCE16(1, 180))
				closeit = false;
			break;

		default: /* INDUSTRY_PRODUCTION */
			for (j = 0; j < 2 && i->produced_cargo[j] != CT_INVALID; j++){
				uint32 r = Random();
				int old, new, percent;
				int mag;

				new = old = i->production_rate[j];
				if (CHANCE16I(20, 1024, r))
					new -= ((RandomRange(50) + 10) * old) >> 8;
				if (CHANCE16I(20 + (i->pct_transported[j] * 20 >> 8), 1024, r >> 16))
					new += ((RandomRange(50) + 10) * old) >> 8;

				new = clamp(new, 0, 255);
				if (new == old) {
					closeit = false;
					continue;
				}

				percent = new * 100 / old - 100;
				i->production_rate[j] = new;

				if (new >= _industry_spec[i->type].production_rate[j] / 4)
					closeit = false;

				mag = abs(percent);
				if (mag >= 10) {
					SetDParam(2, mag);
					SetDParam(0, _cargoc.names_s[i->produced_cargo[j]]);
					SetDParam(1, i->index);
					AddNewsItem(
						percent >= 0 ? STR_INDUSTRY_PROD_GOUP : STR_INDUSTRY_PROD_GODOWN,
						NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_TILE, NT_ECONOMY, 0),
						i->xy + TileDiffXY(1, 1), 0
					);
				}
			}
			break;
	}

	if (closeit) {
		i->prod_level = 0;
		SetDParam(0, i->index);
		AddNewsItem(
			_industry_close_strings[i->type],
			NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_TILE, NT_ECONOMY, 0),
			i->xy + TileDiffXY(1, 1), 0
		);
	}
}


static void UpdateIndustryStatistics(Industry *i)
{
	byte pct;

	if (i->produced_cargo[0] != CT_INVALID) {
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

	if (i->produced_cargo[1] != CT_INVALID) {
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


	if (i->produced_cargo[0] != CT_INVALID || i->produced_cargo[1] != CT_INVALID)
		InvalidateWindow(WC_INDUSTRY_VIEW, i->index);

	if (i->prod_level == 0) {
		DeleteIndustry(i);
	} else if (_patches.smooth_economy) {
		ExtChangeIndustryProduction(i);
	}
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

	type = _new_industry_rand[_opt.landscape][GB(r, 16, 5)];

	if (type == IT_OIL_WELL && _date > 10958) return;
	if (type == IT_OIL_RIG  && _date < 14610) return;

	j = 2000;
	for (;;) {
		i = CreateNewIndustry(RandomTile(), type);
		if (i != NULL) break;
		if (--j == 0) return;
	}

	SetDParam(0, type + STR_4802_COAL_MINE);
	SetDParam(1, i->town->index);
	AddNewsItem(
		(type != IT_FOREST) ?
			STR_482D_NEW_UNDER_CONSTRUCTION : STR_482E_NEW_BEING_PLANTED_NEAR,
		NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_TILE, NT_ECONOMY,0), i->xy, 0
	);
}

static void ChangeIndustryProduction(Industry *i)
{
	bool only_decrease = false;
	StringID str = STR_NULL;
	int type = i->type;

	switch (_industry_close_mode[type]) {
		case INDUSTRY_NOT_CLOSABLE:
			return;

		case INDUSTRY_PRODUCTION:
			/* decrease or increase */
			if (type == IT_OIL_WELL && _opt.landscape == LT_NORMAL)
				only_decrease = true;

			if (only_decrease || CHANCE16(1,3)) {
				/* If you transport > 60%, 66% chance we increase, else 33% chance we increase */
				if (!only_decrease && (i->pct_transported[0] > 153) != CHANCE16(1,3)) {
					/* Increase production */
					if (i->prod_level != 0x80) {
						byte b;

						i->prod_level <<= 1;

						b = i->production_rate[0] * 2;
						if (i->production_rate[0] >= 128)
							b = 0xFF;
						i->production_rate[0] = b;

						b = i->production_rate[1] * 2;
						if (i->production_rate[1] >= 128)
							b = 0xFF;
						i->production_rate[1] = b;

						str = _industry_prod_up_strings[type];
					}
				} else {
					/* Decrease production */
					if (i->prod_level == 4) {
						i->prod_level = 0;
						str = _industry_close_strings[type];
					} else {
						i->prod_level >>= 1;
						i->production_rate[0] = (i->production_rate[0] + 1) >> 1;
						i->production_rate[1] = (i->production_rate[1] + 1) >> 1;

						str = _industry_prod_down_strings[type];
					}
				}
			}
			break;

		case INDUSTRY_CLOSABLE:
			/* maybe close */
			if ( (byte)(_cur_year - i->last_prod_year) >= 5 && CHANCE16(1,2)) {
				i->prod_level = 0;
				str = _industry_close_strings[type];
			}
			break;
	}

	if (str != STR_NULL) {
		SetDParam(0, i->index);
		AddNewsItem(str, NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_TILE, NT_ECONOMY, 0), i->xy + TileDiffXY(1, 1), 0);
	}
}

void IndustryMonthlyLoop(void)
{
	Industry *i;
	PlayerID old_player = _current_player;
	_current_player = OWNER_NONE;

	FOR_ALL_INDUSTRIES(i) {
		if (i->xy != 0) UpdateIndustryStatistics(i);
	}

	/* 3% chance that we start a new industry */
	if (CHANCE16(3, 100)) {
		MaybeNewIndustry(Random());
	} else if (!_patches.smooth_economy && _total_industries > 0) {
		i = GetIndustry(RandomRange(_total_industries));
		if (i->xy != 0) ChangeIndustryProduction(i);
	}

	_current_player = old_player;

	// production-change
	_industry_sort_dirty = true;
	InvalidateWindow(WC_INDUSTRY_DIRECTORY, 0);
}


void InitializeIndustries(void)
{
	CleanPool(&_industry_pool);
	AddBlockToPool(&_industry_pool);

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

static const SaveLoad _industry_desc[] = {
	SLE_CONDVAR(Industry, xy,					SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Industry, xy,					SLE_UINT32, 6, SL_MAX_VERSION),
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
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 4, 2, SL_MAX_VERSION),

	SLE_END()
};

static void Save_INDY(void)
{
	Industry *ind;

	// Write the vehicles
	FOR_ALL_INDUSTRIES(ind) {
		if (ind->xy != 0) {
			SlSetArrayIndex(ind->index);
			SlObject(ind, _industry_desc);
		}
	}
}

static void Load_INDY(void)
{
	int index;

	_total_industries = 0;

	while ((index = SlIterateArray()) != -1) {
		Industry *i;

		if (!AddBlockIfNeeded(&_industry_pool, index))
			error("Industries: failed loading savegame: too many industries");

		i = GetIndustry(index);
		SlObject(i, _industry_desc);

		if (index > _total_industries) _total_industries = index;
	}
}

const ChunkHandler _industry_chunk_handlers[] = {
	{ 'INDY', Save_INDY, Load_INDY, CH_ARRAY | CH_LAST},
};
