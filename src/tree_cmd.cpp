/* $Id$ */

/** @file tree_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "clear_map.h"
#include "tile_cmd.h"
#include "landscape.h"
#include "tree_map.h"
#include "viewport_func.h"
#include "command_func.h"
#include "economy_func.h"
#include "town.h"
#include "variables.h"
#include "genworld.h"
#include "transparency.h"
#include "functions.h"
#include "player_func.h"
#include "sound_func.h"
#include "settings_type.h"
#include "water_map.h"
#include "water.h"

#include "table/strings.h"
#include "table/sprites.h"
#include "table/tree_land.h"

/**
 * List of tree placer algorithm.
 *
 * This enumeration defines all possible tree placer algorithm in the game.
 */
enum TreePlacer {
	TP_NONE,     ///< No tree placer algorithm
	TP_ORIGINAL, ///< The original algorithm
	TP_IMPROVED, ///< A 'improved' algorithm
};

/**
 * Tests if a tile can be converted to MP_TREES
 * This is true for clear ground without farms or rocks.
 *
 * @param tile the tile of interest
 * @param allow_desert Allow planting trees on CLEAR_DESERT?
 * @return true if trees can be built.
 */
static bool CanPlantTreesOnTile(TileIndex tile, bool allow_desert)
{
	switch (GetTileType(tile)) {
		case MP_WATER:
			return !IsBridgeAbove(tile) && IsCoast(tile) && !IsSlopeWithOneCornerRaised(GetTileSlope(tile, NULL));

		case MP_CLEAR:
			return !IsBridgeAbove(tile) && !IsClearGround(tile, CLEAR_FIELDS) && !IsClearGround(tile, CLEAR_ROCKS) &&
			       (allow_desert || !IsClearGround(tile, CLEAR_DESERT));

		default: return false;
	}
}

/**
 * Creates a tree tile
 * Ground type and density is preserved.
 *
 * @pre the tile must be suitable for trees.
 *
 * @param tile where to plant the trees.
 * @param type The type of the tree
 * @param count the number of trees (minus 1)
 * @param growth the growth status
 */
static void PlantTreesOnTile(TileIndex tile, TreeType treetype, uint count, uint growth)
{
	assert(treetype != TREE_INVALID);
	assert(CanPlantTreesOnTile(tile, true));

	TreeGround ground;
	uint density = 3;

	switch (GetTileType(tile)) {
		case MP_WATER:
			ground = TREE_GROUND_SHORE;
			break;

		case MP_CLEAR:
			switch (GetClearGround(tile)) {
				case CLEAR_GRASS:  ground = TREE_GROUND_GRASS;       density = GetClearDensity(tile); break;
				case CLEAR_ROUGH:  ground = TREE_GROUND_ROUGH;                                        break;
				default:           ground = TREE_GROUND_SNOW_DESERT; density = GetClearDensity(tile); break;
			}
			break;

		default: NOT_REACHED();
	}

	MakeTree(tile, treetype, count, growth, ground, density);
}

/**
 * Get a random TreeType for the given tile based on a given seed
 *
 * This function returns a random TreeType which can be placed on the given tile.
 * The seed for randomness must be less or equal 256, use #GB on the value of Random()
 * to get such a value.
 *
 * @param tile The tile to get a random TreeType from
 * @param seed The seed for randomness, must be less or equal 256
 * @return The random tree type
 */
static TreeType GetRandomTreeType(TileIndex tile, uint seed)
{
	switch (_opt.landscape) {
		case LT_TEMPERATE:
			return (TreeType)(seed * TREE_COUNT_TEMPERATE / 256 + TREE_TEMPERATE);

		case LT_ARCTIC:
			return (TreeType)(seed * TREE_COUNT_SUB_ARCTIC / 256 + TREE_SUB_ARCTIC);

		case LT_TROPIC:
			switch (GetTropicZone(tile)) {
				case TROPICZONE_NORMAL:  return (TreeType)(seed * TREE_COUNT_SUB_TROPICAL / 256 + TREE_SUB_TROPICAL);
				case TROPICZONE_DESERT:  return (TreeType)((seed > 12) ? TREE_INVALID : TREE_CACTUS);
				default:                 return (TreeType)(seed * TREE_COUNT_RAINFOREST / 256 + TREE_RAINFOREST);
			}

		default:
			return (TreeType)(seed * TREE_COUNT_TOYLAND / 256 + TREE_TOYLAND);
	}
}

/**
 * Make a random tree tile of the given tile
 *
 * Create a new tree-tile for the given tile. The second parameter is used for
 * randomness like type and number of trees.
 *
 * @param tile The tile to make a tree-tile from
 * @param r The randomness value from a Random() value
 */
static void PlaceTree(TileIndex tile, uint32 r)
{
	TreeType tree = GetRandomTreeType(tile, GB(r, 24, 8));

	if (tree != TREE_INVALID) {
		PlantTreesOnTile(tile, tree, GB(r, 22, 2), min(GB(r, 16, 3), 6));

		/* Rerandomize ground, if neither snow nor shore */
		TreeGround ground = GetTreeGround(tile);
		if (ground != TREE_GROUND_SNOW_DESERT && ground != TREE_GROUND_SHORE) {
			SetTreeGroundDensity(tile, (TreeGround)GB(r, 28, 1), 3);
		}

		/* Set the counter to a random start value */
		SetTreeCounter(tile, (TreeGround)GB(r, 24, 4));
	}
}

/**
 * Place some amount of trees around a given tile.
 *
 * This function adds some trees around a given tile. As this function use
 * the Random() call it depends on the random how many trees are actually placed
 * around the given tile.
 *
 * @param tile The center of the trees to add
 */
static void DoPlaceMoreTrees(TileIndex tile)
{
	uint i;

	for (i = 0; i < 1000; i++) {
		uint32 r = Random();
		int x = GB(r, 0, 5) - 16;
		int y = GB(r, 8, 5) - 16;
		uint dist = abs(x) + abs(y);
		TileIndex cur_tile = TILE_MASK(tile + TileDiffXY(x, y));

		if (dist <= 13 && CanPlantTreesOnTile(cur_tile, true)) {
			PlaceTree(cur_tile, r);
		}
	}
}

/**
 * Place more trees on the map.
 *
 * This function add more trees to the map.
 */
static void PlaceMoreTrees()
{
	uint i = ScaleByMapSize(GB(Random(), 0, 5) + 25);
	do {
		DoPlaceMoreTrees(RandomTile());
	} while (--i);
}

/**
 * Place a tree at the same height as an existing tree.
 *
 * Add a new tree around the given tile which is at the same
 * height or at some offset (2 units) of it.
 *
 * @param tile The base tile to add a new tree somewhere around
 * @param height The height (like the one from the tile)
 */
static void PlaceTreeAtSameHeight(TileIndex tile, uint height)
{
	uint i;

	for (i = 0; i < 1000; i++) {
		uint32 r = Random();
		int x = GB(r, 0, 5) - 16;
		int y = GB(r, 8, 5) - 16;
		TileIndex cur_tile = TILE_MASK(tile + TileDiffXY(x, y));

		/* Keep in range of the existing tree */
		if (abs(x) + abs(y) > 16) continue;

		/* Clear tile, no farm-tiles or rocks */
		if (!CanPlantTreesOnTile(cur_tile, true)) continue;

		/* Not too much height difference */
		if (Delta(GetTileZ(cur_tile), height) > 2) continue;

		/* Place one tree and quit */
		PlaceTree(cur_tile, r);
		break;
	}
}

/**
 * Place some trees randomly
 *
 * This function just place some trees randomly on the map.
 */
void PlaceTreesRandomly()
{
	uint i, j, ht;

	i = ScaleByMapSize(1000);
	do {
		uint32 r = Random();
		TileIndex tile = RandomTileSeed(r);

		IncreaseGeneratingWorldProgress(GWP_TREE);

		if (CanPlantTreesOnTile(tile, true)) {
			PlaceTree(tile, r);
			if (_patches.tree_placer != TP_IMPROVED) continue;

			/* Place a number of trees based on the tile height.
			 *  This gives a cool effect of multiple trees close together.
			 *  It is almost real life ;) */
			ht = GetTileZ(tile);
			/* The higher we get, the more trees we plant */
			j = GetTileZ(tile) / TILE_HEIGHT * 2;
			while (j--) {
				/* Above snowline more trees! */
				if (_opt.landscape == LT_ARCTIC && ht > GetSnowLine()) {
					PlaceTreeAtSameHeight(tile, ht);
					PlaceTreeAtSameHeight(tile, ht);
				};

				PlaceTreeAtSameHeight(tile, ht);
			}
		}
	} while (--i);

	/* place extra trees at rainforest area */
	if (_opt.landscape == LT_TROPIC) {
		i = ScaleByMapSize(15000);

		do {
			uint32 r = Random();
			TileIndex tile = RandomTileSeed(r);

			IncreaseGeneratingWorldProgress(GWP_TREE);

			if (GetTropicZone(tile) == TROPICZONE_RAINFOREST && CanPlantTreesOnTile(tile, false)) {
				PlaceTree(tile, r);
			}
		} while (--i);
	}
}

/**
 * Place new trees.
 *
 * This function takes care of the selected tree placer algorithm and
 * place randomly the trees for a new game.
 */
void GenerateTrees()
{
	uint i, total;

	if (_patches.tree_placer == TP_NONE) return;

	if (_opt.landscape != LT_TOYLAND) PlaceMoreTrees();

	switch (_patches.tree_placer) {
		case TP_ORIGINAL: i = _opt.landscape == LT_ARCTIC ? 15 : 6; break;
		case TP_IMPROVED: i = _opt.landscape == LT_ARCTIC ?  4 : 2; break;
		default: NOT_REACHED(); return;
	}

	total = ScaleByMapSize(1000);
	if (_opt.landscape == LT_TROPIC) total += ScaleByMapSize(15000);
	total *= i;
	SetGeneratingWorldProgress(GWP_TREE, total);

	for (; i != 0; i--) {
		PlaceTreesRandomly();
	}
}

/** Plant a tree.
 * @param tile start tile of area-drag of tree plantation
 * @param flags type of operation
 * @param p1 tree type, -1 means random.
 * @param p2 end tile of area-drag
 */
CommandCost CmdPlantTree(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	StringID msg = INVALID_STRING_ID;
	CommandCost cost(EXPENSES_OTHER);
	int ex;
	int ey;
	int sx, sy, x, y;

	if (p2 >= MapSize()) return CMD_ERROR;
	/* Check the tree type. It can be random or some valid value within the current climate */
	if (p1 != (uint)-1 && p1 - _tree_base_by_landscape[_opt.landscape] >= _tree_count_by_landscape[_opt.landscape]) return CMD_ERROR;

	// make sure sx,sy are smaller than ex,ey
	ex = TileX(tile);
	ey = TileY(tile);
	sx = TileX(p2);
	sy = TileY(p2);
	if (ex < sx) Swap(ex, sx);
	if (ey < sy) Swap(ey, sy);

	for (x = sx; x <= ex; x++) {
		for (y = sy; y <= ey; y++) {
			TileIndex tile = TileXY(x, y);

			switch (GetTileType(tile)) {
				case MP_TREES:
					/* no more space for trees? */
					if (_game_mode != GM_EDITOR && GetTreeCount(tile) == 3) {
						msg = STR_2803_TREE_ALREADY_HERE;
						continue;
					}

					if (flags & DC_EXEC) {
						AddTreeCount(tile, 1);
						MarkTileDirtyByTile(tile);
					}
					/* 2x as expensive to add more trees to an existing tile */
					cost.AddCost(_price.build_trees * 2);
					break;

				case MP_WATER:
					if (!IsCoast(tile) || IsSlopeWithOneCornerRaised(GetTileSlope(tile, NULL))) {
						msg = STR_3807_CAN_T_BUILD_ON_WATER;
						continue;
					}
				/* FALL THROUGH */
				case MP_CLEAR:
					if (IsBridgeAbove(tile)) {
						msg = STR_2804_SITE_UNSUITABLE;
						continue;
					}

					if (IsTileType(tile, MP_CLEAR)) {
						/* Remove fields or rocks. Note that the ground will get barrened */
						switch (GetClearGround(tile)) {
							case CLEAR_FIELDS:
							case CLEAR_ROCKS: {
								CommandCost ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
								if (CmdFailed(ret)) return ret;
								cost.AddCost(ret);
								break;
							}

							default: break;
						}
					}

					if (_game_mode != GM_EDITOR && IsValidPlayer(_current_player)) {
						Town *t = ClosestTownFromTile(tile, _patches.dist_local_authority);
						if (t != NULL) ChangeTownRating(t, RATING_TREE_UP_STEP, RATING_TREE_MAXIMUM);
					}

					if (flags & DC_EXEC) {
						TreeType treetype;

						treetype = (TreeType)p1;
						if (treetype == TREE_INVALID) {
							treetype = GetRandomTreeType(tile, GB(Random(), 24, 8));
							if (treetype == TREE_INVALID) treetype = TREE_CACTUS;
						}

						/* Plant full grown trees in scenario editor */
						PlantTreesOnTile(tile, treetype, 0, _game_mode == GM_EDITOR ? 3 : 0);
						MarkTileDirtyByTile(tile);

						/* When planting rainforest-trees, set tropiczone to rainforest in editor. */
						if (_game_mode == GM_EDITOR && IsInsideMM(treetype, TREE_RAINFOREST, TREE_CACTUS))
							SetTropicZone(tile, TROPICZONE_RAINFOREST);
					}
					cost.AddCost(_price.build_trees);
					break;

				default:
					msg = STR_2804_SITE_UNSUITABLE;
					break;
			}
		}
	}

	if (cost.GetCost() == 0) {
		return_cmd_error(msg);
	} else {
		return cost;
	}
}

struct TreeListEnt {
	SpriteID image;
	SpriteID pal;
	byte x, y;
};

static void DrawTile_Trees(TileInfo *ti)
{
	switch (GetTreeGround(ti->tile)) {
		case TREE_GROUND_SHORE: DrawShoreTile(ti->tileh); break;
		case TREE_GROUND_GRASS: DrawClearLandTile(ti, GetTreeDensity(ti->tile)); break;
		case TREE_GROUND_ROUGH: DrawHillyLandTile(ti); break;
		default: DrawGroundSprite(_tree_sprites_1[GetTreeDensity(ti->tile)] + _tileh_to_sprite[ti->tileh], PAL_NONE); break;
	}

	DrawClearLandFence(ti);

	/* Do not draw trees when the invisible trees patch and transparency tree are set */
	if (IsTransparencySet(TO_TREES) && _patches.invisible_trees) return;

	uint16 tmp = ti->x;

	tmp = ROR(tmp, 2);
	tmp -= ti->y;
	tmp = ROR(tmp, 3);
	tmp -= ti->x;
	tmp = ROR(tmp, 1);
	tmp += ti->y;

	uint index = GB(tmp, 6, 2) + (GetTreeType(ti->tile) << 2);

	/* different tree styles above one of the grounds */
	if (GetTreeGround(ti->tile) == TREE_GROUND_SNOW_DESERT &&
			GetTreeDensity(ti->tile) >= 2 &&
			IsInsideMM(index, TREE_SUB_ARCTIC << 2, TREE_RAINFOREST << 2)) {
		index += 164 - (TREE_SUB_ARCTIC << 2);
	}

	assert(index < lengthof(_tree_layout_sprite));

	const PalSpriteID *s = _tree_layout_sprite[index];
	const TreePos *d = _tree_layout_xy[GB(tmp, 4, 2)];

	/* combine trees into one sprite object */
	StartSpriteCombine();

	TreeListEnt te[4];

	/* put the trees to draw in a list */
	uint trees = GetTreeCount(ti->tile) + 1;

	for (uint i = 0; i < trees; i++) {
		SpriteID image = s[0].sprite + (i == trees - 1 ? GetTreeGrowth(ti->tile) : 3);
		SpriteID pal = s[0].pal;

		te[i].image = image;
		te[i].pal   = pal;
		te[i].x = d->x;
		te[i].y = d->y;
		s++;
		d++;
	}

	/* draw them in a sorted way */
	byte z = ti->z + GetSlopeMaxZ(ti->tileh) / 2;

	for (; trees > 0; trees--) {
		uint min = te[0].x + te[0].y;
		uint mi = 0;

		for (uint i = 1; i < trees; i++) {
			if (te[i].x + te[i].y < min) {
				min = te[i].x + te[i].y;
				mi = i;
			}
		}

		AddSortableSpriteToDraw(te[mi].image, te[mi].pal, ti->x + te[mi].x, ti->y + te[mi].y, 16 - te[mi].x, 16 - te[mi].y, 0x30, z, IsTransparencySet(TO_TREES), -te[mi].x, -te[mi].y);

		/* replace the removed one with the last one */
		te[mi] = te[trees - 1];
	}

	EndSpriteCombine();
}


static uint GetSlopeZ_Trees(TileIndex tile, uint x, uint y)
{
	uint z;
	Slope tileh = GetTileSlope(tile, &z);

	return z + GetPartialZ(x & 0xF, y & 0xF, tileh);
}

static Foundation GetFoundation_Trees(TileIndex tile, Slope tileh)
{
	return FOUNDATION_NONE;
}

static CommandCost ClearTile_Trees(TileIndex tile, byte flags)
{
	uint num;

	if (IsValidPlayer(_current_player)) {
		Town *t = ClosestTownFromTile(tile, _patches.dist_local_authority);
		if (t != NULL) ChangeTownRating(t, RATING_TREE_DOWN_STEP, RATING_TREE_MINIMUM);
	}

	num = GetTreeCount(tile) + 1;
	if (IsInsideMM(GetTreeType(tile), TREE_RAINFOREST, TREE_CACTUS)) num *= 4;

	if (flags & DC_EXEC) DoClearSquare(tile);

	return CommandCost(EXPENSES_CONSTRUCTION, num * _price.remove_trees);
}

static void GetAcceptedCargo_Trees(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void GetTileDesc_Trees(TileIndex tile, TileDesc *td)
{
	TreeType tt = GetTreeType(tile);

	if (IsInsideMM(tt, TREE_RAINFOREST, TREE_CACTUS)) {
		td->str = STR_280F_RAINFOREST;
	} else {
		td->str = tt == TREE_CACTUS ? STR_2810_CACTUS_PLANTS : STR_280E_TREES;
	}

	td->owner = GetTileOwner(tile);
}

static void AnimateTile_Trees(TileIndex tile)
{
	/* not used */
}

static void TileLoopTreesDesert(TileIndex tile)
{
	switch (GetTropicZone(tile)) {
		case TROPICZONE_DESERT:
			if (GetTreeGround(tile) != TREE_GROUND_SNOW_DESERT) {
				SetTreeGroundDensity(tile, TREE_GROUND_SNOW_DESERT, 3);
				MarkTileDirtyByTile(tile);
			}
			break;

		case TROPICZONE_RAINFOREST: {
			static const SoundFx forest_sounds[] = {
				SND_42_LOON_BIRD,
				SND_43_LION,
				SND_44_MONKEYS,
				SND_48_DISTANT_BIRD
			};
			uint32 r = Random();

			if (Chance16I(1, 200, r)) SndPlayTileFx(forest_sounds[GB(r, 16, 2)], tile);
			break;
		}

		default: break;
	}
}

static void TileLoopTreesAlps(TileIndex tile)
{
	int k = GetTileZ(tile) - GetSnowLine() + TILE_HEIGHT;

	if (k < 0) {
		if (GetTreeGround(tile) != TREE_GROUND_SNOW_DESERT) return;
		SetTreeGroundDensity(tile, TREE_GROUND_GRASS, 3);
	} else {
		uint density = min((uint)k / TILE_HEIGHT, 3);

		if (GetTreeGround(tile) != TREE_GROUND_SNOW_DESERT ||
				GetTreeDensity(tile) != density) {
			SetTreeGroundDensity(tile, TREE_GROUND_SNOW_DESERT, density);
		} else {
			if (GetTreeDensity(tile) == 3) {
				uint32 r = Random();
				if (Chance16I(1, 200, r)) {
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
	if (GetTreeGround(tile) == TREE_GROUND_SHORE) {
		TileLoop_Water(tile);
	} else {
		switch (_opt.landscape) {
			case LT_TROPIC: TileLoopTreesDesert(tile); break;
			case LT_ARCTIC: TileLoopTreesAlps(tile);   break;
		}
	}

	TileLoopClearHelper(tile);

	uint treeCounter = GetTreeCounter(tile);

	/* Handle growth of grass at every 8th processings, like it's done for grass */
	if ((treeCounter & 7) == 7 && GetTreeGround(tile) == TREE_GROUND_GRASS) {
		uint density = GetTreeDensity(tile);
		if (density < 3) {
			SetTreeGroundDensity(tile, TREE_GROUND_GRASS, density + 1);
			MarkTileDirtyByTile(tile);
		}
	}
	if (GetTreeCounter(tile) < 15) {
		AddTreeCounter(tile, 1);
		return;
	}
	SetTreeCounter(tile, 0);

	switch (GetTreeGrowth(tile)) {
		case 3: /* regular sized tree */
			if (_opt.landscape == LT_TROPIC &&
					GetTreeType(tile) != TREE_CACTUS &&
					GetTropicZone(tile) == TROPICZONE_DESERT) {
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

						tile += TileOffsByDir((Direction)(Random() & 7));

						/* Cacti don't spread */
						if (!CanPlantTreesOnTile(tile, false)) return;

						/* Don't plant trees, if ground was freshly cleared */
						if (IsTileType(tile, MP_CLEAR) && GetClearGround(tile) == CLEAR_GRASS && GetClearDensity(tile) != 3) return;

						PlantTreesOnTile(tile, treetype, 0, 0);

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
					case TREE_GROUND_SHORE: MakeShore(tile); break;
					case TREE_GROUND_GRASS: MakeClear(tile, CLEAR_GRASS, GetTreeDensity(tile)); break;
					case TREE_GROUND_ROUGH: MakeClear(tile, CLEAR_ROUGH, 3); break;
					default: // snow or desert
						MakeClear(tile, _opt.landscape == LT_TROPIC ? CLEAR_DESERT : CLEAR_SNOW, GetTreeDensity(tile));
						break;
				}
			}
			break;

		default:
			AddTreeGrowth(tile, 1);
			break;
	}

	MarkTileDirtyByTile(tile);
}

void OnTick_Trees()
{
	uint32 r;
	TileIndex tile;
	TreeType tree;

	/* place a tree at a random rainforest spot */
	if (_opt.landscape == LT_TROPIC &&
			(r = Random(), tile = RandomTileSeed(r), GetTropicZone(tile) == TROPICZONE_RAINFOREST) &&
			CanPlantTreesOnTile(tile, false) &&
			(tree = GetRandomTreeType(tile, GB(r, 24, 8))) != TREE_INVALID) {
		PlantTreesOnTile(tile, tree, 0, 0);
	}

	/* byte underflow */
	if (--_trees_tick_ctr != 0) return;

	/* place a tree at a random spot */
	r = Random();
	tile = TILE_MASK(r);
	if (CanPlantTreesOnTile(tile, false) && (tree = GetRandomTreeType(tile, GB(r, 24, 8))) != TREE_INVALID) {
		PlantTreesOnTile(tile, tree, 0, 0);
	}
}

static void ClickTile_Trees(TileIndex tile)
{
	/* not used */
}

static uint32 GetTileTrackStatus_Trees(TileIndex tile, TransportType mode, uint sub_mode)
{
	return 0;
}

static void ChangeTileOwner_Trees(TileIndex tile, PlayerID old_player, PlayerID new_player)
{
	/* not used */
}

void InitializeTrees()
{
	_trees_tick_ctr = 0;
}

static CommandCost TerraformTile_Trees(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new)
{
	return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
}


extern const TileTypeProcs _tile_type_trees_procs = {
	DrawTile_Trees,           /* draw_tile_proc */
	GetSlopeZ_Trees,          /* get_slope_z_proc */
	ClearTile_Trees,          /* clear_tile_proc */
	GetAcceptedCargo_Trees,   /* get_accepted_cargo_proc */
	GetTileDesc_Trees,        /* get_tile_desc_proc */
	GetTileTrackStatus_Trees, /* get_tile_track_status_proc */
	ClickTile_Trees,          /* click_tile_proc */
	AnimateTile_Trees,        /* animate_tile_proc */
	TileLoop_Trees,           /* tile_loop_clear */
	ChangeTileOwner_Trees,    /* change_tile_owner_clear */
	NULL,                     /* get_produced_cargo_proc */
	NULL,                     /* vehicle_enter_tile_proc */
	GetFoundation_Trees,      /* get_foundation_proc */
	TerraformTile_Trees,      /* terraform_tile_proc */
};
