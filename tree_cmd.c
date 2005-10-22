/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "table/tree_land.h"
#include "functions.h"
#include "map.h"
#include "tile.h"
#include "viewport.h"
#include "command.h"
#include "town.h"
#include "sound.h"
#include "variables.h"

static int GetRandomTreeType(TileIndex tile, uint seed)
{
	switch (_opt.landscape) {
		case LT_NORMAL:
			return seed * 12 >> 8;

		case LT_HILLY:
			return (seed >> 5) + 12;

		case LT_DESERT:
			switch (GetMapExtraBits(tile)) {
				case 0:  return (seed >> 6) + 28;
				case 1:  return (seed > 12) ? -1 : 27;
				default: return (seed * 7 >> 8) + 20;
			}

		default:
			return (seed * 9 >> 8) + 32;
	}
}

static void PlaceTree(TileIndex tile, uint32 r, byte m5_or)
{
	int tree = GetRandomTreeType(tile, GB(r, 24, 8));
	byte m5;

	if (tree >= 0) {
		SetTileType(tile, MP_TREES);

		m5 = GB(r, 16, 8);
		if (GB(m5, 0, 3) == 7) m5--; // there is no growth state 7

		_m[tile].m5 = m5 & 0x07;	// growth state;
		_m[tile].m5 |=  m5 & 0xC0;	// amount of trees

		_m[tile].m3 = tree;		// set type of tree
		_m[tile].m4 = 0;		// no hedge

		// above snowline?
		if (_opt.landscape == LT_HILLY && GetTileZ(tile) > _opt.snow_line) {
			_m[tile].m2 = 0xE0;	// set land type to snow
			_m[tile].m2 |= GB(r, 24, 3); // randomize counter
		} else {
			_m[tile].m2 = GB(r, 24, 5); // randomize counter and ground
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

		/* Only on tiles within 13 squares from tile,
		    on clear tiles, and NOT on farm-tiles or rocks */
		if (dist <= 13 && IsTileType(cur_tile, MP_CLEAR) &&
			 (_m[cur_tile].m5 & 0x1F) != 0x0F && (_m[cur_tile].m5 & 0x1C) != 8) {
			PlaceTree(cur_tile, r, dist <= 6 ? 0xC0 : 0);
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
		/* Only on clear tiles, and NOT on farm-tiles or rocks */
		if (IsTileType(tile, MP_CLEAR) && (_m[tile].m5 & 0x1F) != 0x0F && (_m[tile].m5 & 0x1C) != 8) {
			PlaceTree(tile, r, 0);
		}
	} while (--i);

	/* place extra trees at rainforest area */
	if (_opt.landscape == LT_DESERT) {
		i = ScaleByMapSize(15000);

		do {
			uint32 r = Random();
			TileIndex tile = RandomTileSeed(r);
			if (IsTileType(tile, MP_CLEAR) && GetMapExtraBits(tile) == 2) {
				PlaceTree(tile, r, 0);
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

	if (p2 > MapSize()) return CMD_ERROR;
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
					if (_game_mode != GM_EDITOR && (_m[tile].m5 & 0xC0) == 0xC0) {
						_error_message = STR_2803_TREE_ALREADY_HERE;
						continue;
					}

					if (flags & DC_EXEC) {
						_m[tile].m5 += 0x40;
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

					// it's expensive to clear farmland
					if ((_m[tile].m5 & 0x1F) == 0xF)
						cost += _price.clear_3;
					else if ((_m[tile].m5 & 0x1C) == 8)
						cost += _price.clear_2;

					if (flags & DC_EXEC) {
						int treetype;
						int m2;

						if (_game_mode != GM_EDITOR && _current_player < MAX_PLAYERS) {
							Town *t = ClosestTownFromTile(tile, _patches.dist_local_authority);
							if (t != NULL)
								ChangeTownRating(t, RATING_TREE_UP_STEP, RATING_TREE_MAXIMUM);
						}

						switch (_m[tile].m5 & 0x1C) {
							case 0x04:
								m2 = 16;
								break;

							case 0x10:
								m2 = ((_m[tile].m5 & 3) << 6) | 0x20;
								break;

							default:
								m2 = 0;
								break;
						}

						treetype = p1;
						if (treetype == -1) {
							treetype = GetRandomTreeType(tile, Random() >> 24);
							if (treetype == -1) treetype = 27;
						}

						ModifyTile(tile,
							MP_SETTYPE(MP_TREES) |
							MP_MAP2 | MP_MAP3LO | MP_MAP3HI_CLEAR | MP_MAP5,
							m2, /* map2 */
							treetype, /* map3lo */
							_game_mode == GM_EDITOR ? 3 : 0 /* map5 */
						);

						if (_game_mode == GM_EDITOR && IS_BYTE_INSIDE(treetype, 0x14, 0x1B))
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
	uint16 m2;
	const uint32 *s;
	const byte *d;
	byte z;

	m2 = _m[ti->tile].m2;

	if ((m2 & 0x30) == 0) {
		DrawClearLandTile(ti, 3);
	} else if ((m2 & 0x30) == 0x20) {
		DrawGroundSprite(_tree_sprites_1[m2 >> 6] + _tileh_to_sprite[ti->tileh]);
	} else {
		DrawHillyLandTile(ti);
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

		index = GB(tmp, 6, 2) + (_m[ti->tile].m3 << 2);

		/* different tree styles above one of the grounds */
		if ((m2 & 0xB0) == 0xA0 && index >= 48 && index < 80)
			index += 164 - 48;

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
			if (_display_opt & DO_TRANS_BUILDINGS)
				MAKE_TRANSPARENT(image);
			te[i].image = image;
			te[i].x = d[0];
			te[i].y = d[1];
			s++;
			d += 2;
		} while (i);

		/* draw them in a sorted way */
		for(;;) {
			byte min = 0xFF;
			TreeListEnt *tep = NULL;

			i = (ti->map5 >> 6) + 1;
			do {
				if (te[--i].image != 0 && (byte)(te[i].x + te[i].y) < min) {
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

	num = GB(_m[tile].m5, 6, 2) + 1;
	if (IS_INT_INSIDE(_m[tile].m3, 20, 26 + 1)) num *= 4;

	if (flags & DC_EXEC) DoClearSquare(tile);

	return num * _price.remove_trees;
}

static void GetAcceptedCargo_Trees(TileIndex tile, AcceptedCargo ac)
{
	/* not used */
}

static void GetTileDesc_Trees(TileIndex tile, TileDesc *td)
{
	byte b;
	StringID str;

	td->owner = GetTileOwner(tile);

	b = _m[tile].m3;
	(str=STR_2810_CACTUS_PLANTS, b==0x1B) ||
	(str=STR_280F_RAINFOREST, IS_BYTE_INSIDE(b, 0x14, 0x1A+1)) ||
	(str=STR_280E_TREES, true);
	td->str = str;
}

static void AnimateTile_Trees(TileIndex tile)
{
	/* not used */
}

static void TileLoopTreesDesert(TileIndex tile)
{
	static const SoundFx forest_sounds[] = {
		SND_42_LOON_BIRD,
		SND_43_LION,
		SND_44_MONKEYS,
		SND_48_DISTANT_BIRD
	};

	byte b = GetMapExtraBits(tile);

	if (b == 2) {
		uint32 r = Random();

		if (CHANCE16I(1, 200, r)) SndPlayTileFx(forest_sounds[GB(r, 16, 2)], tile);
	} else if (b == 1) {
		if (GB(_m[tile].m2, 4, 2) != 2) {
			SB(_m[tile].m2, 4, 2, 2);
			SB(_m[tile].m2, 6, 2, 3);
			MarkTileDirtyByTile(tile);
		}
	}
}

static void TileLoopTreesAlps(TileIndex tile)
{
	byte tmp, m2;
	int k;

	/* distance from snow line, in steps of 8 */
	k = GetTileZ(tile) - _opt.snow_line;

	tmp = _m[tile].m2 & 0xF0;

	if (k < -8) {
		if ((tmp & 0x30) != 0x20) return;
		m2 = 0; // no snow
	} else if (k == -8) {
		m2 = 0x20; // 1/4 snow
		if (tmp == m2) return;
	} else if (k == 0) {
		m2 = 0x60;// 1/2 snow
		if (tmp == m2) return;
	} else if (k == 8) {
		m2 = 0xA0; // 3/4 snow
		if (tmp == m2) return;
	} else {
		if (tmp == 0xE0) {
			uint32 r = Random();
			if (CHANCE16I(1, 200, r)) {
				SndPlayTileFx((r & 0x80000000) ? SND_39_HEAVY_WIND : SND_34_WIND, tile);
			}
			return;
		} else {
			m2 = 0xE0; // full snow
		}
	}

	_m[tile].m2 &= 0xF;
	_m[tile].m2 |= m2;
	MarkTileDirtyByTile(tile);
}

static void TileLoop_Trees(TileIndex tile)
{
	byte m5;
	uint16 m2;

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

	if (_opt.landscape == LT_DESERT) {
		TileLoopTreesDesert(tile);
	} else if (_opt.landscape == LT_HILLY) {
		TileLoopTreesAlps(tile);
	}

	TileLoopClearHelper(tile);

	/* increase counter */
	AB(_m[tile].m2, 0, 4, 1);
	if (GB(_m[tile].m2, 0, 4) != 0) return;

	m5 = _m[tile].m5;
	if (GB(m5, 0, 3) == 3) {
		/* regular sized tree */
		if (_opt.landscape == LT_DESERT && _m[tile].m3 != 0x1B && GetMapExtraBits(tile) == 1) {
			m5++; /* start destructing */
		} else {
			switch(Random() & 0x7) {
			case 0: /* start destructing */
				m5++;
				break;

			case 1: /* add a tree */
				if (m5 < 0xC0) {
					m5 = (m5 + 0x40) & ~7;
					break;
				}
				/* fall through */

			case 2: { /* add a neighbouring tree */
				byte m3 = _m[tile].m3;

				tile += ToTileIndexDiff(_tileloop_trees_dir[Random() & 7]);

				if (!IsTileType(tile, MP_CLEAR)) return;

				if ( (_m[tile].m5 & 0x1C) == 4) {
					_m[tile].m2 = 0x10;
				} else if ((_m[tile].m5 & 0x1C) == 16) {
					_m[tile].m2 = ((_m[tile].m5 & 3) << 6) | 0x20;
				} else {
					if ((_m[tile].m5 & 0x1F) != 3) return;
					_m[tile].m2 = 0;
				}

				_m[tile].m3 = m3;
				_m[tile].m4 = 0;
				SetTileType(tile, MP_TREES);

				m5 = 0;
				break;
			}

			default:
				return;
			}
		}
	} else if (GB(m5, 0, 3) == 6) {
		/* final stage of tree destruction */
		if (GB(m5, 6, 2) != 0) {
			/* more than one tree, delete it? */
			m5 = ((m5 - 6) - 0x40) + 3;
		} else {
			/* just one tree, change type into MP_CLEAR */
			SetTileType(tile, MP_CLEAR);

			m5 = 3;
			m2 = _m[tile].m2;
			if ((m2 & 0x30) != 0) { // on snow/desert or rough land
				m5 = (m2 >> 6) | 0x10;
				if ((m2 & 0x30) != 0x20) // if not on snow/desert, then on rough land
					m5 = 7;
			}
			SetTileOwner(tile, OWNER_NONE);
		}
	} else {
		/* in the middle of a transition, change to next */
		m5++;
	}

	_m[tile].m5 = m5;
	MarkTileDirtyByTile(tile);
}

void OnTick_Trees(void)
{
	uint32 r;
	TileIndex tile;
	byte m;
	int tree;

	/* place a tree at a random rainforest spot */
	if (_opt.landscape == LT_DESERT &&
			(r = Random(), tile = RandomTileSeed(r), GetMapExtraBits(tile) == 2) &&
			IsTileType(tile, MP_CLEAR) &&
			(m = _m[tile].m5 & 0x1C, m <= 4) &&
			(tree = GetRandomTreeType(tile, r >> 24)) >= 0) {

		ModifyTile(tile,
			MP_SETTYPE(MP_TREES) |
			MP_MAP2 | MP_MAP3LO | MP_MAP3HI | MP_MAP5,
			(m == 4 ? 0x10 : 0),
			tree,
			_m[tile].m4 & ~3,
			0
		);
	}

	// byte underflow
	if (--_trees_tick_ctr != 0) return;

	/* place a tree at a random spot */
	r = Random();
	tile = TILE_MASK(r);
	if (IsTileType(tile, MP_CLEAR) &&
			(m = _m[tile].m5 & 0x1C, m == 0 || m == 4 || m == 0x10) &&
			(tree = GetRandomTreeType(tile, r >> 24)) >= 0) {
		int m2;

		if (m == 0) {
			m2 = 0;
		} else if (m == 4) {
			m2 = 0x10;
		} else {
			m2 = ((_m[tile].m5 & 3) << 6) | 0x20;
		}

		ModifyTile(tile,
			MP_SETTYPE(MP_TREES) |
			MP_MAP2 | MP_MAP3LO | MP_MAP3HI | MP_MAP5,
			m2,
			tree,
			_m[tile].m4 & ~3,
			0
		);
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
