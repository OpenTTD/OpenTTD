/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "clear.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "table/tree_land.h"
#include "functions.h"
#include "map.h"
#include "tile.h"
#include "tree.h"
#include "viewport.h"
#include "command.h"
#include "town.h"
#include "sound.h"
#include "variables.h"

static TreeType GetRandomTreeType(TileIndex tile, uint seed)
{
	switch (_opt.landscape) {
		case LT_NORMAL:
			return seed * TR_COUNT_TEMPERATE / 256 + TR_TEMPERATE;

		case LT_HILLY:
			return seed * TR_COUNT_SUB_ARCTIC / 256 + TR_SUB_ARCTIC;

		case LT_DESERT:
			switch (GetMapExtraBits(tile)) {
				case 0:  return seed * TR_COUNT_SUB_TROPICAL / 256 + TR_SUB_TROPICAL;
				case 1:  return (seed > 12) ? TR_INVALID : TR_CACTUS;
				default: return seed * TR_COUNT_RAINFOREST / 256 + TR_RAINFOREST;
			}

		default:
			return seed * TR_COUNT_TOYLAND / 256 + TR_TOYLAND;
	}
}

static void PlaceTree(TileIndex tile, uint32 r)
{
	TreeType tree = GetRandomTreeType(tile, GB(r, 24, 8));

	if (tree != TR_INVALID) {
		SetTileType(tile, MP_TREES);
		SetTreeType(tile, tree);
		SetFenceSE(tile, 0);
		SetFenceSW(tile, 0);
		SetTreeCount(tile, GB(r, 22, 2));
		SetTreeGrowth(tile, min(GB(r, 16, 3), 6));

		// above snowline?
		if (_opt.landscape == LT_HILLY && GetTileZ(tile) > _opt.snow_line) {
			SetTreeGroundDensity(tile, TR_SNOW_DESERT, 3);
			SetTreeCounter(tile, GB(r, 24, 3));
		} else {
			SetTreeGroundDensity(tile, GB(r, 28, 1), 0);
			SetTreeCounter(tile, GB(r, 24, 4));
		}
	}
}

static void DoPlaceMoreTrees(TileIndex tile)
{
	uint i;

	for (i = 0; i < 1000; i++) {
		uint32 r = Random();
		int x = GB(r, 0, 5) - 16;
		int y = GB(r, 8, 5) - 16;
		uint dist = myabs(x) + myabs(y);
		TileIndex cur_tile = TILE_MASK(tile + TileDiffXY(x, y));

		if (dist <= 13 &&
				IsTileType(cur_tile, MP_CLEAR) &&
				!IsClearGround(cur_tile, CL_FIELDS) &&
				!IsClearGround(cur_tile, CL_ROCKS)) {
			PlaceTree(cur_tile, r);
		}
	}
}

static void PlaceMoreTrees(void)
{
	uint i = ScaleByMapSize(GB(Random(), 0, 5) + 25);
	do {
		DoPlaceMoreTrees(RandomTile());
	} while (--i);
}

void PlaceTreesRandomly(void)
{
	uint i;

	i = ScaleByMapSize(1000);
	do {
		uint32 r = Random();
		TileIndex tile = RandomTileSeed(r);
		if (IsTileType(tile, MP_CLEAR) &&
				!IsClearGround(tile, CL_FIELDS) &&
				!IsClearGround(tile, CL_ROCKS)) {
			PlaceTree(tile, r);
		}
	} while (--i);

	/* place extra trees at rainforest area */
	if (_opt.landscape == LT_DESERT) {
		i = ScaleByMapSize(15000);

		do {
			uint32 r = Random();
			TileIndex tile = RandomTileSeed(r);
			if (IsTileType(tile, MP_CLEAR) && GetMapExtraBits(tile) == 2) {
				PlaceTree(tile, r);
			}
		} while (--i);
	}
}

void GenerateTrees(void)
{
	uint i;

	if (_opt.landscape != LT_CANDY) PlaceMoreTrees();

	for (i = _opt.landscape == LT_HILLY ? 15 : 6; i != 0; i--) {
		PlaceTreesRandomly();
	}
}

/** Plant a tree.
 * @param x,y start tile of area-drag of tree plantation
 * @param p1 tree type, -1 means random.
 * @param p2 end tile of area-drag
 */
int32 CmdPlantTree(int ex, int ey, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost;
	int sx, sy, x, y;

	if (p2 >= MapSize()) return CMD_ERROR;
	/* Check the tree type. It can be random or some valid value within the current climate */
	if (p1 != (uint)-1 && p1 - _tree_base_by_landscape[_opt.landscape] >= _tree_count_by_landscape[_opt.landscape]) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	// make sure sx,sy are smaller than ex,ey
	sx = TileX(p2);
	sy = TileY(p2);
	ex /= 16; ey /= 16;
	if (ex < sx) intswap(ex, sx);
	if (ey < sy) intswap(ey, sy);

	cost = 0; // total cost

	for (x = sx; x <= ex; x++) {
		for (y = sy; y <= ey; y++) {
			TileIndex tile = TileXY(x, y);

			if (!EnsureNoVehicle(tile)) continue;

			switch (GetTileType(tile)) {
				case MP_TREES:
					// no more space for trees?
					if (_game_mode != GM_EDITOR && GetTreeCount(tile) == 3) {
						_error_message = STR_2803_TREE_ALREADY_HERE;
						continue;
					}

					if (flags & DC_EXEC) {
						AddTreeCount(tile, 1);
						MarkTileDirtyByTile(tile);
					}
					// 2x as expensive to add more trees to an existing tile
					cost += _price.build_trees * 2;
					break;

				case MP_CLEAR:
					if (!IsTileOwner(tile, OWNER_NONE)) {
						_error_message = STR_2804_SITE_UNSUITABLE;
						continue;
					}

					switch (GetClearGround(tile)) {
						case CL_FIELDS: cost += _price.clear_3; break;
						case CL_ROCKS:  cost += _price.clear_2; break;
						default: break;
					}

					if (flags & DC_EXEC) {
						TreeType treetype;

						if (_game_mode != GM_EDITOR && _current_player < MAX_PLAYERS) {
							Town *t = ClosestTownFromTile(tile, _patches.dist_local_authority);
							if (t != NULL)
								ChangeTownRating(t, RATING_TREE_UP_STEP, RATING_TREE_MAXIMUM);
						}

						treetype = p1;
						if (treetype == TR_INVALID) {
							treetype = GetRandomTreeType(tile, GB(Random(), 24, 8));
							if (treetype == TR_INVALID) treetype = TR_CACTUS;
						}

						switch (GetClearGround(tile)) {
							case CL_ROUGH: SetTreeGroundDensity(tile, TR_ROUGH, 0); break;
							case CL_SNOW:  SetTreeGroundDensity(tile, TR_SNOW_DESERT, GetClearDensity(tile)); break;
							default:       SetTreeGroundDensity(tile, TR_GRASS, 0); break;
						}
						SetTreeCounter(tile, 0);

						SetTileType(tile, MP_TREES);
						SetTreeType(tile, treetype);
						SetFenceSE(tile, 0);
						SetFenceSW(tile, 0);
						SetTreeCount(tile, 0);
						SetTreeGrowth(tile, _game_mode == GM_EDITOR ? 3 : 0);
						MarkTileDirtyByTile(tile);

						if (_game_mode == GM_EDITOR && IS_INT_INSIDE(treetype, TR_RAINFOREST, TR_CACTUS))
							SetMapExtraBits(tile, 2);
					}
					cost += _price.build_trees;
					break;

				default:
					_error_message = STR_2804_SITE_UNSUITABLE;
					break;
			}
		}
	}

	return (cost == 0) ? CMD_ERROR : cost;
}

typedef struct TreeListEnt {
	uint32 image;
	byte x,y;
} TreeListEnt;

static void DrawTile_Trees(TileInfo *ti)
{
	const uint32 *s;
	const TreePos* d;
	byte z;

	switch (GetTreeGround(ti->tile)) {
		case TR_GRASS: DrawClearLandTile(ti, 3); break;
		case TR_ROUGH: DrawHillyLandTile(ti); break;
		default:       DrawGroundSprite(_tree_sprites_1[GetTreeDensity(ti->tile)] + _tileh_to_sprite[ti->tileh]); break;
	}

	DrawClearLandFence(ti);

	z = ti->z;
	if (ti->tileh != 0) {
		z += 4;
		if (IsSteepTileh(ti->tileh))
			z += 4;
	}

	{
		uint16 tmp = ti->x;
		uint index;

		tmp = ROR(tmp, 2);
		tmp -= ti->y;
		tmp = ROR(tmp, 3);
		tmp -= ti->x;
		tmp = ROR(tmp, 1);
		tmp += ti->y;

		d = _tree_layout_xy[GB(tmp, 4, 2)];

		index = GB(tmp, 6, 2) + (GetTreeType(ti->tile) << 2);

		/* different tree styles above one of the grounds */
		if (GetTreeGround(ti->tile) == TR_SNOW_DESERT &&
				GetTreeDensity(ti->tile) >= 2 &&
				IS_INT_INSIDE(index, TR_SUB_ARCTIC << 2, TR_RAINFOREST << 2)) {
			index += 164 - (TR_SUB_ARCTIC << 2);
		}

		assert(index < lengthof(_tree_layout_sprite));
		s = _tree_layout_sprite[index];
	}

	StartSpriteCombine();

	if (!(_display_opt & DO_TRANS_BUILDINGS) || !_patches.invisible_trees) {
		TreeListEnt te[4];
		uint i;

		/* put the trees to draw in a list */
		i = (ti->map5 >> 6) + 1;
		do {
			uint32 image = s[0] + (--i == 0 ? GB(ti->map5, 0, 3) : 3);
			if (_display_opt & DO_TRANS_BUILDINGS) MAKE_TRANSPARENT(image);
			te[i].image = image;
			te[i].x = d->x;
			te[i].y = d->y;
			s++;
			d++;
		} while (i);

		/* draw them in a sorted way */
		for (;;) {
			byte min = 0xFF;
			TreeListEnt *tep = NULL;

			i = (ti->map5 >> 6) + 1;
			do {
				if (te[--i].image != 0 && te[i].x + te[i].y < min) {
					min = te[i].x + te[i].y;
					tep = &te[i];
				}
			} while (i);

			if (tep == NULL) break;

			AddSortableSpriteToDraw(tep->image, ti->x + tep->x, ti->y + tep->y, 5, 5, 0x10, z);
			tep->image = 0;
		}
	}

	EndSpriteCombine();
}


static uint GetSlopeZ_Trees(const TileInfo* ti)
{
	return GetPartialZ(ti->x & 0xF, ti->y & 0xF, ti->tileh) + ti->z;
}

static uint GetSlopeTileh_Trees(const TileInfo* ti)
{
	return ti->tileh;
}

static int32 ClearTile_Trees(TileIndex tile, byte flags)
{
	uint num;

	if (flags & DC_EXEC && _current_player < MAX_PLAYERS) {
		Town *t = ClosestTownFromTile(tile, _patches.dist_local_authority);
		if (t != NULL)
			ChangeTownRating(t, RATING_TREE_DOWN_STEP, RATING_TREE_MINIMUM);
	}

	num = GetTreeCount(tile) + 1;
	if (IS_INT_INSIDE(GetTreeType(tile), TR_RAINFOREST, TR_CACTUS)) num *= 4;

	if (flags & DC_EXEC) DoClearSquare(tile);

	return num * _price.remove_trees;
}

static void GetAcceptedCargo_Trees(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void GetTileDesc_Trees(TileIndex tile, TileDesc *td)
{
	TreeType tt = GetTreeType(tile);

	if (IS_INT_INSIDE(tt, TR_RAINFOREST, TR_CACTUS)) {
		td->str = STR_280F_RAINFOREST;
	} else if (tt == TR_CACTUS) {
		td->str = STR_2810_CACTUS_PLANTS;
	} else {
		td->str = STR_280E_TREES;
	}

	td->owner = GetTileOwner(tile);
}

static void AnimateTile_Trees(TileIndex tile)
{
	/* not used */
}

static void TileLoopTreesDesert(TileIndex tile)
{
	switch (GetMapExtraBits(tile)) {
		case 1:
			if (GetTreeGround(tile) != TR_SNOW_DESERT) {
				SetTreeGroundDensity(tile, TR_SNOW_DESERT, 3);
				MarkTileDirtyByTile(tile);
			}
			break;

		case 2: {
			static const SoundFx forest_sounds[] = {
				SND_42_LOON_BIRD,
				SND_43_LION,
				SND_44_MONKEYS,
				SND_48_DISTANT_BIRD
			};
			uint32 r = Random();

			if (CHANCE16I(1, 200, r)) SndPlayTileFx(forest_sounds[GB(r, 16, 2)], tile);
			break;
		}
	}
}

static void TileLoopTreesAlps(TileIndex tile)
{
	int k = GetTileZ(tile) - _opt.snow_line;

	if (k < -8) {
		if (GetTreeGround(tile) != TR_SNOW_DESERT) return;
		SetTreeGroundDensity(tile, TR_GRASS, 0);
	} else {
		uint density = min((uint)(k + 8) / 8, 3);

		if (GetTreeGround(tile) != TR_SNOW_DESERT || GetTreeDensity(tile) != density) {
			SetTreeGroundDensity(tile, TR_SNOW_DESERT, density);
		} else {
			if (GetTreeDensity(tile) == 3) {
				uint32 r = Random();
				if (CHANCE16I(1, 200, r)) {
					SndPlayTileFx((r & 0x80000000) ? SND_39_HEAVY_WIND : SND_34_WIND, tile);
				}
			}
			return;
		}
	}
	MarkTileDirtyByTile(tile);
}

static void TileLoop_Trees(TileIndex tile)
{
	static const TileIndexDiffC _tileloop_trees_dir[] = {
		{-1, -1},
		{ 0, -1},
		{ 1, -1},
		{-1,  0},
		{ 1,  0},
		{-1,  1},
		{ 0,  1},
		{ 1,  1}
	};

	switch (_opt.landscape) {
		case LT_DESERT: TileLoopTreesDesert(tile); break;
		case LT_HILLY:  TileLoopTreesAlps(tile);   break;
	}

	TileLoopClearHelper(tile);

	if (GetTreeCounter(tile) < 15) {
		AddTreeCounter(tile, 1);
		return;
	}
	SetTreeCounter(tile, 0);

	switch (GetTreeGrowth(tile)) {
		case 3: /* regular sized tree */
			if (_opt.landscape == LT_DESERT && GetTreeType(tile) != TR_CACTUS && GetMapExtraBits(tile) == 1) {
				AddTreeGrowth(tile, 1);
			} else {
				switch (GB(Random(), 0, 3)) {
					case 0: /* start destructing */
						AddTreeGrowth(tile, 1);
						break;

					case 1: /* add a tree */
						if (GetTreeCount(tile) < 3) {
							AddTreeCount(tile, 1);
							SetTreeGrowth(tile, 0);
							break;
						}
						/* FALL THROUGH */

					case 2: { /* add a neighbouring tree */
						TreeType treetype = GetTreeType(tile);

						tile += ToTileIndexDiff(_tileloop_trees_dir[Random() & 7]);

						if (!IsTileType(tile, MP_CLEAR)) return;

						switch (GetClearGround(tile)) {
							case CL_GRASS:
								if (GetClearDensity(tile) != 3) return;
								SetTreeGroundDensity(tile, TR_GRASS, 0);
								break;

							case CL_ROUGH: SetTreeGroundDensity(tile, TR_ROUGH, 0); break;
							case CL_SNOW:  SetTreeGroundDensity(tile, TR_SNOW_DESERT, GetClearDensity(tile)); break;
							default: return;
						}
						SetTreeCounter(tile, 0);

						SetTileType(tile, MP_TREES);
						SetTreeType(tile, treetype);
						SetFenceSE(tile, 0);
						SetFenceSW(tile, 0);
						SetTreeCount(tile, 0);
						SetTreeGrowth(tile, 0);
						break;
					}

					default:
						return;
				}
			}
			break;

		case 6: /* final stage of tree destruction */
			if (GetTreeCount(tile) > 0) {
				/* more than one tree, delete it */
				AddTreeCount(tile, -1);
				SetTreeGrowth(tile, 3);
			} else {
				/* just one tree, change type into MP_CLEAR */
				switch (GetTreeGround(tile)) {
					case TR_GRASS: MakeClear(tile, CL_GRASS, 3); break;
					case TR_ROUGH: MakeClear(tile, CL_ROUGH, 3); break;
					default:       MakeClear(tile, CL_SNOW, GetTreeDensity(tile)); break;
				}
			}
			break;

		default:
			AddTreeGrowth(tile, 1);
			break;
	}

	MarkTileDirtyByTile(tile);
}

void OnTick_Trees(void)
{
	uint32 r;
	TileIndex tile;
	ClearGround ct;
	TreeType tree;

	/* place a tree at a random rainforest spot */
	if (_opt.landscape == LT_DESERT &&
			(r = Random(), tile = RandomTileSeed(r), GetMapExtraBits(tile) == 2) &&
			IsTileType(tile, MP_CLEAR) &&
			(ct = GetClearGround(tile), ct == CL_GRASS || ct == CL_ROUGH) &&
			(tree = GetRandomTreeType(tile, GB(r, 24, 8))) != TR_INVALID) {
		SetTileType(tile, MP_TREES);
		SetTreeGroundDensity(tile, ct == CL_ROUGH ? TR_ROUGH : TR_GRASS, 0);
		SetTreeCounter(tile, 0);
		SetTreeType(tile, tree);
		SetTreeCount(tile, 0);
		SetTreeGrowth(tile, 0);
	}

	// byte underflow
	if (--_trees_tick_ctr != 0) return;

	/* place a tree at a random spot */
	r = Random();
	tile = TILE_MASK(r);
	if (IsTileType(tile, MP_CLEAR) &&
			(ct = GetClearGround(tile), ct == CL_GRASS || ct == CL_ROUGH || ct == CL_SNOW) &&
			(tree = GetRandomTreeType(tile, GB(r, 24, 8))) != TR_INVALID) {
		switch (ct) {
			case CL_GRASS: SetTreeGroundDensity(tile, TR_GRASS, 0); break;
			case CL_ROUGH: SetTreeGroundDensity(tile, TR_ROUGH, 0); break;
			default:       SetTreeGroundDensity(tile, TR_SNOW_DESERT, GetClearDensity(tile)); break;
		}
		SetTreeCounter(tile, 0);
		SetTileType(tile, MP_TREES);
		SetTreeType(tile, tree);
		SetTreeCount(tile, 0);
		SetTreeGrowth(tile, 0);
	}
}

static void ClickTile_Trees(TileIndex tile)
{
	/* not used */
}

static uint32 GetTileTrackStatus_Trees(TileIndex tile, TransportType mode)
{
	return 0;
}

static void ChangeTileOwner_Trees(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	/* not used */
}

void InitializeTrees(void)
{
	_trees_tick_ctr = 0;
}


const TileTypeProcs _tile_type_trees_procs = {
	DrawTile_Trees,						/* draw_tile_proc */
	GetSlopeZ_Trees,					/* get_slope_z_proc */
	ClearTile_Trees,					/* clear_tile_proc */
	GetAcceptedCargo_Trees,		/* get_accepted_cargo_proc */
	GetTileDesc_Trees,				/* get_tile_desc_proc */
	GetTileTrackStatus_Trees,	/* get_tile_track_status_proc */
	ClickTile_Trees,					/* click_tile_proc */
	AnimateTile_Trees,				/* animate_tile_proc */
	TileLoop_Trees,						/* tile_loop_clear */
	ChangeTileOwner_Trees,		/* change_tile_owner_clear */
	NULL,											/* get_produced_cargo_proc */
	NULL,											/* vehicle_enter_tile_proc */
	NULL,											/* vehicle_leave_tile_proc */
	GetSlopeTileh_Trees,			/* get_slope_tileh_proc */
};
