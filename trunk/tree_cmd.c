#include "stdafx.h"
#include "ttd.h"
#include "viewport.h"
#include "command.h"
#include "town.h"

static int GetRandomTreeType(uint tile, uint seed)
{
	byte i;

	if (_opt.landscape == LT_NORMAL) {
		return seed * 12 >> 8;
	} else if (_opt.landscape == LT_HILLY) {
		return (seed >> 5) + 12;
	} else if (_opt.landscape == LT_DESERT) {
		i = GetMapExtraBits(tile);
		if (i == 0) {
			return (seed >> 6) + 28;
		} else if (i == 1) {
			if (seed > 12)
				return -1;
			return 27;
		} else {
			return (seed * 7 >> 8) + 20;
		}
	} else {
		return (seed * 9 >> 8) + 32;
	}
}

static void PlaceTree(uint tile, uint32 r, byte m5_or)
{
	int tree = GetRandomTreeType(tile, (r >> 24));
	byte m5;

	if (tree >= 0) {
		m5 = (byte)(r >> 16);
		if(m5==7) m5--;

		_map5[tile] = m5 & 0x07;	// growth state;
		_map5[tile] |=  m5 & 0xC0;	// amount of trees

		_map3_lo[tile] = tree;		// set type of tree
		_map3_hi[tile] = 0;		// no hedge

		// above snowline?
		if( (_opt.landscape == LT_HILLY) && (GetTileZ(tile) - _opt.snow_line > 0) )
		{
			_map2[tile] = 0xE0;	// set land type to snow
			_map2[tile] |= (byte)(r >> 24)&0x07; // randomize counter
		}
		else
		{
			_map2[tile] = (byte)(r >> 24)&0x1F; // randomize counter and ground
		}


		// make it tree class
		_map_type_and_height[tile] |= MP_TREES << 4;
	}
}

static void DoPlaceMoreTrees(uint tile)
{
	int i = 1000;
	int x,y;
	uint cur_tile;
	int dist;

	do {
		uint32 r = Random();
		x = (r & 0x1F) - 16;
		y = ((r>>8) & 0x1F) - 16;

		dist = myabs(x) + myabs(y);

		cur_tile=TILE_MASK(tile + TILE_XY(x,y));

		if (dist <= 13 && IS_TILETYPE(cur_tile, MP_CLEAR)) {
			PlaceTree(cur_tile, r, dist <= 6 ? 0xC0 : 0);
		}
	} while (--i);
}

static void PlaceMoreTrees()
{
	int i = (Random() & 0x1F) + 25;
	do {
		DoPlaceMoreTrees(TILE_MASK(Random()));
	} while (--i);
}

void PlaceTreesRandomly()
{
	int i;
	uint32 r;
	uint tile;

	i = 1000;
	do {
		r = Random();
		tile = TILE_MASK(r);
		if (IS_TILETYPE(tile, MP_CLEAR)) {
			PlaceTree(tile, r, 0);
		}
	} while (--i);

	/* place extra trees at rainforest area */
	if (_opt.landscape == LT_DESERT) {
		i = 15000;

		do {
			r = Random();
			tile = TILE_MASK(r);
			if (IS_TILETYPE(tile, MP_CLEAR) && GetMapExtraBits(tile) == 2) {
				PlaceTree(tile, r, 0);
			}
		} while (--i);
	}
}

void GenerateTrees()
{
	int i;

	if (_opt.landscape != LT_CANDY) {
		PlaceMoreTrees();
	}

	i = _opt.landscape == LT_HILLY ? 15 : 6;
	do {
		PlaceTreesRandomly();
	} while (--i);
}

/* Plant a tree
 * p1 = tree type, -1 means random.
 * p2 = end tile
 */

int32 CmdPlantTree(int ex, int ey, uint32 flags, uint32 p1, uint32 p2)
{
	TileInfo ti;
	int32 cost;
	int sx,sy,x,y;
	int treetype;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	// make sure sx,sy are smaller than ex,ey
	sx = GET_TILE_X(p2)*16;
	sy = GET_TILE_Y(p2)*16;
	if (ex < sx) intswap(ex, sx);
	if (ey < sy) intswap(ey, sy);

	cost = 0; // total cost

	for(x=sx; x<=ex; x+=16) {
		for(y=sy; y<=ey; y+=16) {
			FindLandscapeHeight(&ti, x, y);
			if (!EnsureNoVehicle(ti.tile))
				continue;

			if (ti.type == MP_TREES) {
				// no more space for trees?
				if (_game_mode != GM_EDITOR && (ti.map5 & 0xC0) == 0xC0) {
					_error_message = STR_2803_TREE_ALREADY_HERE;
					continue;
				}

				if (flags & DC_EXEC) {
					_map5[ti.tile] = ti.map5 + 0x40;
					MarkTileDirtyByTile(ti.tile);
				}
				// 2x as expensive to add more trees to an existing tile
				cost += _price.build_trees * 2;
			} else {
				// don't allow building on rocks
				if (ti.type != MP_CLEAR || _map_owner[ti.tile] != OWNER_NONE || (ti.map5 & 0x1C) == 8) {
					_error_message = STR_2804_SITE_UNSUITABLE;
					continue;
				}

				// it's expensive to clear farmland
				if ((ti.map5 & 0x1F) == 0xF) cost += _price.clear_3;

				if (flags & DC_EXEC) {
					int m2;

					if (_game_mode != GM_EDITOR && _current_player < MAX_PLAYERS) {
						Town *t = ClosestTownFromTile(ti.tile, _patches.dist_local_authority);
						if (t != NULL)
							ChangeTownRating(t, 7, 220);
					}
					m2 = 0;
					if ( (ti.map5 & 0x1C) == 4 ) {
						m2 = 16;
					} else if ( (ti.map5 & 0x1C) == 16 ) {
						m2 = (ti.map5 << 6) | 0x20;
					}

					treetype = p1;
					if (treetype == -1) {
						treetype = GetRandomTreeType(ti.tile, Random()>>24);
						if (treetype==-1) treetype=27;
					}

					ModifyTile(ti.tile,
						MP_SETTYPE(MP_TREES) |
						MP_MAP2 | MP_MAP3LO | MP_MAP3HI_CLEAR | MP_MAP5,
						m2, /* map2 */
						treetype, /* map3lo */
			 			_game_mode == GM_EDITOR ? 3 : 0 /* map5 */
					);

					if (_game_mode == GM_EDITOR && IS_BYTE_INSIDE(treetype, 0x14, 0x1B)) {
						SetMapExtraBits(ti.tile, 2);
					}
				}
				cost += _price.build_trees;
			}
		}
	}

	if (cost == 0) return CMD_ERROR;
	return cost;
}

typedef struct TreeListEnt {
	uint32 image;
	byte x,y;
} TreeListEnt;


#include "table/tree_land.h"


static void DrawTile_Trees(TileInfo *ti)
{
	byte m2;
	const uint32 *s;
	const byte *d;
	byte z;
	TreeListEnt te[4];

	m2 = _map2[ti->tile];

	if ( (m2&0x30) == 0) {
		DrawClearLandTile(ti, 3);
	} else if ((m2&0x30) == 0x20) {
		DrawGroundSprite(_tree_sprites_1[m2 >> 6] + _tileh_to_sprite[ti->tileh]);
	} else {
		DrawHillyLandTile(ti);
	}

	DrawClearLandFence(ti, _map3_hi[ti->tile] >> 2);

	z = ti->z;
	if (ti->tileh != 0) {
		z += 4;
		if (ti->tileh & 0x10)
			z += 4;
	}

	{
		uint16 tmp = ti->x;
		int index;

		tmp = (tmp >> 2) | (tmp << 14);
		tmp -= ti->y;
		tmp = (tmp >> 3) | (tmp << 13);
		tmp -= ti->x;
		tmp = (tmp >> 1) | (tmp << 15);
		tmp += ti->y;

		d = _tree_layout_xy[(tmp & 0x30) >> 4];

		index = ((tmp>>6)&3) + (_map3_lo[ti->tile]<<2);

		/* different tree styles above one of the grounds */
		if ((m2 & 0xB0) == 0xA0 && index >= 48 && index < 80)
			index += 164 - 48;

		assert(index < lengthof(_tree_layout_sprite));
		s = _tree_layout_sprite[index];
	}

	StartSpriteCombine();

	if((_display_opt & DO_TRANS_BUILDINGS) || !_patches.invisible_trees)
	{
		int i;

		/* put the trees to draw in a list */
		i = (ti->map5 >> 6) + 1;
		do {
			uint32 image = s[0] + (--i==0 ? (ti->map5 & 7) : 3);
			if (!(_display_opt & DO_TRANS_BUILDINGS))
				image = (image & 0x3FFF) | 0x3224000;
			te[i].image = image;
			te[i].x = d[0];
			te[i].y = d[1];
			s++;
			d+=2;
		} while (i);

		/* draw them in a sorted way */
		for(;;) {
			byte min = 0xFF;
			TreeListEnt *tep = NULL;

			i = (ti->map5 >> 6) + 1;
			do {
				if (te[--i].image!=0 && (byte)(te[i].x + te[i].y) < min) {
					min = te[i].x + te[i].y;
					tep = &te[i];
				}
			} while (i);

			if (tep == NULL)
				break;

			AddSortableSpriteToDraw(tep->image, ti->x + tep->x, ti->y + tep->y, 5, 5, 0x10, z);
			tep->image = 0;
		}
	}

	EndSpriteCombine();
}


static uint GetSlopeZ_Trees(TileInfo *ti) {
	return GetPartialZ(ti->x&0xF, ti->y&0xF, ti->tileh) + ti->z;
}

static uint GetSlopeTileh_Trees(TileInfo *ti) {
	return ti->tileh;
}

static int32 ClearTile_Trees(uint tile, byte flags) {
	int num;

	if (flags & DC_EXEC && _current_player < MAX_PLAYERS) {
		Town *t = ClosestTownFromTile(tile, _patches.dist_local_authority);
		if (t != NULL)
			ChangeTownRating(t, -35, -1000);
	}

	num = (_map5[tile] >> 6) + 1;
	if ( (byte)(_map3_lo[tile]-0x14) <= (0x1A-0x14))
		num <<= 2;

	if (flags & DC_EXEC)
		DoClearSquare(tile);

	return num * _price.remove_trees;
}

static void GetAcceptedCargo_Trees(uint tile, AcceptedCargo *ac)
{
	/* not used */
}

static void GetTileDesc_Trees(uint tile, TileDesc *td)
{
	byte b;
	StringID str;

	td->owner = _map_owner[tile];

	b = _map3_lo[tile];
	(str=STR_2810_CACTUS_PLANTS, b==0x1B) ||
	(str=STR_280F_RAINFOREST, IS_BYTE_INSIDE(b, 0x14, 0x1A+1)) ||
	(str=STR_280E_TREES, true);
	td->str = str;
}

static void AnimateTile_Trees(uint tile)
{
	/* not used */
}

static byte _desert_sounds[] = {
	66,67,68,72
};

static void TileLoopTreesDesert(uint tile)
{
	byte b;

	if ((b=GetMapExtraBits(tile)) == 2) {
		uint32 r;

		if (CHANCE16I(1,200,r=Random())) {
			SndPlayTileFx(_desert_sounds[(r >> 16) & 3], tile);
		}
	} else if (b == 1) {
		if ((_map2[tile] & 0x30) != 0x20) {
			_map2[tile] &= 0xF;
			_map2[tile] |= 0xE0;
			MarkTileDirtyByTile(tile);
		}
	}
}

static void TileLoopTreesAlps(uint tile)
{
	byte tmp, m2;
	int k;

	/* distance from snow line, in steps of 8 */
	k = GetTileZ(tile) - _opt.snow_line;

	tmp = _map5[tile] & 0xF0;

	if (k < -8) {
		/* snow_m2_down */
		if ((tmp&0x30) != 0x20)
			return;
		m2 = 0;
	} else if (k == -8) {
		/* snow_m1 */
		m2 = 0x20;
		if (tmp == m2)
			return;
	} else if (k < 8) {
		/* snow_0 */
		m2 = 0x60;
		if (tmp == m2)
			return;
	} else if (k == 8) {
		/* snow_p1 */
		m2 = 0xA0;
		if (tmp == m2)
			return;
	} else {
		/* snow_p2_up */
		if (tmp == 0xC0) {
			uint32 r;
			if (CHANCE16I(1,200,r=Random())) {
				SndPlayTileFx( (r&0x80000000) ? 57 : 52, tile);
			}
			return;
		} else {
			m2 = 0xE0;
		}
	}

	_map2[tile] &= 0xF;
	_map2[tile] |= m2;
	MarkTileDirtyByTile(tile);
}

static void TileLoop_Trees(uint tile)
{
	byte m5, m2;

	static const TileIndexDiff _tileloop_trees_dir[] = {
		TILE_XY(-1,-1),
		TILE_XY(0,-1),
		TILE_XY(1,-1),
		TILE_XY(-1,0),
		TILE_XY(1,0),
		TILE_XY(-1,1),
		TILE_XY(0,1),
		TILE_XY(1,1),
	};

	if (_opt.landscape == LT_DESERT) {
		TileLoopTreesDesert(tile);
	} else if (_opt.landscape == LT_HILLY) {
		TileLoopTreesAlps(tile);
	}

	TileLoopClearHelper(tile);

	/* increase counter */
	{
		byte m2 = _map2[tile];
		_map2[tile] = m2 = (m2 & 0xF0) | ((m2+1)&0xF);
		if (m2 & 0xF)
			return;
	}

	m5 = _map5[tile];
	if ((m5&7) == 3) {
		/* regular sized tree */
		if (_opt.landscape == LT_DESERT && _map3_lo[tile]!=0x1B && GetMapExtraBits(tile)==1) {
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
				byte m3 = _map3_lo[tile];

				tile += _tileloop_trees_dir[Random() & 7];

				if (!IS_TILETYPE(tile, MP_CLEAR))
					return;

				if ( (_map5[tile] & 0x1C) == 4) {
					_map2[tile] = 0x10;
				} else if ((_map5[tile] & 0x1C) == 16) {
					_map2[tile] = (_map5[tile] << 6) | 0x20;
				} else {
					if ((_map5[tile] & 0x1F) != 3)
						return;
					_map2[tile] = 0;
				}

				_map3_lo[tile] = m3;
				_map3_hi[tile] = 0;
				_map_type_and_height[tile] &= 0xF;
				_map_type_and_height[tile] |= MP_TREES << 4;

				m5 = 0;
				break;
			}

			default:
				return;
			}
		}
	} else if ((m5&7) == 6) {
		/* final stage of tree destruction */
		if (m5 & 0xC0) {
			/* more than one tree, delete it? */
			m5 = ((m5 - 6) - 0x40) + 3;
		} else {
			/* just one tree, change type into MP_CLEAR */
			_map_type_and_height[tile] = (_map_type_and_height[tile]&~0xF0) | (MP_CLEAR<<4);

			m5 = 3;
			m2 = _map2[tile];
			if ((m2&0x30) != 0) {
				m5 = (m2 >> 6) | 0x10;
				if (m2 != 0x20)
					m5 = 7;
			}
			_map_owner[tile] = OWNER_NONE;
		}
	} else {
		/* in the middle of a transition, change to next */
		m5++;
	}

	_map5[tile] = m5;
	MarkTileDirtyByTile(tile);
}

void OnTick_Trees()
{
	uint32 r;
	uint tile;
	byte m;
	int tree;

	/* place a tree at a random rainforest spot */
	if (_opt.landscape == LT_DESERT &&
			(r=Random(),tile=TILE_MASK(r),GetMapExtraBits(tile)==2) &&
			IS_TILETYPE(tile, MP_CLEAR) &&
			(m=_map5[tile]&0x1C, m<=4) &&
			(tree=GetRandomTreeType(tile, r>>24)) >= 0) {

		ModifyTile(tile,
			MP_SETTYPE(MP_TREES) |
			MP_MAP2 | MP_MAP3LO | MP_MAP3HI | MP_MAP5,
			(m == 4 ? 0x10 : 0),
			tree,
			_map3_hi[tile] & ~3,
			0
		);
	}

	// byte underflow
	if (--_trees_tick_ctr)
		return;

	/* place a tree at a random spot */
	r = Random();
	tile = TILE_MASK(r);
	if (IS_TILETYPE(tile, MP_CLEAR) &&
			(m=_map5[tile]&0x1C, m==0 || m==4 || m==0x10) &&
			(tree=GetRandomTreeType(tile, r>>24)) >= 0) {
		int m2;

		if (m == 0) {
			m2 = 0;
		} else if (m == 4) {
			m2 = 0x10;
		} else {
			m2 = (_map5[tile] << 6) | 0x20;
		}

		ModifyTile(tile,
			MP_SETTYPE(MP_TREES) |
			MP_MAP2 | MP_MAP3LO | MP_MAP3HI | MP_MAP5,
			m2,
			tree,
			_map3_hi[tile] & ~3,
			0
		);
	}
}

static void ClickTile_Trees(uint tile)
{
	/* not used */
}

static uint32 GetTileTrackStatus_Trees(uint tile, TransportType mode)
{
	return 0;
}

static void ChangeTileOwner_Trees(uint tile, byte old_player, byte new_player)
{
	/* not used */
}

void InitializeTrees()
{
	memset(_sign_list, 0, sizeof(_sign_list));
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
