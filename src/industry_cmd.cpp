/* $Id$ */

/** @file industry_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "clear_map.h"
#include "functions.h"
#include "industry_map.h"
#include "station_map.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "map.h"
#include "tile.h"
#include "landscape.h"
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
#include "table/industry_land.h"
#include "table/build_industry.h"
#include "genworld.h"
#include "date.h"
#include "water_map.h"
#include "tree_map.h"
#include "cargotype.h"

void ShowIndustryViewWindow(int industry);
void BuildOilRig(TileIndex tile);

static byte _industry_sound_ctr;
static TileIndex _industry_sound_tile;

int _total_industries;                      //general counter
uint16 _industry_counts[NUM_INDUSTRYTYPES]; // Number of industries per type ingame

const Industry **_industry_sort;
bool _industry_sort_dirty;

IndustrySpec _industry_specs[NUM_INDUSTRYTYPES];
IndustryTileSpec _industry_tile_specs[NUM_INDUSTRYTILES];

/** This function initialize the spec arrays of both
 * industry and industry tiles.
 * It adjusts the enabling of the industry too, based on climate availability.
 * This will allow for clearer testings */
void ResetIndustries()
{
	memset(&_industry_specs, 0, sizeof(_industry_specs));
	memcpy(&_industry_specs, &_origin_industry_specs, sizeof(_origin_industry_specs));

	/* once performed, enable only the current climate industries */
	for (IndustryType i = 0; i < NUM_INDUSTRYTYPES; i++) {
		_industry_specs[i].enabled = HASBIT(_origin_industry_specs[i].climate_availability, _opt.landscape);
	}


	memset(&_industry_tile_specs, 0, sizeof(_industry_tile_specs));
	memcpy(&_industry_tile_specs, &_origin_industry_tile_specs, sizeof(_origin_industry_tile_specs));
}

/**
 * Called if a new block is added to the industry-pool
 */
static void IndustryPoolNewBlock(uint start_item)
{
	Industry *i;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (i = GetIndustry(start_item); i != NULL; i = (i->index + 1U < GetIndustryPoolSize()) ? GetIndustry(i->index + 1U) : NULL) i->index = start_item++;
}

DEFINE_OLD_POOL(Industry, Industry, IndustryPoolNewBlock, NULL)

/**
 * Retrieve the type for this industry.  Although it is accessed by a tile,
 * it will return the general type of industry, and not the sprite index
 * as would do GetIndustryGfx.
 * @param tile that is queried
 * @pre IsTileType(tile, MP_INDUSTRY)
 * @return general type for this industry, as defined in industry.h
 **/
IndustryType GetIndustryType(TileIndex tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));

	const Industry *ind = GetIndustryByTile(tile);
	return IsValidIndustry(ind) ? ind->type : (IndustryType)IT_INVALID;
}

/**
 * Accessor for array _industry_specs.
 * This will ensure at once : proper access and
 * not allowing modifications of it.
 * @param thistype of industry (which is the index in _industry_specs)
 * @pre thistype < NUM_INDUSTRYTYPES
 * @return a pointer to the corresponding industry spec
 **/
const IndustrySpec *GetIndustrySpec(IndustryType thistype)
{
	assert(thistype < NUM_INDUSTRYTYPES);
	return &_industry_specs[thistype];
}

/**
 * Accessor for array _industry_tile_specs.
 * This will ensure at once : proper access and
 * not allowing modifications of it.
 * @param gfx of industrytile (which is the index in _industry_specs)
 * @pre gfx < INVALID_INDUSTRYTILE
 * @return a pointer to the corresponding industrytile spec
 **/
const IndustryTileSpec *GetIndustryTileSpec(IndustryGfx gfx)
{
	assert(gfx < INVALID_INDUSTRYTILE);
	return &_industry_tile_specs[gfx];
}

void DestroyIndustry(Industry *i)
{
	BEGIN_TILE_LOOP(tile_cur, i->width, i->height, i->xy);
		if (IsTileType(tile_cur, MP_INDUSTRY)) {
			if (GetIndustryIndex(tile_cur) == i->index) {
				DoClearSquare(tile_cur);
			}
		} else if (IsTileType(tile_cur, MP_STATION) && IsOilRig(tile_cur)) {
			DeleteOilRig(tile_cur);
		}
	END_TILE_LOOP(tile_cur, i->width, i->height, i->xy);

	if (GetIndustrySpec(i->type)->behaviour & INDUSTRYBEH_PLANT_FIELDS) {
		/* Remove the farmland and convert it to regular tiles over time. */
		BEGIN_TILE_LOOP(tile_cur, 42, 42, i->xy - TileDiffXY(21, 21)) {
			tile_cur = TILE_MASK(tile_cur);
			if (IsTileType(tile_cur, MP_CLEAR) && IsClearGround(tile_cur, CLEAR_FIELDS) &&
					GetIndustryIndexOfField(tile_cur) == i->index) {
				SetIndustryIndexOfField(tile_cur, INVALID_INDUSTRY);
			}
		} END_TILE_LOOP(tile_cur, 42, 42, i->xy - TileDiff(21, 21))
	}

	_industry_sort_dirty = true;
	DecIndustryTypeCount(i->type);

	DeleteSubsidyWithIndustry(i->index);
	DeleteWindowById(WC_INDUSTRY_VIEW, i->index);
	InvalidateWindow(WC_INDUSTRY_DIRECTORY, 0);
}

static void IndustryDrawSugarMine(const TileInfo *ti)
{
	const DrawIndustryAnimationStruct *d;

	if (!IsIndustryCompleted(ti->tile)) return;

	d = &_draw_industry_spec1[GetIndustryAnimationState(ti->tile)];

	AddChildSpriteScreen(SPR_IT_SUGAR_MINE_SIEVE + d->image_1, PAL_NONE, d->x, 0);

	if (d->image_2 != 0) {
		AddChildSpriteScreen(SPR_IT_SUGAR_MINE_CLOUDS + d->image_2 - 1, PAL_NONE, 8, 41);
	}

	if (d->image_3 != 0) {
		AddChildSpriteScreen(SPR_IT_SUGAR_MINE_PILE + d->image_3 - 1, PAL_NONE,
			_drawtile_proc1[d->image_3 - 1].x, _drawtile_proc1[d->image_3 - 1].y);
	}
}

static void IndustryDrawToffeeQuarry(const TileInfo *ti)
{
	uint8 x = 0;

	if (IsIndustryCompleted(ti->tile)) {
		x = _industry_anim_offs_toffee[GetIndustryAnimationState(ti->tile)];
		if (x == 0xFF)
			x = 0;
	}

	AddChildSpriteScreen(SPR_IT_TOFFEE_QUARRY_SHOVEL, PAL_NONE, 22 - x, 24 + x);
	AddChildSpriteScreen(SPR_IT_TOFFEE_QUARRY_TOFFEE, PAL_NONE, 6, 14);
}

static void IndustryDrawBubbleGenerator( const TileInfo *ti)
{
	if (IsIndustryCompleted(ti->tile)) {
		AddChildSpriteScreen(SPR_IT_BUBBLE_GENERATOR_BUBBLE, PAL_NONE, 5, _industry_anim_offs_bubbles[GetIndustryAnimationState(ti->tile)]);
	} else {
		AddChildSpriteScreen(SPR_IT_BUBBLE_GENERATOR_SPRING, PAL_NONE, 3, 67);
	}
}

static void IndustryDrawToyFactory(const TileInfo *ti)
{
	const DrawIndustryAnimationStruct *d;

	d = &_industry_anim_offs_toys[GetIndustryAnimationState(ti->tile)];

	if (d->image_1 != 0xFF) {
		AddChildSpriteScreen(SPR_IT_TOY_FACTORY_CLAY, PAL_NONE, d->x, 96 + d->image_1);
	}

	if (d->image_2 != 0xFF) {
		AddChildSpriteScreen(SPR_IT_TOY_FACTORY_ROBOT, PAL_NONE, 16 - d->image_2 * 2, 100 + d->image_2);
	}

	AddChildSpriteScreen(SPR_IT_TOY_FACTORY_STAMP, PAL_NONE, 7, d->image_3);
	AddChildSpriteScreen(SPR_IT_TOY_FACTORY_STAMP_HOLDER, PAL_NONE, 0, 42);
}

static void IndustryDrawCoalPlantSparks(const TileInfo *ti)
{
	if (IsIndustryCompleted(ti->tile)) {
		uint8 image = GetIndustryAnimationState(ti->tile);

		if (image != 0 && image < 7) {
			AddChildSpriteScreen(image + SPR_IT_POWER_PLANT_TRANSFORMERS,
				PAL_NONE,
				_coal_plant_sparks[image - 1].x,
				_coal_plant_sparks[image - 1].y
			);
		}
	}
}

typedef void IndustryDrawTileProc(const TileInfo *ti);
static IndustryDrawTileProc * const _industry_draw_tile_procs[5] = {
	IndustryDrawSugarMine,
	IndustryDrawToffeeQuarry,
	IndustryDrawBubbleGenerator,
	IndustryDrawToyFactory,
	IndustryDrawCoalPlantSparks,
};

static void DrawTile_Industry(TileInfo *ti)
{
	const IndustryGfx gfx = GetIndustryGfx(ti->tile);
	const Industry *ind;
	const DrawBuildingsTileStruct *dits;
	byte z;
	SpriteID image;
	SpriteID pal;

	/* Pointer to industry */
	ind = GetIndustryByTile(ti->tile);

	/* Retrieve pointer to the draw industry tile struct */
	dits = &_industry_draw_tile_data[gfx << 2 | (GetIndustryTileSpec(gfx)->anim_state ?
			GetIndustryAnimationState(ti->tile) & 3 :
			GetIndustryConstructionStage(ti->tile))];

	image = dits->ground.sprite;
	if (HASBIT(image, PALETTE_MODIFIER_COLOR) && dits->ground.pal == PAL_NONE) {
		pal = GENERAL_SPRITE_COLOR(ind->random_color);
	} else {
		pal = dits->ground.pal;
	}

	z = ti->z;
	/* Add bricks below the industry? */
	if (ti->tileh != SLOPE_FLAT) {
		AddSortableSpriteToDraw(SPR_FOUNDATION_BASE + ti->tileh, PAL_NONE, ti->x, ti->y, 16, 16, 7, z);
		AddChildSpriteScreen(image, pal, 31, 1);
		z += TILE_HEIGHT;
	} else {
		/* Else draw regular ground */
		DrawGroundSprite(image, pal);
	}

	/* Add industry on top of the ground? */
	image = dits->building.sprite;
	if (image != 0) {
		if (HASBIT(_transparent_opt, TO_INDUSTRIES)) {
			SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
			pal = PALETTE_TO_TRANSPARENT;
		} else {
			pal = dits->building.pal;
		}

		AddSortableSpriteToDraw(image, pal,
			ti->x + dits->subtile_x,
			ti->y + dits->subtile_y,
			dits->width  + 1,
			dits->height + 1,
			dits->dz,
			z);

		if (HASBIT(_transparent_opt, TO_INDUSTRIES)) return;
	}

	{
		int proc = dits->draw_proc - 1;
		if (proc >= 0) _industry_draw_tile_procs[proc](ti);
	}
}

static uint GetSlopeZ_Industry(TileIndex tile, uint x, uint y)
{
	return GetTileMaxZ(tile);
}

static Slope GetSlopeTileh_Industry(TileIndex tile, Slope tileh)
{
	return SLOPE_FLAT;
}

static void GetAcceptedCargo_Industry(TileIndex tile, AcceptedCargo ac)
{
	const IndustryTileSpec *itspec = GetIndustryTileSpec(GetIndustryGfx(tile));
	CargoID a;

	for (byte i = 0; i < lengthof(itspec->accepts_cargo); i++) {
		a = itspec->accepts_cargo[i];
		if (a != CT_INVALID) ac[a] = itspec->acceptance[i];
	}
}

static void GetTileDesc_Industry(TileIndex tile, TileDesc *td)
{
	const Industry *i = GetIndustryByTile(tile);

	td->owner = i->owner;
	td->str = GetIndustrySpec(i->type)->name;
	if (!IsIndustryCompleted(tile)) {
		SetDParamX(td->dparam, 0, td->str);
		td->str = STR_2058_UNDER_CONSTRUCTION;
	}
}

static int32 ClearTile_Industry(TileIndex tile, byte flags)
{
	Industry *i = GetIndustryByTile(tile);
	const IndustrySpec *indspec = GetIndustrySpec(i->type);

	/* water can destroy industries
	 * in editor you can bulldoze industries
	 * with magic_bulldozer cheat you can destroy industries
	 * (area around OILRIG is water, so water shouldn't flood it
	 */
	if ((_current_player != OWNER_WATER && _game_mode != GM_EDITOR &&
			!_cheats.magic_bulldozer.value) ||
			(_current_player == OWNER_WATER && (indspec->behaviour & INDUSTRYBEH_BUILT_ONWATER))) {
		SetDParam(0, indspec->name);
		return_cmd_error(STR_4800_IN_THE_WAY);
	}

	if (flags & DC_EXEC) DeleteIndustry(i);
	return 0;
}

static void TransportIndustryGoods(TileIndex tile)
{
	Industry *i = GetIndustryByTile(tile);
	const IndustrySpec *indspec = GetIndustrySpec(i->type);
	uint cw, am;

	cw = min(i->cargo_waiting[0], 255);
	if (cw > indspec->minimal_cargo/* && i->produced_cargo[0] != 0xFF*/) {
		i->cargo_waiting[0] -= cw;

		/* fluctuating economy? */
		if (_economy.fluct <= 0) cw = (cw + 1) / 2;

		i->this_month_production[0] += cw;

		am = MoveGoodsToStation(i->xy, i->width, i->height, indspec->produced_cargo[0], cw);
		i->this_month_transported[0] += am;
		if (am != 0) {
			uint newgfx = GetIndustryTileSpec(GetIndustryGfx(tile))->anim_production;

			if (newgfx != INDUSTRYTILE_NOANIM) {
				ResetIndustryConstructionStage(tile);
				SetIndustryCompleted(tile, true);
				SetIndustryGfx(tile, newgfx);
				MarkTileDirtyByTile(tile);
			}
		}
	}

	cw = min(i->cargo_waiting[1], 255);
	if (cw > indspec->minimal_cargo) {
		i->cargo_waiting[1] -= cw;

		if (_economy.fluct <= 0) cw = (cw + 1) / 2;

		i->this_month_production[1] += cw;

		am = MoveGoodsToStation(i->xy, i->width, i->height, indspec->produced_cargo[1], cw);
		i->this_month_transported[1] += am;
	}
}


static void AnimateTile_Industry(TileIndex tile)
{
	byte m;

	switch (GetIndustryGfx(tile)) {
	case GFX_SUGAR_MINE_SIEVE:
		if ((_tick_counter & 1) == 0) {
			m = GetIndustryAnimationState(tile) + 1;

			switch (m & 7) {
			case 2: SndPlayTileFx(SND_2D_RIP_2, tile); break;
			case 6: SndPlayTileFx(SND_29_RIP, tile); break;
			}

			if (m >= 96) {
				m = 0;
				DeleteAnimatedTile(tile);
			}
			SetIndustryAnimationState(tile, m);

			MarkTileDirtyByTile(tile);
		}
		break;

	case GFX_TOFFEE_QUARY:
		if ((_tick_counter & 3) == 0) {
			m = GetIndustryAnimationState(tile);

			if (_industry_anim_offs_toffee[m] == 0xFF) {
				SndPlayTileFx(SND_30_CARTOON_SOUND, tile);
			}

			if (++m >= 70) {
				m = 0;
				DeleteAnimatedTile(tile);
			}
			SetIndustryAnimationState(tile, m);

			MarkTileDirtyByTile(tile);
		}
		break;

	case GFX_BUBBLE_CATCHER:
		if ((_tick_counter & 1) == 0) {
			m = GetIndustryAnimationState(tile);

			if (++m >= 40) {
				m = 0;
				DeleteAnimatedTile(tile);
			}
			SetIndustryAnimationState(tile, m);

			MarkTileDirtyByTile(tile);
		}
		break;

	/* Sparks on a coal plant */
	case GFX_POWERPLANT_SPARKS:
		if ((_tick_counter & 3) == 0) {
			m = GetIndustryAnimationState(tile);
			if (m == 6) {
				SetIndustryAnimationState(tile, 0);
				DeleteAnimatedTile(tile);
			} else {
				SetIndustryAnimationState(tile, m + 1);
				MarkTileDirtyByTile(tile);
			}
		}
		break;

	case GFX_TOY_FACTORY:
		if ((_tick_counter & 1) == 0) {
			m = GetIndustryAnimationState(tile) + 1;

			switch (m) {
				case  1: SndPlayTileFx(SND_2C_MACHINERY, tile); break;
				case 23: SndPlayTileFx(SND_2B_COMEDY_HIT, tile); break;
				case 28: SndPlayTileFx(SND_2A_EXTRACT_AND_POP, tile); break;
				default:
					if (m >= 50) {
						int n = GetIndustryAnimationLoop(tile) + 1;
						m = 0;
						if (n >= 8) {
							n = 0;
							DeleteAnimatedTile(tile);
						}
						SetIndustryAnimationLoop(tile, n);
					}
			}

			SetIndustryAnimationState(tile, m);
			MarkTileDirtyByTile(tile);
		}
		break;

	case GFX_PLASTIC_FOUNTAIN_ANIMATED_1: case GFX_PLASTIC_FOUNTAIN_ANIMATED_2:
	case GFX_PLASTIC_FOUNTAIN_ANIMATED_3: case GFX_PLASTIC_FOUNTAIN_ANIMATED_4:
	case GFX_PLASTIC_FOUNTAIN_ANIMATED_5: case GFX_PLASTIC_FOUNTAIN_ANIMATED_6:
	case GFX_PLASTIC_FOUNTAIN_ANIMATED_7: case GFX_PLASTIC_FOUNTAIN_ANIMATED_8:
		if ((_tick_counter & 3) == 0) {
			IndustryGfx gfx = GetIndustryGfx(tile);

			gfx = (gfx < 155) ? gfx + 1 : 148;
			SetIndustryGfx(tile, gfx);
			MarkTileDirtyByTile(tile);
		}
		break;

	case GFX_OILWELL_ANIMATED_1:
	case GFX_OILWELL_ANIMATED_2:
	case GFX_OILWELL_ANIMATED_3:
		if ((_tick_counter & 7) == 0) {
			bool b = CHANCE16(1, 7);
			IndustryGfx gfx = GetIndustryGfx(tile);

			m = GetIndustryAnimationState(tile) + 1;
			if (m == 4 && (m = 0, ++gfx) == GFX_OILWELL_ANIMATED_3 + 1 && (gfx = GFX_OILWELL_ANIMATED_1, b)) {
				SetIndustryGfx(tile, GFX_OILWELL_NOT_ANIMATED);
				SetIndustryConstructionStage(tile, 3);
				DeleteAnimatedTile(tile);
			} else {
				SetIndustryAnimationState(tile, m);
				SetIndustryGfx(tile, gfx);
				MarkTileDirtyByTile(tile);
			}
		}
		break;

	case GFX_COAL_MINE_TOWER_ANIMATED:
	case GFX_COPPER_MINE_TOWER_ANIMATED:
	case GFX_GOLD_MINE_TOWER_ANIMATED: {
			int state = _tick_counter & 0x7FF;

			if ((state -= 0x400) < 0)
				return;

			if (state < 0x1A0) {
				if (state < 0x20 || state >= 0x180) {
					m = GetIndustryAnimationState(tile);
					if (!(m & 0x40)) {
						SetIndustryAnimationState(tile, m | 0x40);
						SndPlayTileFx(SND_0B_MINING_MACHINERY, tile);
					}
					if (state & 7)
						return;
				} else {
					if (state & 3)
						return;
				}
				m = (GetIndustryAnimationState(tile) + 1) | 0x40;
				if (m > 0xC2) m = 0xC0;
				SetIndustryAnimationState(tile, m);
				MarkTileDirtyByTile(tile);
			} else if (state >= 0x200 && state < 0x3A0) {
				int i;
				i = (state < 0x220 || state >= 0x380) ? 7 : 3;
				if (state & i)
					return;

				m = (GetIndustryAnimationState(tile) & 0xBF) - 1;
				if (m < 0x80) m = 0x82;
				SetIndustryAnimationState(tile, m);
				MarkTileDirtyByTile(tile);
			}
		} break;
	}
}

static void CreateIndustryEffectSmoke(TileIndex tile)
{
	uint x = TileX(tile) * TILE_SIZE;
	uint y = TileY(tile) * TILE_SIZE;
	uint z = GetTileMaxZ(tile);

	CreateEffectVehicle(x + 15, y + 14, z + 59, EV_CHIMNEY_SMOKE);
}

static void MakeIndustryTileBigger(TileIndex tile)
{
	byte cnt = GetIndustryConstructionCounter(tile) + 1;
	byte stage;

	if (cnt != 4) {
		SetIndustryConstructionCounter(tile, cnt);
		return;
	}

	stage = GetIndustryConstructionStage(tile) + 1;
	SetIndustryConstructionCounter(tile, 0);
	SetIndustryConstructionStage(tile, stage);
	if (stage == 3) {
		SetIndustryCompleted(tile, true);
	}

	MarkTileDirtyByTile(tile);

	if (!IsIndustryCompleted(tile)) return;

	switch (GetIndustryGfx(tile)) {
	case GFX_POWERPLANT_CHIMNEY:
		CreateIndustryEffectSmoke(tile);
		break;

	case GFX_OILRIG_1:
		if (GetIndustryGfx(tile + TileDiffXY(0, 1)) == GFX_OILRIG_1) BuildOilRig(tile);
		break;

	case GFX_TOY_FACTORY:
	case GFX_BUBBLE_CATCHER:
	case GFX_TOFFEE_QUARY:
		SetIndustryAnimationState(tile, 0);
		SetIndustryAnimationLoop(tile, 0);
		break;

	case GFX_PLASTIC_FOUNTAIN_ANIMATED_1: case GFX_PLASTIC_FOUNTAIN_ANIMATED_2:
	case GFX_PLASTIC_FOUNTAIN_ANIMATED_3: case GFX_PLASTIC_FOUNTAIN_ANIMATED_4:
	case GFX_PLASTIC_FOUNTAIN_ANIMATED_5: case GFX_PLASTIC_FOUNTAIN_ANIMATED_6:
	case GFX_PLASTIC_FOUNTAIN_ANIMATED_7: case GFX_PLASTIC_FOUNTAIN_ANIMATED_8:
		AddAnimatedTile(tile);
		break;
	}
}


static void TileLoopIndustry_BubbleGenerator(TileIndex tile)
{
	int dir;
	Vehicle *v;
	static const int8 _tileloop_ind_case_161[12] = {
		11,   0, -4, -14,
		-4, -10, -4,   1,
		49,  59, 60,  65,
	};

	SndPlayTileFx(SND_2E_EXTRACT_AND_POP, tile);

	dir = Random() & 3;

	v = CreateEffectVehicleAbove(
		TileX(tile) * TILE_SIZE + _tileloop_ind_case_161[dir + 0],
		TileY(tile) * TILE_SIZE + _tileloop_ind_case_161[dir + 4],
		_tileloop_ind_case_161[dir + 8],
		EV_BUBBLE
	);

	if (v != NULL) v->u.special.unk2 = dir;
}

static void TileLoop_Industry(TileIndex tile)
{
	IndustryGfx newgfx;
	IndustryGfx gfx;

	if (!IsIndustryCompleted(tile)) {
		MakeIndustryTileBigger(tile);
		return;
	}

	if (_game_mode == GM_EDITOR) return;

	TransportIndustryGoods(tile);

	newgfx = GetIndustryTileSpec(GetIndustryGfx(tile))->anim_next;
	if (newgfx != INDUSTRYTILE_NOANIM) {
		ResetIndustryConstructionStage(tile);
		SetIndustryGfx(tile, newgfx);
		MarkTileDirtyByTile(tile);
		return;
	}

	gfx = GetIndustryGfx(tile);

	switch (gfx) {
	case GFX_OILRIG_1: // coast line at oilrigs
	case GFX_OILRIG_2:
	case GFX_OILRIG_3:
	case GFX_OILRIG_4:
	case GFX_OILRIG_5:
		TileLoop_Water(tile);
		break;

	case GFX_COAL_MINE_TOWER_NOT_ANIMATED:
	case GFX_COPPER_MINE_TOWER_NOT_ANIMATED:
	case GFX_GOLD_MINE_TOWER_NOT_ANIMATED:
		if (!(_tick_counter & 0x400) && CHANCE16(1, 2)) {
			switch (gfx) {
				case GFX_COAL_MINE_TOWER_NOT_ANIMATED:   gfx = GFX_COAL_MINE_TOWER_ANIMATED;   break;
				case GFX_COPPER_MINE_TOWER_NOT_ANIMATED: gfx = GFX_COPPER_MINE_TOWER_ANIMATED; break;
				case GFX_GOLD_MINE_TOWER_NOT_ANIMATED:   gfx = GFX_GOLD_MINE_TOWER_ANIMATED;   break;
			}
			SetIndustryGfx(tile, gfx);
			SetIndustryAnimationState(tile, 0x80);
			AddAnimatedTile(tile);
		}
		break;

	case GFX_OILWELL_NOT_ANIMATED:
		if (CHANCE16(1, 6)) {
			SetIndustryGfx(tile, GFX_OILWELL_ANIMATED_1);
			SetIndustryAnimationState(tile, 0);
			AddAnimatedTile(tile);
		}
		break;

	case GFX_COAL_MINE_TOWER_ANIMATED:
	case GFX_COPPER_MINE_TOWER_ANIMATED:
	case GFX_GOLD_MINE_TOWER_ANIMATED:
		if (!(_tick_counter & 0x400)) {
			switch (gfx) {
				case GFX_COAL_MINE_TOWER_ANIMATED:   gfx = GFX_COAL_MINE_TOWER_NOT_ANIMATED;   break;
				case GFX_COPPER_MINE_TOWER_ANIMATED: gfx = GFX_COPPER_MINE_TOWER_NOT_ANIMATED; break;
				case GFX_GOLD_MINE_TOWER_ANIMATED:   gfx = GFX_GOLD_MINE_TOWER_NOT_ANIMATED;   break;
			}
			SetIndustryGfx(tile, gfx);
			SetIndustryCompleted(tile, true);
			SetIndustryConstructionStage(tile, 3);
			DeleteAnimatedTile(tile);
		}
		break;

	case GFX_POWERPLANT_SPARKS:
		if (CHANCE16(1, 3)) {
			SndPlayTileFx(SND_0C_ELECTRIC_SPARK, tile);
			AddAnimatedTile(tile);
		}
		break;

	case GFX_COPPER_MINE_CHIMNEY:
		CreateEffectVehicleAbove(TileX(tile) * TILE_SIZE + 6, TileY(tile) * TILE_SIZE + 6, 43, EV_SMOKE);
		break;


	case GFX_TOY_FACTORY: {
			Industry *i = GetIndustryByTile(tile);
			if (i->was_cargo_delivered) {
				i->was_cargo_delivered = false;
				SetIndustryAnimationLoop(tile, 0);
				AddAnimatedTile(tile);
			}
		}
		break;

	case GFX_BUBBLE_GENERATOR:
		TileLoopIndustry_BubbleGenerator(tile);
		break;

	case GFX_TOFFEE_QUARY:
		AddAnimatedTile(tile);
		break;

	case GFX_SUGAR_MINE_SIEVE:
		if (CHANCE16(1, 3)) AddAnimatedTile(tile);
		break;
	}
}


static void ClickTile_Industry(TileIndex tile)
{
	ShowIndustryViewWindow(GetIndustryIndex(tile));
}

static uint32 GetTileTrackStatus_Industry(TileIndex tile, TransportType mode, uint sub_mode)
{
	return 0;
}

static void GetProducedCargo_Industry(TileIndex tile, CargoID *b)
{
	const IndustrySpec *i = GetIndustrySpec(GetIndustryByTile(tile)->type);

	b[0] = i->produced_cargo[0];
	b[1] = i->produced_cargo[1];
}

static void ChangeTileOwner_Industry(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	/* not used */
}

static const byte _plantfarmfield_type[] = {1, 1, 1, 1, 1, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6};

static bool IsBadFarmFieldTile(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_CLEAR: return IsClearGround(tile, CLEAR_FIELDS) || IsClearGround(tile, CLEAR_SNOW) || IsClearGround(tile, CLEAR_DESERT);
		case MP_TREES: return false;
		default:       return true;
	}
}

static bool IsBadFarmFieldTile2(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_CLEAR: return IsClearGround(tile, CLEAR_SNOW) || IsClearGround(tile, CLEAR_DESERT);
		case MP_TREES: return false;
		default:       return true;
	}
}

static void SetupFarmFieldFence(TileIndex tile, int size, byte type, Axis direction)
{
	do {
		tile = TILE_MASK(tile);

		if (IsTileType(tile, MP_CLEAR) || IsTileType(tile, MP_TREES)) {
			byte or_ = type;

			if (or_ == 1 && CHANCE16(1, 7)) or_ = 2;

			if (direction == AXIS_X) {
				SetFenceSE(tile, or_);
			} else {
				SetFenceSW(tile, or_);
			}
		}

		tile += (direction == AXIS_X ? TileDiffXY(1, 0) : TileDiffXY(0, 1));
	} while (--size);
}

static void PlantFarmField(TileIndex tile, IndustryID industry)
{
	uint size_x, size_y;
	uint32 r;
	uint count;
	uint counter;
	uint field_type;
	int type;

	if (_opt.landscape == LT_ARCTIC) {
		if (GetTileZ(tile) + TILE_HEIGHT * 2 >= GetSnowLine())
			return;
	}

	/* determine field size */
	r = (Random() & 0x303) + 0x404;
	if (_opt.landscape == LT_ARCTIC) r += 0x404;
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
			MakeField(cur_tile, field_type, industry);
			SetClearCounter(cur_tile, counter);
			MarkTileDirtyByTile(cur_tile);
		}
	END_TILE_LOOP(cur_tile, size_x, size_y, tile)

	type = 3;
	if (_opt.landscape != LT_ARCTIC && _opt.landscape != LT_TROPIC) {
		type = _plantfarmfield_type[Random() & 0xF];
	}

	SetupFarmFieldFence(tile - TileDiffXY(1, 0), size_y, type, AXIS_Y);
	SetupFarmFieldFence(tile - TileDiffXY(0, 1), size_x, type, AXIS_X);
	SetupFarmFieldFence(tile + TileDiffXY(size_x - 1, 0), size_y, type, AXIS_Y);
	SetupFarmFieldFence(tile + TileDiffXY(0, size_y - 1), size_x, type, AXIS_X);
}

void PlantRandomFarmField(const Industry *i)
{
	int x = i->width  / 2 + Random() % 31 - 16;
	int y = i->height / 2 + Random() % 31 - 16;

	TileIndex tile = TileAddWrap(i->xy, x, y);

	if (tile != INVALID_TILE) PlantFarmField(tile, i->index);
}

static void MaybePlantFarmField(const Industry *i)
{
	if (CHANCE16(1, 8)) PlantRandomFarmField(i);
}

/**
 * Search callback function for ChopLumberMillTrees
 * @param tile to test
 * @param data that is passed by the caller.  In this case, nothing
 * @return the result of the test
 */
static bool SearchLumberMillTrees(TileIndex tile, uint32 data)
{
	if (IsTileType(tile, MP_TREES) && GetTreeGrowth(tile) > 2) { ///< 3 and up means all fully grown trees
		PlayerID old_player = _current_player;
		/* found a tree */

		_current_player = OWNER_NONE;
		_industry_sound_ctr = 1;
		_industry_sound_tile = tile;
		SndPlayTileFx(SND_38_CHAINSAW, tile);

		DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);

		_current_player = old_player;
		return true;
	}
	return false;
}

/**
 * Perform a circular search around the Lumber Mill in order to find trees to cut
 * @param i industry
 */
static void ChopLumberMillTrees(Industry *i)
{
	TileIndex tile = i->xy;

	if (!IsIndustryCompleted(tile)) return;  ///< Can't proceed if not completed

	if (CircularTileSearch(tile, 40, SearchLumberMillTrees, 0)) ///< 40x40 tiles  to search
		i->cargo_waiting[0] = min(0xffff, i->cargo_waiting[0] + 45); ///< Found a tree, add according value to waiting cargo
}

static void ProduceIndustryGoods(Industry *i)
{
	uint32 r;
	uint num;
	const IndustrySpec *indsp = GetIndustrySpec(i->type);

	/* play a sound? */
	if ((i->counter & 0x3F) == 0) {
		if (CHANCE16R(1, 14, r) && (num = indsp->number_of_sounds) != 0) {
			SndPlayTileFx(
				(SoundFx)(indsp->random_sounds[((r >> 16) * num) >> 16]),
				i->xy);
		}
	}

	i->counter--;

	/* produce some cargo */
	if ((i->counter & 0xFF) == 0) {
		IndustyBehaviour indbehav = indsp->behaviour;
		i->cargo_waiting[0] = min(0xffff, i->cargo_waiting[0] + i->production_rate[0]);
		i->cargo_waiting[1] = min(0xffff, i->cargo_waiting[1] + i->production_rate[1]);

		if (indbehav & INDUSTRYBEH_PLANT_FIELDS) {
			MaybePlantFarmField(i);
		} else if ((indbehav & INDUSTRYBEH_CUT_TREES) && (i->counter & 0x1FF) == 0) {
			ChopLumberMillTrees(i);
		}
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

	if (_game_mode == GM_EDITOR) return;

	FOR_ALL_INDUSTRIES(i) {
		ProduceIndustryGoods(i);
	}
}


static bool CheckNewIndustry_NULL(TileIndex tile)
{
	return true;
}

static bool CheckNewIndustry_Forest(TileIndex tile)
{
	if (_opt.landscape == LT_ARCTIC) {
		if (GetTileZ(tile) < HighestSnowLine() + TILE_HEIGHT * 2U) {
			_error_message = STR_4831_FOREST_CAN_ONLY_BE_PLANTED;
			return false;
		}
	}
	return true;
}

static bool CheckNewIndustry_OilRefinery(TileIndex tile)
{
	if (_game_mode == GM_EDITOR) return true;
	if (DistanceFromEdge(TILE_ADDXY(tile, 1, 1)) < _patches.oil_refinery_limit) return true;

	_error_message = STR_483B_CAN_ONLY_BE_POSITIONED;
	return false;
}

extern bool _ignore_restrictions;

static bool CheckNewIndustry_OilRig(TileIndex tile)
{
	if (_game_mode == GM_EDITOR && _ignore_restrictions) return true;
	if (TileHeight(tile) == 0 &&
			DistanceFromEdge(TILE_ADDXY(tile, 1, 1)) < _patches.oil_refinery_limit) return true;

	_error_message = STR_483B_CAN_ONLY_BE_POSITIONED;
	return false;
}

static bool CheckNewIndustry_Farm(TileIndex tile)
{
	if (_opt.landscape == LT_ARCTIC) {
		if (GetTileZ(tile) + TILE_HEIGHT * 2 >= HighestSnowLine()) {
			_error_message = STR_0239_SITE_UNSUITABLE;
			return false;
		}
	}
	return true;
}

static bool CheckNewIndustry_Plantation(TileIndex tile)
{
	if (GetTropicZone(tile) == TROPICZONE_DESERT) {
		_error_message = STR_0239_SITE_UNSUITABLE;
		return false;
	}

	return true;
}

static bool CheckNewIndustry_Water(TileIndex tile)
{
	if (GetTropicZone(tile) != TROPICZONE_DESERT) {
		_error_message = STR_0318_CAN_ONLY_BE_BUILT_IN_DESERT;
		return false;
	}

	return true;
}

static bool CheckNewIndustry_Lumbermill(TileIndex tile)
{
	if (GetTropicZone(tile) != TROPICZONE_RAINFOREST) {
		_error_message = STR_0317_CAN_ONLY_BE_BUILT_IN_RAINFOREST;
		return false;
	}
	return true;
}

static bool CheckNewIndustry_BubbleGen(TileIndex tile)
{
	return GetTileZ(tile) <= TILE_HEIGHT * 4;
}

typedef bool CheckNewIndustryProc(TileIndex tile);
static CheckNewIndustryProc * const _check_new_industry_procs[CHECK_END] = {
	CheckNewIndustry_NULL,
	CheckNewIndustry_Forest,
	CheckNewIndustry_OilRefinery,
	CheckNewIndustry_Farm,
	CheckNewIndustry_Plantation,
	CheckNewIndustry_Water,
	CheckNewIndustry_Lumbermill,
	CheckNewIndustry_BubbleGen,
	CheckNewIndustry_OilRig
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

static const Town *CheckMultipleIndustryInTown(TileIndex tile, int type)
{
	const Town *t;
	const Industry *i;

	t = ClosestTownFromTile(tile, (uint)-1);

	if (_patches.multiple_industry_per_town) return t;

	FOR_ALL_INDUSTRIES(i) {
		if (i->type == (byte)type &&
				i->town == t) {
			_error_message = STR_0287_ONLY_ONE_ALLOWED_PER_TOWN;
			return NULL;
		}
	}

	return t;
}

static bool CheckIfIndustryTilesAreFree(TileIndex tile, const IndustryTileTable *it, int type)
{
	_error_message = STR_0239_SITE_UNSUITABLE;

	do {
		TileIndex cur_tile = tile + ToTileIndexDiff(it->ti);

		if (!IsValidTile(cur_tile)) {
			if (it->gfx == 0xff) continue;
			return false;
		}

		if (it->gfx == 0xFF) {
			if (!IsTileType(cur_tile, MP_WATER) ||
					GetTileSlope(cur_tile, NULL) != SLOPE_FLAT) {
				return false;
			}
		} else {
			if (!EnsureNoVehicle(cur_tile)) return false;
			IndustyBehaviour ind_behav = GetIndustrySpec(type)->behaviour;

			if (ind_behav & INDUSTRYBEH_BUILT_ONWATER) {
				/* As soon as the tile is not water, bail out.
				 * But that does not mean the search is over.  You have
				 * to make sure every tile of the industry will be only water*/
				if (!IsClearWaterTile(cur_tile)) return false;
			} else {
				Slope tileh;

				if (IsClearWaterTile(cur_tile)) return false;

				tileh = GetTileSlope(cur_tile, NULL);
				if (IsSteepSlope(tileh)) return false;

				if (_patches.land_generator != LG_TERRAGENESIS || !_generating_world) {
					/* It is almost impossible to have a fully flat land in TG, so what we
					 *  do is that we check if we can make the land flat later on. See
					 *  CheckIfCanLevelIndustryPlatform(). */
					if (tileh != SLOPE_FLAT) {
						Slope t;
						byte bits = GetIndustryTileSpec(it->gfx)->slopes_refused;

						if (bits & 0x10) return false;

						t = ComplementSlope(tileh);

						if (bits & 1 && (t & SLOPE_NW)) return false;
						if (bits & 2 && (t & SLOPE_NE)) return false;
						if (bits & 4 && (t & SLOPE_SW)) return false;
						if (bits & 8 && (t & SLOPE_SE)) return false;
					}
				}

				if (ind_behav & INDUSTRYBEH_ONLY_INTOWN) {
					if (!IsTileType(cur_tile, MP_HOUSE)) {
						_error_message = STR_029D_CAN_ONLY_BE_BUILT_IN_TOWNS;
						return false;
					}
				} else {
					if (ind_behav & INDUSTRYBEH_ONLY_NEARTOWN) {
						if (!IsTileType(cur_tile, MP_HOUSE)) goto do_clear;
					} else {
do_clear:
						if (CmdFailed(DoCommand(cur_tile, 0, 0, DC_AUTO, CMD_LANDSCAPE_CLEAR)))
							return false;
					}
				}
			}
		}
	} while ((++it)->ti.x != -0x80);

	return true;
}

static bool CheckIfIndustryIsAllowed(TileIndex tile, int type, const Town *t)
{
	if ((GetIndustrySpec(type)->behaviour & INDUSTRYBEH_TOWN1200_MORE) && t->population < 1200) {
		_error_message = STR_029D_CAN_ONLY_BE_BUILT_IN_TOWNS;
		return false;
	}

	if ((GetIndustrySpec(type)->behaviour & INDUSTRYBEH_ONLY_NEARTOWN) && DistanceMax(t->xy, tile) > 9) {
		_error_message = STR_0239_SITE_UNSUITABLE;
		return false;
	}

	return true;
}

static bool CheckCanTerraformSurroundingTiles(TileIndex tile, uint height, int internal)
{
	int size_x, size_y;
	uint curh;

	size_x = 2;
	size_y = 2;

	/* Check if we don't leave the map */
	if (TileX(tile) == 0 || TileY(tile) == 0 || GetTileType(tile) == MP_VOID) return false;

	tile += TileDiffXY(-1, -1);
	BEGIN_TILE_LOOP(tile_walk, size_x, size_y, tile) {
		curh = TileHeight(tile_walk);
		/* Is the tile clear? */
		if ((GetTileType(tile_walk) != MP_CLEAR) && (GetTileType(tile_walk) != MP_TREES))
			return false;

		/* Don't allow too big of a change if this is the sub-tile check */
		if (internal != 0 && delta(curh, height) > 1) return false;

		/* Different height, so the surrounding tiles of this tile
		 *  has to be correct too (in level, or almost in level)
		 *  else you get a chain-reaction of terraforming. */
		if (internal == 0 && curh != height) {
			if (!CheckCanTerraformSurroundingTiles(tile_walk + TileDiffXY(-1, -1), height, internal + 1))
				return false;
		}
	} END_TILE_LOOP(tile_walk, size_x, size_y, tile);

	return true;
}

/**
 * This function tries to flatten out the land below an industry, without
 *  damaging the surroundings too much.
 */
static bool CheckIfCanLevelIndustryPlatform(TileIndex tile, uint32 flags, const IndustryTileTable* it, int type)
{
	const int MKEND = -0x80;   // used for last element in an IndustryTileTable (see build_industry.h)
	int max_x = 0;
	int max_y = 0;
	TileIndex cur_tile;
	uint size_x, size_y;
	uint h, curh;

	/* Finds dimensions of largest variant of this industry */
	do {
		if (it->ti.x > max_x) max_x = it->ti.x;
		if (it->ti.y > max_y) max_y = it->ti.y;
	} while ((++it)->ti.x != MKEND);

	/* Remember level height */
	h = TileHeight(tile);

	/* Check that all tiles in area and surrounding are clear
	 * this determines that there are no obstructing items */
	cur_tile = tile + TileDiffXY(-1, -1);
	size_x = max_x + 4;
	size_y = max_y + 4;

	/* Check if we don't leave the map */
	if (TileX(cur_tile) == 0 || TileY(cur_tile) == 0 || TileX(cur_tile) + size_x >= MapMaxX() || TileY(cur_tile) + size_y >= MapMaxY()) return false;

	BEGIN_TILE_LOOP(tile_walk, size_x, size_y, cur_tile) {
		curh = TileHeight(tile_walk);
		if (curh != h) {
			/* This tile needs terraforming. Check if we can do that without
			 *  damaging the surroundings too much. */
			if (!CheckCanTerraformSurroundingTiles(tile_walk, h, 0)) return false;
			/* This is not 100% correct check, but the best we can do without modifying the map.
			 *  What is missing, is if the difference in height is more than 1.. */
			if (CmdFailed(DoCommand(tile_walk, 8, (curh > h) ? 0 : 1, flags & ~DC_EXEC, CMD_TERRAFORM_LAND))) return false;
		}
	} END_TILE_LOOP(tile_walk, size_x, size_y, cur_tile)

	if (flags & DC_EXEC) {
		/* Terraform the land under the industry */
		BEGIN_TILE_LOOP(tile_walk, size_x, size_y, cur_tile) {
			curh = TileHeight(tile_walk);
			while (curh != h) {
				/* We give the terraforming for free here, because we can't calculate
				 *  exact cost in the test-round, and as we all know, that will cause
				 *  a nice assert if they don't match ;) */
				DoCommand(tile_walk, 8, (curh > h) ? 0 : 1, flags, CMD_TERRAFORM_LAND);
				curh += (curh > h) ? -1 : 1;
			}
		} END_TILE_LOOP(tile_walk, size_x, size_y, cur_tile)
	}

	return true;
}


static bool CheckIfTooCloseToIndustry(TileIndex tile, int type)
{
	const IndustrySpec *indspec = GetIndustrySpec(type);
	const Industry *i;

	/* accepting industries won't be close, not even with patch */
	if (_patches.same_industry_close && indspec->accepts_cargo[0] == CT_INVALID)
		return true;

	FOR_ALL_INDUSTRIES(i) {
		/* check if an industry that accepts the same goods is nearby */
		if (DistanceMax(tile, i->xy) <= 14 &&
				indspec->accepts_cargo[0] != CT_INVALID &&
				indspec->accepts_cargo[0] == indspec->accepts_cargo[0] && (
					_game_mode != GM_EDITOR ||
					!_patches.same_industry_close ||
					!_patches.multiple_industry_per_town
				)) {
			_error_message = STR_INDUSTRY_TOO_CLOSE;
			return false;
		}

		/* check "not close to" field. */
		if ((i->type == indspec->conflicting[0] || i->type == indspec->conflicting[1] || i->type == indspec->conflicting[2]) &&
				DistanceMax(tile, i->xy) <= 14) {
			_error_message = STR_INDUSTRY_TOO_CLOSE;
			return false;
		}
	}
	return true;
}

static Industry *AllocateIndustry()
{
	Industry *i;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (i = GetIndustry(0); i != NULL; i = (i->index + 1U < GetIndustryPoolSize()) ? GetIndustry(i->index + 1U) : NULL) {
		IndustryID index = i->index;

		if (IsValidIndustry(i)) continue;

		memset(i, 0, sizeof(*i));
		i->index = index;

		return i;
	}

	/* Check if we can add a block to the pool */
	return AddBlockToPool(&_Industry_pool) ? AllocateIndustry() : NULL;
}

static void DoCreateNewIndustry(Industry *i, TileIndex tile, int type, const IndustryTileTable *it, const Town *t, Owner owner)
{
	const IndustrySpec *indspec = GetIndustrySpec(type);
	uint32 r;
	int j;

	i->xy = tile;
	i->width = i->height = 0;
	i->type = type;
	IncIndustryTypeCount(type);

	i->production_rate[0] = indspec->production_rate[0];
	i->production_rate[1] = indspec->production_rate[1];

	if (_patches.smooth_economy) {
		i->production_rate[0] = min((RandomRange(256) + 128) * i->production_rate[0] >> 8 , 255);
		i->production_rate[1] = min((RandomRange(256) + 128) * i->production_rate[1] >> 8 , 255);
	}

	i->town = t;
	i->owner = owner;

	r = Random();
	i->random_color = GB(r, 8, 4);
	i->counter = GB(r, 0, 12);
	i->cargo_waiting[0] = 0;
	i->cargo_waiting[1] = 0;
	i->this_month_production[0] = 0;
	i->this_month_production[1] = 0;
	i->this_month_transported[0] = 0;
	i->this_month_transported[1] = 0;
	i->last_month_pct_transported[0] = 0;
	i->last_month_pct_transported[1] = 0;
	i->last_month_transported[0] = 0;
	i->last_month_transported[1] = 0;
	i->was_cargo_delivered = false;
	i->last_prod_year = _cur_year;
	i->last_month_production[0] = i->production_rate[0] * 8;
	i->last_month_production[1] = i->production_rate[1] * 8;

	if (!_generating_world) i->last_month_production[0] = i->last_month_production[1] = 0;

	i->prod_level = 0x10;

	do {
		TileIndex cur_tile = tile + ToTileIndexDiff(it->ti);

		if (it->gfx != 0xFF) {
			byte size;

			size = it->ti.x;
			if (size > i->width) i->width = size;
			size = it->ti.y;
			if (size > i->height)i->height = size;

			DoCommand(cur_tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);

			MakeIndustry(cur_tile, i->index, it->gfx);
			if (_generating_world) {
				SetIndustryConstructionCounter(cur_tile, 3);
				SetIndustryConstructionStage(cur_tile, 2);
			}
		}
	} while ((++it)->ti.x != -0x80);

	i->width++;
	i->height++;

	if (GetIndustrySpec(i->type)->behaviour & INDUSTRYBEH_PLANT_ON_BUILT) {
		for (j = 0; j != 50; j++) PlantRandomFarmField(i);
	}
	_industry_sort_dirty = true;
	InvalidateWindow(WC_INDUSTRY_DIRECTORY, 0);
}

/** Helper function for Build/Fund an industry
 * @param tile tile where industry is built
 * @param type of industry to build
 * @param flags of operations to conduct
 * @param indspec pointer to industry specifications
 * @param it pointer to list of tile type to build
 * @return the pointer of the newly created industry, or NULL if it failed
 */
static Industry *CreateNewIndustryHelper(TileIndex tile, IndustryType type, uint32 flags, const IndustrySpec *indspec, const IndustryTileTable *it)
{
	const Town *t;
	Industry *i;

	if (!CheckIfIndustryTilesAreFree(tile, it, type)) return NULL;
	if (_patches.land_generator == LG_TERRAGENESIS && _generating_world && !CheckIfCanLevelIndustryPlatform(tile, 0, it, type)) return NULL;
	if (!_check_new_industry_procs[indspec->check_proc](tile)) return NULL;
	if (!CheckIfTooCloseToIndustry(tile, type)) return NULL;

	t = CheckMultipleIndustryInTown(tile, type);
	if (t == NULL) return NULL;

	if (!CheckIfIndustryIsAllowed(tile, type, t)) return NULL;
	if (!CheckSuitableIndustryPos(tile)) return NULL;

	i = AllocateIndustry();
	if (i == NULL) return NULL;

	if (flags & DC_EXEC) {
		CheckIfCanLevelIndustryPlatform(tile, DC_EXEC, it, type);
		DoCreateNewIndustry(i, tile, type, it, t, OWNER_NONE);
	}

	return i;
}

/** Build/Fund an industry
 * @param tile tile where industry is built
 * @param flags of operations to conduct
 * @param p1 industry type see build_industry.h and see industry.h
 * @param p2 unused
 * @return index of the newly create industry, or CMD_ERROR if it failed
 */
int32 CmdBuildIndustry(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int num;
	const IndustryTileTable * const *itt;
	const IndustryTileTable *it;
	const IndustrySpec *indspec;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	indspec = GetIndustrySpec(p1);

	/* Check if the to-be built/founded industry is available for this climate. */
	if (!indspec->enabled) {
		return CMD_ERROR;
	}

	/* If the patch for raw-material industries is not on, you cannot build raw-material industries.
	 * Raw material industries are industries that do not accept cargo (at least for now)
	 * Exclude the lumber mill (only "raw" industry that can be built) */
	if (!_patches.build_rawmaterial_ind &&
			indspec->accepts_cargo[0] == CT_INVALID &&
			indspec->accepts_cargo[1] == CT_INVALID &&
			indspec->accepts_cargo[2] == CT_INVALID &&
			!(indspec->behaviour & INDUSTRYBEH_CUT_TREES)) {
		return CMD_ERROR;
	}

	num = indspec->num_table;
	itt = indspec->table;

	do {
		if (--num < 0) return_cmd_error(STR_0239_SITE_UNSUITABLE);
	} while (!CheckIfIndustryTilesAreFree(tile, it = itt[num], p1));

	if (CreateNewIndustryHelper(tile, p1, flags, indspec, it) == NULL) return CMD_ERROR;

	return (_price.build_industry >> 8) * indspec->cost_multiplier;
}


Industry *CreateNewIndustry(TileIndex tile, IndustryType type)
{
	const IndustrySpec *indspec = GetIndustrySpec(type);
	const IndustryTileTable *it = indspec->table[RandomRange(indspec->num_table)];

	return CreateNewIndustryHelper(tile, type, DC_EXEC, indspec, it);
}

static const byte _numof_industry_table[5][12] = {
	/* difficulty settings for number of industries */
	{0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0},   //none
	{0, 1, 1, 1, 1, 1, 1, 1,  1,  1,  1},   //very low
	{0, 1, 1, 1, 2, 2, 3, 3,  4,  4,  5},   //low
	{0, 1, 2, 3, 4, 5, 6, 7,  8,  9, 10},   //normal
	{0, 2, 3, 4, 6, 7, 8, 9, 10, 10, 10},   //high
};

/** This function is the one who really do the creation work
 * of random industries during game creation
 * @param type IndustryType of the desired industry
 * @param amount of industries that need to be built */
static void PlaceInitialIndustry(IndustryType type, int amount)
{
	int num = _numof_industry_table[_opt.diff.number_industries][amount];
	const IndustrySpec *ind_spc = GetIndustrySpec(type);

	/* These are always placed next to the coastline, so we scale by the perimeter instead. */
	num = (ind_spc->check_proc == CHECK_REFINERY || ind_spc->check_proc == CHECK_OIL_RIG) ? ScaleByMapSize1D(num) : ScaleByMapSize(num);

	if (_opt.diff.number_industries != 0) {
		PlayerID old_player = _current_player;
		_current_player = OWNER_NONE;
		assert(num > 0);

		do {
			uint i;

			IncreaseGeneratingWorldProgress(GWP_INDUSTRY);

			for (i = 0; i < 2000; i++) {
				if (CreateNewIndustry(RandomTile(), type) != NULL) break;
			}
		} while (--num);

		_current_player = old_player;
	}
}

/** This function will create ramdon industries during game creation.
 * It will scale the amount of industries by map size as well as difficulty level */
void GenerateIndustries()
{
	uint i = 0;
	uint8 chance;
	IndustryType it;
	const IndustrySpec *ind_spc;

	/* Find the total amount of industries */
	for (it = IT_COAL_MINE; it < NUM_INDUSTRYTYPES; it++) {

		ind_spc = GetIndustrySpec(it);
		if (ind_spc->enabled) {
			chance = ind_spc->appear_creation[_opt.landscape];
			if (chance > 0) {
				/* once the chance of appearance is determind, it have to be scaled by
				 * the difficulty level. The "chance" in question is more an index into
				 * the _numof_industry_table,in fact */
				int num = _numof_industry_table[_opt.diff.number_industries][chance];

				/* These are always placed next to the coastline, so we scale by the perimeter instead. */
				num = (ind_spc->check_proc == CHECK_REFINERY || ind_spc->check_proc == CHECK_OIL_RIG) ? ScaleByMapSize1D(num) : ScaleByMapSize(num);
				i += num;
			}
		}
	}

	SetGeneratingWorldProgress(GWP_INDUSTRY, i);

	for (it = IT_COAL_MINE; it < NUM_INDUSTRYTYPES; it++) {
		/* Once the number of industries has been determined, let's really create them.
		 * The test for chance allows us to try create industries that are available only
		 * for this landscape.
		 * @todo :  Do we really have to pass chance as un-scaled value, since we've already
		 *          processed that scaling above? No, don't think so.  Will find a way. */
		ind_spc = GetIndustrySpec(it);
		if (ind_spc->enabled) {
			chance = ind_spc->appear_creation[_opt.landscape];
			if (chance > 0) PlaceInitialIndustry(it, chance);
		}
	};
}

/* Change industry production or do closure */
static void ExtChangeIndustryProduction(Industry *i)
{
	bool closeit = true;
	int j;
	const IndustrySpec *indspec = GetIndustrySpec(i->type);

	switch (indspec->life_type) {
		case INDUSTRYLIFE_NOT_CLOSABLE:
			return;

		case INDUSTRYLIFE_CLOSABLE:
			if ((byte)(_cur_year - i->last_prod_year) < 5 || !CHANCE16(1, 180))
				closeit = false;
			break;

		default: /* INDUSTRY_PRODUCTION */
			for (j = 0; j < 2 && indspec->produced_cargo[j] != CT_INVALID; j++){
				uint32 r = Random();
				int old_prod, new_prod, percent;
				int mag;

				new_prod = old_prod = i->production_rate[j];
				if (CHANCE16I(20, 1024, r))
					new_prod -= ((RandomRange(50) + 10) * old_prod) >> 8;
				if (CHANCE16I(20 + (i->last_month_pct_transported[j] * 20 >> 8), 1024, r >> 16))
					new_prod += ((RandomRange(50) + 10) * old_prod) >> 8;

				new_prod = clamp(new_prod, 0, 255);
				if (new_prod == old_prod) {
					closeit = false;
					continue;
				}

				percent = new_prod * 100 / old_prod - 100;
				i->production_rate[j] = new_prod;

				if (new_prod >= indspec->production_rate[j] / 4)
					closeit = false;

				mag = abs(percent);
				if (mag >= 10) {
					SetDParam(2, mag);
					SetDParam(0, GetCargo(indspec->produced_cargo[j])->name);
					SetDParam(1, i->index);
					AddNewsItem(
						percent >= 0 ? STR_INDUSTRY_PROD_GOUP : STR_INDUSTRY_PROD_GODOWN,
						NEWS_FLAGS(NM_THIN, NF_VIEWPORT | NF_TILE, NT_ECONOMY, 0),
						i->xy + TileDiffXY(1, 1), 0
					);
				}
			}
			break;
	}

	/* If industry will be closed down, show this */
	if (closeit) {
		i->prod_level = 0;
		SetDParam(0, i->index);
		AddNewsItem(
			indspec->closure_text,
			NEWS_FLAGS(NM_THIN, NF_VIEWPORT | NF_TILE, NT_OPENCLOSE, 0),
			i->xy + TileDiffXY(1, 1), 0
		);
	}
}


static void UpdateIndustryStatistics(Industry *i)
{
	byte pct;
	bool refresh = false;
	const IndustrySpec *indsp = GetIndustrySpec(i->type);

	for (byte j = 0; j < lengthof(indsp->produced_cargo); j++) {
		if (indsp->produced_cargo[j] != CT_INVALID) {
			pct = 0;
			if (i->this_month_production[j] != 0) {
				i->last_prod_year = _cur_year;
				pct = min(i->this_month_transported[j] * 256 / i->this_month_production[j], 255);
			}
			i->last_month_pct_transported[j] = pct;

			i->last_month_production[j] = i->this_month_production[j];
			i->this_month_production[j] = 0;

			i->last_month_transported[j] = i->this_month_transported[j];
			i->this_month_transported[j] = 0;
			refresh = true;
		}
	}

	if (refresh)
		InvalidateWindow(WC_INDUSTRY_VIEW, i->index);

	if (i->prod_level == 0) {
		DeleteIndustry(i);
	} else if (_patches.smooth_economy) {
		ExtChangeIndustryProduction(i);
	}
}

/** Simple helper that will collect data for the generation of industries */
struct ProbabilityHelper {
	uint16 prob;      ///< probability
	IndustryType ind; ///< industry id correcponding
};

/**
 * Try to create a random industry, during gameplay
 */
static void MaybeNewIndustry(void)
{
	Industry *ind;               //will receive the industry's creation pointer
	IndustryType rndtype, j;     // Loop controlers
	const IndustrySpec *ind_spc;
	uint num = 0;
	ProbabilityHelper cumulative_probs[NUM_INDUSTRYTYPES]; // probability collector
	uint16 probability_max = 0;

	/* Generate a list of all possible industries that can be built. */
	for (j = 0; j < NUM_INDUSTRYTYPES; j++) {
		byte chance = GetIndustrySpec(j)->appear_ingame[_opt.landscape];

		/* if appearing chance for this landscape is above 0, this industry can be chosen */
		if (chance != 0) {
			probability_max += chance;
			/* adds the result for this industry */
			cumulative_probs[num].ind = j;
			cumulative_probs[num++].prob = probability_max;
		}
	}

	/* Find a random type, with maximum being what has been evaluate above*/
	rndtype = RandomRange(probability_max);
	for (j = 0; j < NUM_INDUSTRYTYPES; j++) {
		/* and choose the index of the industry that matches as close as possible this random type */
		if (cumulative_probs[j].prob >= rndtype) break;
	}

	ind_spc = GetIndustrySpec(cumulative_probs[j].ind);
	/*  Check if it is allowed */
	if ((ind_spc->behaviour & INDUSTRYBEH_BEFORE_1950) && _cur_year > 1950) return;
	if ((ind_spc->behaviour & INDUSTRYBEH_AFTER_1960) && _cur_year < 1960) return;

	/* try to create 2000 times this industry */
	num = 2000;
	for (;;) {
		ind = CreateNewIndustry(RandomTile(), cumulative_probs[j].ind);
		if (ind != NULL) break;
		if (--num == 0) return;
	}

	SetDParam(0, ind_spc->name);
	SetDParam(1, ind->town->index);
	AddNewsItem(ind_spc->new_industry_text,
		NEWS_FLAGS(NM_THIN, NF_VIEWPORT | NF_TILE, NT_OPENCLOSE, 0), ind->xy, 0);
}

static void ChangeIndustryProduction(Industry *i)
{
	bool only_decrease = false;
	StringID str = STR_NULL;
	int type = i->type;
	const IndustrySpec *indspec = GetIndustrySpec(type);

	switch (indspec->life_type) {
		case INDUSTRYLIFE_NOT_CLOSABLE:
			return;

		case INDUSTRYLIFE_PRODUCTION:
			/* decrease or increase */
			if ((indspec->behaviour & INDUSTRYBEH_DONT_INCR_PROD) && _opt.landscape == LT_TEMPERATE)
				only_decrease = true;

			if (only_decrease || CHANCE16(1, 3)) {
				/* If you transport > 60%, 66% chance we increase, else 33% chance we increase */
				if (!only_decrease && (i->last_month_pct_transported[0] > 153) != CHANCE16(1, 3)) {
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

						str = indspec->production_up_text;
					}
				} else {
					/* Decrease production */
					if (i->prod_level == 4) {
						i->prod_level = 0;
						str = indspec->closure_text;
					} else {
						i->prod_level >>= 1;
						i->production_rate[0] = (i->production_rate[0] + 1) >> 1;
						i->production_rate[1] = (i->production_rate[1] + 1) >> 1;

						str = indspec->production_down_text;
					}
				}
			}
			break;

		case INDUSTRYLIFE_CLOSABLE:
			/* maybe close */
			if ( (byte)(_cur_year - i->last_prod_year) >= 5 && CHANCE16(1, 2)) {
				i->prod_level = 0;
				str = indspec->closure_text;
			}
			break;
	}

	if (str != STR_NULL) {
		SetDParam(0, i->index);
		AddNewsItem(str, NEWS_FLAGS(NM_THIN, NF_VIEWPORT | NF_TILE, str == indspec->closure_text ? NT_OPENCLOSE : NT_ECONOMY, 0), i->xy + TileDiffXY(1, 1), 0);
	}
}

void IndustryMonthlyLoop()
{
	Industry *i;
	PlayerID old_player = _current_player;
	_current_player = OWNER_NONE;

	FOR_ALL_INDUSTRIES(i) {
		UpdateIndustryStatistics(i);
	}

	/* 3% chance that we start a new industry */
	if (CHANCE16(3, 100)) {
		MaybeNewIndustry();
	} else if (!_patches.smooth_economy) {
		i = GetRandomIndustry();
		if (i != NULL) ChangeIndustryProduction(i);
	}

	_current_player = old_player;

	/* production-change */
	_industry_sort_dirty = true;
	InvalidateWindow(WC_INDUSTRY_DIRECTORY, 0);
}


void InitializeIndustries()
{
	CleanPool(&_Industry_pool);
	AddBlockToPool(&_Industry_pool);

	ResetIndustryCounts();
	_industry_sort_dirty = true;
	_industry_sound_tile = 0;
}

extern const TileTypeProcs _tile_type_industry_procs = {
	DrawTile_Industry,           /* draw_tile_proc */
	GetSlopeZ_Industry,          /* get_slope_z_proc */
	ClearTile_Industry,          /* clear_tile_proc */
	GetAcceptedCargo_Industry,   /* get_accepted_cargo_proc */
	GetTileDesc_Industry,        /* get_tile_desc_proc */
	GetTileTrackStatus_Industry, /* get_tile_track_status_proc */
	ClickTile_Industry,          /* click_tile_proc */
	AnimateTile_Industry,        /* animate_tile_proc */
	TileLoop_Industry,           /* tile_loop_proc */
	ChangeTileOwner_Industry,    /* change_tile_owner_proc */
	GetProducedCargo_Industry,   /* get_produced_cargo_proc */
	NULL,                        /* vehicle_enter_tile_proc */
	GetSlopeTileh_Industry,      /* get_slope_tileh_proc */
};

static const SaveLoad _industry_desc[] = {
	SLE_CONDVAR(Industry, xy,                         SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Industry, xy,                         SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(Industry, width,                      SLE_UINT8),
	    SLE_VAR(Industry, height,                     SLE_UINT8),
	    SLE_REF(Industry, town,                       REF_TOWN),
	SLE_CONDNULL( 2, 2, 60),       ///< used to be industry's produced_cargo
	    SLE_ARR(Industry, cargo_waiting,              SLE_UINT16, 2),
	    SLE_ARR(Industry, production_rate,            SLE_UINT8,  2),
	SLE_CONDNULL( 3, 2, 60),       ///< used to be industry's accepts_cargo
	    SLE_VAR(Industry, prod_level,                 SLE_UINT8),
	    SLE_ARR(Industry, this_month_production,      SLE_UINT16, 2),
	    SLE_ARR(Industry, this_month_transported,     SLE_UINT16, 2),
	    SLE_ARR(Industry, last_month_pct_transported, SLE_UINT8,  2),
	    SLE_ARR(Industry, last_month_production,      SLE_UINT16, 2),
	    SLE_ARR(Industry, last_month_transported,     SLE_UINT16, 2),

	    SLE_VAR(Industry, counter,                    SLE_UINT16),

	    SLE_VAR(Industry, type,                       SLE_UINT8),
	    SLE_VAR(Industry, owner,                      SLE_UINT8),
	    SLE_VAR(Industry, random_color,               SLE_UINT8),
	SLE_CONDVAR(Industry, last_prod_year,             SLE_FILE_U8 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Industry, last_prod_year,             SLE_INT32,                 31, SL_MAX_VERSION),
	    SLE_VAR(Industry, was_cargo_delivered,        SLE_UINT8),

	/* reserve extra space in savegame here. (currently 32 bytes) */
	SLE_CONDNULL(32, 2, SL_MAX_VERSION),

	SLE_END()
};

static void Save_INDY()
{
	Industry *ind;

	/* Write the vehicles */
	FOR_ALL_INDUSTRIES(ind) {
		SlSetArrayIndex(ind->index);
		SlObject(ind, _industry_desc);
	}
}

static void Load_INDY()
{
	int index;

	ResetIndustryCounts();

	while ((index = SlIterateArray()) != -1) {
		Industry *i;

		if (!AddBlockIfNeeded(&_Industry_pool, index))
			error("Industries: failed loading savegame: too many industries");

		i = GetIndustry(index);
		SlObject(i, _industry_desc);
		IncIndustryTypeCount(i->type);
	}
}

extern const ChunkHandler _industry_chunk_handlers[] = {
	{ 'INDY', Save_INDY, Load_INDY, CH_ARRAY | CH_LAST},
};
