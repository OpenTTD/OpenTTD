#include "stdafx.h"
#include "ttd.h"
#include "vehicle.h"
#include "command.h"
#include "pathfind.h"
#include "station.h"
#include "gfx.h"
#include "news.h"
#include "engine.h"
#include "gui.h"
#include "player.h"


static const uint16 _ship_sprites[] = {0x0E5D, 0x0E55, 0x0E65, 0x0E6D};
static const byte _ship_sometracks[4] = {0x19, 0x16, 0x25, 0x2A};

static byte GetTileShipTrackStatus(uint tile) {
	uint32 r = GetTileTrackStatus(tile, TRANSPORT_WATER);
	return r | r >> 8;
}

void DrawShipEngine(int x, int y, int engine, uint32 image_ormod)
{
	int spritenum = ship_vehicle_info(engine).image_index;

	if (is_custom_sprite(spritenum)) {
		int sprite = GetCustomEngineSprite(engine, 0xffff, CID_PURCHASE, 0, 0, 6);

		if (sprite) {
			DrawSprite(sprite | image_ormod, x, y);
			return;
		}
		spritenum = _engine_original_sprites[engine];
	}
	DrawSprite((6 + _ship_sprites[spritenum]) | image_ormod, x, y);
}

void DrawShipEngineInfo(int engine, int x, int y, int maxw)
{
	ShipVehicleInfo *svi = &ship_vehicle_info(engine);
	SET_DPARAM32(0, svi->base_cost * (_price.ship_base>>3)>>5);
	SET_DPARAM16(1, svi->max_speed * 10 >> 5);
	SET_DPARAM16(2, _cargoc.names_long_p[svi->cargo_type]);
	SET_DPARAM16(3, svi->capacity);
	SET_DPARAM32(4, svi->running_cost * _price.ship_running >> 8);
	DrawStringMultiCenter(x, y, STR_982E_COST_MAX_SPEED_CAPACITY, maxw);
}

int GetShipImage(Vehicle *v, byte direction)
{
	int spritenum = v->spritenum;

	if (is_custom_sprite(spritenum)) {
		int sprite = GetCustomVehicleSprite(v, direction);

		if (sprite) return sprite;
		spritenum = _engine_original_sprites[v->engine_type];
	}
	return _ship_sprites[spritenum] + direction;
}

static int FindClosestShipDepot(Vehicle *v)
{
	uint tile, dist, best_dist = (uint)-1;
	int best_depot = -1;
	int i;
	byte owner = v->owner;
	uint tile2 = v->tile;

	for(i=0; i!=lengthof(_depots); i++) {
		tile = _depots[i].xy;
		if (IS_TILETYPE(tile, MP_WATER) && _map_owner[tile] == owner) {
			dist = GetTileDist(tile, tile2);
			if (dist < best_dist) {
				best_dist = dist;
				best_depot = i;
			}
		}
	}

	return best_depot;
}

static void CheckIfShipNeedsService(Vehicle *v)
{
	int i;

	if (_patches.servint_ships == 0)
		return;

	if (SERVICE_INTERVAL)
		return;

	if (v->vehstatus & VS_STOPPED)
		return;
	
	if ((v->next_order & (OT_MASK | OF_FULL_LOAD)) == (OT_GOTO_DEPOT | OF_FULL_LOAD))
		return;

	if (_patches.gotodepot && ScheduleHasDepotOrders(v->schedule_ptr))
		return;

	i = FindClosestShipDepot(v);

	if (i < 0 || GetTileDist(v->tile, (&_depots[i])->xy) > 12) {
		if ((v->next_order & OT_MASK) == OT_GOTO_DEPOT) {
			v->next_order = OT_DUMMY;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
		}
		return;
	}

	v->next_order = OT_GOTO_DEPOT | OF_NON_STOP;
	v->next_order_param = (byte)i;
	v->dest_tile = (&_depots[i])->xy;
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);		
}

void OnNewDay_Ship(Vehicle *v)
{
	int32 cost;

	if ((++v->day_counter & 7) == 0)
		DecreaseVehicleValue(v);

	CheckVehicleBreakdown(v);
	AgeVehicle(v);
	CheckIfShipNeedsService(v);

	CheckOrders(v);
	
	if (v->vehstatus & VS_STOPPED)
		return;

	

	cost = ship_vehicle_info(v->engine_type).running_cost * _price.ship_running / 364;
	v->profit_this_year -= cost >> 8;

	SET_EXPENSES_TYPE(EXPENSES_SHIP_RUN);
	SubtractMoneyFromPlayerFract(v->owner, cost);

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	InvalidateWindow(WC_SHIPS_LIST, v->owner);
}

static void HandleBrokenShip(Vehicle *v)
{
	if (v->breakdown_ctr != 1) {
		v->breakdown_ctr = 1;
		v->cur_speed = 0;

		if (v->breakdowns_since_last_service != 255)
			v->breakdowns_since_last_service++;
		
		InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		
		SndPlayVehicleFx((_opt.landscape != LT_CANDY) ? 0xE : 0x3A, v);

		if (!(v->vehstatus & VS_HIDDEN)) {
			Vehicle *u = CreateEffectVehicleRel(v, 4, 4, 5, EV_BREAKDOWN_SMOKE);
			if (u)
				u->u.special.unk0 = v->breakdown_delay * 2;
		}
	}

	if (!(v->tick_counter & 1)) {
		if (!--v->breakdown_delay) {
			v->breakdown_ctr = 0;
			InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		}
	}
}

static void MarkShipDirty(Vehicle *v)
{
	v->cur_image = GetShipImage(v, v->direction);
	MarkAllViewportsDirty(v->left_coord, v->top_coord, v->right_coord + 1, v->bottom_coord + 1);
}

static void PlayShipSound(Vehicle *v)
{
	SndPlayVehicleFx(ship_vehicle_info(v->engine_type).sfx, v);
}

static const TileIndexDiff _dock_offs[] = {
	TILE_XY(2, 0),
	TILE_XY(-2, 0),
	TILE_XY(0, 2),
	TILE_XY(2, 0),
	TILE_XY(0, -2),
	TILE_XY(0,0),
	TILE_XY(0,0),
	TILE_XY(0,0),
};

static void ProcessShipOrder(Vehicle *v)
{
	uint order;
	Station *st;

	if ((v->next_order & OT_MASK) >= OT_GOTO_DEPOT && (v->next_order & OT_MASK) <= OT_LEAVESTATION) {
		if ((v->next_order & (OT_MASK|OF_UNLOAD)) != (OT_GOTO_DEPOT|OF_UNLOAD))
			return;
	}

	if ((v->next_order & (OT_MASK|OF_UNLOAD|OF_FULL_LOAD)) == (OT_GOTO_DEPOT|OF_UNLOAD|OF_FULL_LOAD) &&
			SERVICE_INTERVAL) {
		v->cur_order_index++;
	}


	if (v->cur_order_index >= v->num_orders)
		v->cur_order_index = 0;

	order = v->schedule_ptr[v->cur_order_index];

	if (order == 0) {
		v->next_order = OT_NOTHING;
		v->dest_tile = 0;
		return;
	}

	if (order == (uint)((v->next_order | (v->next_order_param<<8))))
		return;

	v->next_order = (byte)order;
	v->next_order_param = (byte)(order >> 8);

	if ((order & OT_MASK) == OT_GOTO_STATION) {
		if ( (byte)(order >> 8) == v->last_station_visited)
			v->last_station_visited = 0xFF;
		
		st = DEREF_STATION(order >> 8);
		if (st->dock_tile != 0) {
			v->dest_tile = TILE_ADD(st->dock_tile, _dock_offs[_map5[st->dock_tile]-0x4B]);
		}
	} else if ((order & OT_MASK) == OT_GOTO_DEPOT) {
		v->dest_tile = _depots[order >> 8].xy;
	} else {
		v->dest_tile = 0;
	}
	InvalidateVehicleOrderWidget(v);
}

static void HandleShipLoading(Vehicle *v)
{
	if (v->next_order == OT_NOTHING)
		return;
	
	if (v->next_order != OT_DUMMY) {
		if ((v->next_order&OT_MASK) != OT_LOADING)
			return;

		if (--v->load_unload_time_rem)
			return;

		if (v->next_order&OF_FULL_LOAD && CanFillVehicle(v)) {
			SET_EXPENSES_TYPE(EXPENSES_SHIP_INC);
			if (LoadUnloadVehicle(v)) {
				InvalidateWindow(WC_SHIPS_LIST, v->owner);
				MarkShipDirty(v);
			}
			return;
		}
		PlayShipSound(v);
		
		{
			byte b = v->next_order;
			v->next_order = OT_LEAVESTATION;
			if (!(b & OF_NON_STOP))
				return;
		}
	}

	v->cur_order_index++;
	InvalidateVehicleOrderWidget(v);
}

static void UpdateShipDeltaXY(Vehicle *v, int dir)
{
#define MKIT(d,c,b,a) ((a&0xFF)<<24) | ((b&0xFF)<<16) | ((c&0xFF)<<8) | ((d&0xFF)<<0)
	static const uint32 _delta_xy_table[8] = {
		MKIT( -3, -3,  6,  6),
		MKIT(-16, -3, 32,  6),
		MKIT( -3, -3,  6,  6),
		MKIT( -3,-16,  6, 32),
		MKIT( -3, -3,  6,  6),
		MKIT(-16, -3, 32,  6),
		MKIT( -3, -3,  6,  6),
		MKIT( -3,-16,  6, 32),
	};
#undef MKIT
	uint32 x = _delta_xy_table[dir];
	v->x_offs = (byte)x;
	v->y_offs = (byte)(x>>=8);
	v->sprite_width = (byte)(x>>=8);
	v->sprite_height = (byte)(x>>=8);
}

static void RecalcShipStuff(Vehicle *v)
{
	UpdateShipDeltaXY(v, v->direction);
	v->cur_image = GetShipImage(v, v->direction);
	MarkShipDirty(v);
	InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
}

static const TileIndexDiff _ship_leave_depot_offs[] = {
	TILE_XY(-1,0),
	TILE_XY(0,-1),
};

static void CheckShipLeaveDepot(Vehicle *v)
{
	uint tile;
	int d;
	uint m;

	if (v->u.ship.state != 0x80)
		return;

	tile = v->tile;
	d = (_map5[tile]&2) ? 1 : 0;

	// Check first side
	if (_ship_sometracks[d] & GetTileShipTrackStatus(TILE_ADD(tile,_ship_leave_depot_offs[d]))) {
		m = (d==0) ? 0x101 : 0x207;
	// Check second side
	} else if (_ship_sometracks[d+2] & GetTileShipTrackStatus(TILE_ADD(tile, -2*_ship_leave_depot_offs[d]))) {
		m = (d==0) ? 0x105 : 0x203;
	} else {
		return;
	}
	v->direction = (byte)m;
	v->u.ship.state = (byte)(m >> 8);
	v->vehstatus &= ~VS_HIDDEN;

	v->cur_speed = 0;
	RecalcShipStuff(v);

	PlayShipSound(v);
}

static bool ShipAccelerate(Vehicle *v)
{
	uint spd;
	byte t;

	spd = min(v->cur_speed + 1, v->max_speed);

	//updates statusbar only if speed have changed to save CPU time
	if (spd != v->cur_speed) {
		v->cur_speed = spd;	
		if (_patches.vehicle_speed)
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
	}

	// Decrease somewhat when turning
	if (!(v->direction&1)) {
		spd = spd * 3 >> 2;
	}

	if (spd == 0)
		return false;

	if ((byte)++spd == 0)
		return true;

	v->progress = (t = v->progress) - (byte)spd;

	return (t < v->progress);
}


static int32 EstimateShipCost(uint16 engine_type);

static void ShipEnterDepot(Vehicle *v)
{
	byte t;

	v->u.ship.state = 0x80;
	v->vehstatus |= VS_HIDDEN;
	v->cur_speed = 0;
	RecalcShipStuff(v);
	
	v->date_of_last_service = _date;
	v->breakdowns_since_last_service = 0;
	v->reliability = _engines[v->engine_type].reliability;
	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	MaybeRenewVehicle(v, EstimateShipCost(v->engine_type));

	if ((v->next_order&OT_MASK) == OT_GOTO_DEPOT) {
		InvalidateWindow(WC_VEHICLE_VIEW, v->index);
	
		t = v->next_order;
		v->next_order = OT_DUMMY;

		if (t&OF_UNLOAD) { v->cur_order_index++; }

		else if (t & 0x40) {
			v->vehstatus |= VS_STOPPED;
			if (v->owner == _local_player) {
				SET_DPARAM16(0, v->unitnumber);
				AddNewsItem(
					STR_981C_SHIP_IS_WAITING_IN_DEPOT,
					NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0),
					v->index,
					0);
			}
		}
	}
}

static void ShipArrivesAt(Vehicle *v, Station *st)
{
	/* Check if station was ever visited before */
	if (!(st->had_vehicle_of_type & HVOT_SHIP)) {
		uint32 flags;
		st->had_vehicle_of_type |= HVOT_SHIP;
		SET_DPARAM16(0, st->index);
		flags = (v->owner == _local_player) ? NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_PLAYER, 0) : NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_OTHER, 0);
		AddNewsItem(
			STR_9833_CITIZENS_CELEBRATE_FIRST,
			flags,
			v->index,
			0);
	}
}

typedef struct {
	uint skiptile;
	uint dest_coords;
	uint best_bird_dist;
	uint best_length;
} PathFindShip;

//extern void dbg_store_path();

static bool ShipTrackFollower(uint tile, PathFindShip *pfs, int track, uint length, byte *state)
{
	// Found dest?
	if (tile == pfs->dest_coords) {
		pfs->best_bird_dist = 0;
		
//		if (length < pfs->best_length)
//			dbg_store_path();

		pfs->best_length = minu(pfs->best_length, length);
		return true;
	}

	// Skip this tile in the calculation
	if (tile != pfs->skiptile) {
		pfs->best_bird_dist = minu(pfs->best_bird_dist, GetTileDist1Db(pfs->dest_coords, tile));
	}
	
	return false;
}

static const byte _ship_search_directions[6][4] = {
	{ 0, 9, 2, 9 },
	{ 9, 1, 9, 3 },
	{ 9, 0, 3, 9 },
	{ 1, 9, 9, 2 },
	{ 3, 2, 9, 9 },
	{ 9, 9, 1, 0 },
};

static const byte _pick_shiptrack_table[6] = {1, 3, 2, 2, 0, 0};

static uint FindShipTrack(Vehicle *v, uint tile, int dir, uint bits, uint skiptile, int *track)
{
	PathFindShip pfs;
	int i, best_track;
	uint best_bird_dist = 0;
	uint best_length    = 0;
	uint r;
	byte ship_dir = v->direction & 3;

	pfs.dest_coords = v->dest_tile;
	pfs.skiptile = skiptile;

	best_track = -1;

	do {
		i = FIND_FIRST_BIT(bits);
		bits = KILL_FIRST_BIT(bits);
		
		pfs.best_bird_dist = (uint)-1;
		pfs.best_length = (uint)-1;

		FollowTrack(tile, 0x3800 | TRANSPORT_WATER, _ship_search_directions[i][dir], (TPFEnumProc*)ShipTrackFollower, NULL, &pfs);
		
		if (best_track >= 0) {
			if (pfs.best_bird_dist != 0) {
				/* neither reached the destination, pick the one with the smallest bird dist */
				if (pfs.best_bird_dist > best_bird_dist) goto bad;
				if (pfs.best_bird_dist < best_bird_dist) goto good;
			} else {
				if (pfs.best_length > best_length) goto bad;
				if (pfs.best_length < best_length) goto good;
			}
			
			/* if we reach this position, there's two paths of equal value so far. 
			 * pick one randomly. */
			r = (byte)Random();
			if (_pick_shiptrack_table[i] == ship_dir) r += 80;
			if (_pick_shiptrack_table[best_track] == ship_dir) r -= 80;
			if (r <= 127) goto bad;
		}
good:;
		best_track = i;
		best_bird_dist = pfs.best_bird_dist;
		best_length = pfs.best_length;
bad:;

	} while (bits != 0);

	*track = best_track;
	return best_bird_dist;
}

static int ChooseShipTrack(Vehicle *v, uint tile, int dir, uint tracks)
{
	uint b;
	uint tot_dist, dist;
	int track;
	uint tile2;

	assert(dir>=0 && dir<=3);
	tile2 = TILE_ADD(tile, -_tileoffs_by_dir[dir]);
	dir ^= 2;
	tot_dist = (uint)-1;
	b = GetTileShipTrackStatus(tile2) & _ship_sometracks[dir] & v->u.ship.state;
	if (b != 0) {
		dist = FindShipTrack(v, tile2, dir, b, tile, &track);
		if (dist != (uint)-1)
			tot_dist = dist + 1;
	}
	dist = FindShipTrack(v, tile, dir^2, tracks, 0, &track);
	if (dist > tot_dist)
		return -1;
	return track;
}

static const byte _new_vehicle_direction_table[11] = {
	0, 7, 6, 0,
	1, 0, 5, 0,
	2, 3, 4,
};

static int ShipGetNewDirectionFromTiles(uint new_tile, uint old_tile)
{
	uint offs = (GET_TILE_Y(new_tile) - GET_TILE_Y(old_tile) + 1) * 4 + 
							GET_TILE_X(new_tile) - GET_TILE_X(old_tile) + 1;
	assert(offs < 11 && offs != 3 && offs != 7);
	return _new_vehicle_direction_table[offs];
}

static int ShipGetNewDirection(Vehicle *v, int x, int y)
{
	uint offs = (y - v->y_pos + 1) * 4 + (x - v->x_pos + 1);
	assert(offs < 11 && offs != 3 && offs != 7);
	return _new_vehicle_direction_table[offs];
}

static int GetAvailShipTracks(uint tile, int dir)
{
	uint32 r = GetTileTrackStatus(tile, TRANSPORT_WATER);
	return (byte) ((r | r >> 8)) & _ship_sometracks[dir];
}

static const byte _ship_subcoord[4][6][3] = {
	{
		{15, 8, 1},
		{ 0, 0, 0},
		{ 0, 0, 0},
		{15, 8, 2},
		{15, 7, 0},
		{ 0, 0, 0},
	},
	{
		{ 0, 0, 0},
		{ 8, 0, 3},
		{ 7, 0, 2},
		{ 0, 0, 0},
		{ 8, 0, 4},
		{ 0, 0, 0},
	},
	{
		{ 0, 8, 5},
		{ 0, 0, 0},
		{ 0, 7, 6},
		{ 0, 0, 0},
		{ 0, 0, 0},
		{ 0, 8, 4},
	},
	{
		{ 0, 0, 0},
		{ 8,15, 7},
		{ 0, 0, 0},
		{ 8,15, 6},
		{ 0, 0, 0},
		{ 7,15, 0},
	}
};


static void ShipController(Vehicle *v)
{
	GetNewVehiclePosResult gp;
	uint32 r;
	const byte *b;
	int dir,track,tracks;

	v->tick_counter++;

	if (v->breakdown_ctr != 0) {
		if (v->breakdown_ctr <= 2) {
			HandleBrokenShip(v);
			return;
		}
		v->breakdown_ctr--;
	}

	if (v->vehstatus & VS_STOPPED)
		return;

	ProcessShipOrder(v);
	HandleShipLoading(v);

	if ((v->next_order & OT_MASK) == OT_LOADING)
		return;

	CheckShipLeaveDepot(v);

	if (!ShipAccelerate(v))
		return;

	BeginVehicleMove(v);

	if (GetNewVehiclePos(v, &gp)) {
		// staying in tile
		if (v->u.ship.state == 0x80) {
			gp.x = v->x_pos;
			gp.y = v->y_pos;
		} else {
			/* isnot inside depot */
			r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
			if (r & 0x8) goto reverse_direction;

			if (v->dest_tile != 0 && v->dest_tile == gp.new_tile) {
				if ((v->next_order & OT_MASK) == OT_GOTO_DEPOT) {
					if ((gp.x&0xF)==8 && (gp.y&0xF)==8) {
						ShipEnterDepot(v);
						return;
					}
				} else if ((v->next_order & OT_MASK) == OT_GOTO_STATION) {
					Station *st;

					v->last_station_visited = v->next_order_param;

					st = DEREF_STATION(v->next_order_param);
					if (!(st->had_vehicle_of_type & HVOT_BUOY)) {
						v->next_order = (v->next_order & (OF_FULL_LOAD|OF_UNLOAD)) | OF_NON_STOP | OT_LOADING;
						ShipArrivesAt(v, st);

						SET_EXPENSES_TYPE(EXPENSES_SHIP_INC);
						if (LoadUnloadVehicle(v)) {
							InvalidateWindow(WC_SHIPS_LIST, v->owner);
							MarkShipDirty(v);
						}
						InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
					} else {
						v->next_order = OT_LEAVESTATION;
						v->cur_order_index++;
						InvalidateVehicleOrderWidget(v);
					}
					goto else_end;
				}
			}

			if (v->next_order == OT_LEAVESTATION) {
				v->next_order = OT_NOTHING;
				InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
			}
		}
	} else {
		// new tile
		if (GET_TILE_X(gp.new_tile) == 0xFF ||
				(byte)GET_TILE_Y(gp.new_tile) == 0xFF)
					goto reverse_direction;

		dir = ShipGetNewDirectionFromTiles(gp.new_tile, gp.old_tile);
		assert(dir == 1 || dir == 3 || dir == 5 || dir == 7);
		dir>>=1;
		tracks = GetAvailShipTracks(gp.new_tile, dir);
		if (tracks == 0)
			goto reverse_direction;

		// Choose a direction, and continue if we find one
		track = ChooseShipTrack(v, gp.new_tile, dir, tracks);
		if (track < 0)
			goto reverse_direction;

		b = _ship_subcoord[dir][track];
	
		gp.x = (gp.x&~0xF) | b[0];
		gp.y = (gp.y&~0xF) | b[1];

		/* Call the landscape function and tell it that the vehicle entered the tile */
		r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
		if (r&0x8) goto reverse_direction;

		if (!(r&0x4)) {
			v->tile = gp.new_tile;
			v->u.ship.state = 1 << track;
		}

		v->direction = b[2];
	}
else_end:;

	/* update image of ship, as well as delta XY */
	dir = ShipGetNewDirection(v, gp.x, gp.y);
	v->x_pos = gp.x;
	v->y_pos = gp.y;
	v->z_pos = GetSlopeZ(gp.x, gp.y);

getout:
	UpdateShipDeltaXY(v, dir);
	v->cur_image = GetShipImage(v, dir);
	VehiclePositionChanged(v);
	EndVehicleMove(v);
	return;

reverse_direction:
	dir = v->direction ^ 4;
	v->direction = dir;
	goto getout;
}

static void AgeShipCargo(Vehicle *v)
{
	if (_age_cargo_skip_counter != 0)
		return;
	if (v->cargo_days != 255)
		v->cargo_days++;
}

void Ship_Tick(Vehicle *v)
{
	AgeShipCargo(v);
	ShipController(v);
}

void OnTick_Ship()
{
	// unused
}


void HandleClickOnShip(Vehicle *v)
{
	ShowShipViewWindow(v);
}

void ShipsYearlyLoop()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Ship) {
			v->profit_last_year = v->profit_this_year;
			v->profit_this_year = 0;
			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		}
	}
}

static int32 EstimateShipCost(uint16 engine_type)
{
	return ship_vehicle_info(engine_type).base_cost * (_price.ship_base>>3)>>5;
}

// p1 = type to build
int32 CmdBuildShip(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	int32 value;
	Vehicle *v;
	uint unit_num;
	uint tile = TILE_FROM_XY(x,y);
	Engine *e;
	
	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	value = EstimateShipCost(p1);
	if (flags & DC_QUERY_COST)
		return value;

	v = AllocateVehicle();
	if (v == NULL || _ptr_to_next_order >= endof(_order_array) || 
			(unit_num = GetFreeUnitNumber(VEH_Ship)) > _patches.max_ships)
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);
	
	if (flags & DC_EXEC) {
		v->unitnumber = unit_num;

		v->owner = _current_player;
		v->tile = tile;
		x = GET_TILE_X(tile)*16 + 8;
		y = GET_TILE_Y(tile)*16 + 8;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopeZ(x,y);

		v->z_height = 6;
		v->sprite_width = 6;
		v->sprite_height = 6;
		v->x_offs = -3;
		v->y_offs = -3;
		v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;
		
		v->spritenum = ship_vehicle_info(p1).image_index;
		v->cargo_type = ship_vehicle_info(p1).cargo_type;
		v->cargo_cap = ship_vehicle_info(p1).capacity;
		v->value = value;
		
		v->last_station_visited = 255;
		v->max_speed = ship_vehicle_info(p1).max_speed;
		v->engine_type = (byte)p1;

		e = &_engines[p1];
		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->lifelength * 366;
		_new_ship_id = v->index;

		v->string_id = STR_SV_SHIP_NAME;
		v->u.ship.state = 0x80;
		*(v->schedule_ptr = _ptr_to_next_order++) = 0;

		v->service_interval = _patches.servint_ships;
		v->date_of_last_service = _date;
		v->build_year = _cur_year;
		v->cur_image = 0x0E5E;
		v->type = VEH_Ship;

		VehiclePositionChanged(v);

		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		_vehicle_sort_dirty[VEHSHIP] = true; // build a ship
		InvalidateWindow(WC_SHIPS_LIST, v->owner);
		InvalidateWindow(WC_COMPANY, v->owner);
	}
	
	return value;
}

int32 CmdSellShip(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
	
	v = &_vehicles[p1];

	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	if (!IsShipDepotTile(v->tile) || v->u.road.state != 0x80 || !(v->vehstatus&VS_STOPPED))
		return_cmd_error(STR_980B_SHIP_MUST_BE_STOPPED_IN);
	
	if (flags & DC_EXEC) {
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		_vehicle_sort_dirty[VEHSHIP] = true; // sell a ship
		InvalidateWindow(WC_SHIPS_LIST, v->owner);
		InvalidateWindow(WC_COMPANY, v->owner);
		DeleteWindowById(WC_VEHICLE_VIEW, v->index);
		DeleteVehicle(v);
	}
	
	return -(int32)v->value;
}

// p1 = vehicle
int32 CmdStartStopShip(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	v = &_vehicles[p1];

	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->vehstatus ^= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}

	return 0;
}

int32 CmdSendShipToDepot(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	int depot;

	v = &_vehicles[p1];

	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	if ((v->next_order&OT_MASK) == OT_GOTO_DEPOT) {
		if (flags & DC_EXEC) {
			if (v->next_order&OF_UNLOAD) {v->cur_order_index++;}
			v->next_order = OT_DUMMY;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);			
		}
	} else {
		depot = FindClosestShipDepot(v);
		if (depot < 0)
			return_cmd_error(STR_981A_UNABLE_TO_FIND_LOCAL_DEPOT);

		if (flags & DC_EXEC) {
			v->dest_tile = _depots[depot].xy;
			v->next_order = OF_NON_STOP | OF_FULL_LOAD | OT_GOTO_DEPOT;
			v->next_order_param = depot;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
		}
	}

	return 0;
}

int32 CmdChangeShipServiceInt(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	v = &_vehicles[p1];

	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->service_interval = (uint16)p2;
		InvalidateWindowWidget(WC_VEHICLE_DETAILS, v->index, 7);
	}

	return 0;
}


// p1 = vehicle
// p2 = new cargo
int32 CmdRefitShip(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	int32 cost;

	SET_EXPENSES_TYPE(EXPENSES_SHIP_RUN);
	
	v = &_vehicles[p1];
	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	if (!IsShipDepotTile(v->tile) ||
			!(v->vehstatus&VS_STOPPED) ||
			v->u.ship.state != 0x80)
		return_cmd_error(STR_980B_SHIP_MUST_BE_STOPPED_IN);

	cost = 0;
	if (IS_HUMAN_PLAYER(v->owner) && (byte)p2 != v->cargo_type) {
		cost = _price.ship_base >> 7;
	}

	if (flags & DC_EXEC) {
		v->cargo_count = 0;
		v->cargo_type = (byte)p2;
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	}

	return cost;

}


