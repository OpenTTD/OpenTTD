/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "functions.h"
#include "map.h"
#include "tile.h"
#include "vehicle.h"
#include "command.h"
#include "pathfind.h"
#include "station.h"
#include "news.h"
#include "engine.h"
#include "player.h"
#include "sound.h"
#include "npf.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"

static const uint16 _ship_sprites[] = {0x0E5D, 0x0E55, 0x0E65, 0x0E6D};
static const byte _ship_sometracks[4] = {0x19, 0x16, 0x25, 0x2A};

static byte GetTileShipTrackStatus(TileIndex tile)
{
	uint32 r = GetTileTrackStatus(tile, TRANSPORT_WATER);
	return r | r >> 8;
}

void DrawShipEngine(int x, int y, EngineID engine, uint32 image_ormod)
{
	int spritenum = ShipVehInfo(engine)->image_index;

	if (is_custom_sprite(spritenum)) {
		int sprite = GetCustomVehicleIcon(engine, 6);

		if (sprite != 0) {
			DrawSprite(sprite | image_ormod, x, y);
			return;
		}
		spritenum = orig_ship_vehicle_info[engine - SHIP_ENGINES_INDEX].image_index;
	}
	DrawSprite((6 + _ship_sprites[spritenum]) | image_ormod, x, y);
}

int GetShipImage(const Vehicle *v, byte direction)
{
	int spritenum = v->spritenum;

	if (is_custom_sprite(spritenum)) {
		int sprite = GetCustomVehicleSprite(v, direction);

		if (sprite != 0) return sprite;
		spritenum = orig_ship_vehicle_info[v->engine_type - SHIP_ENGINES_INDEX].image_index;
	}
	return _ship_sprites[spritenum] + direction;
}

static const Depot* FindClosestShipDepot(const Vehicle* v)
{
	const Depot* depot;
	const Depot* best_depot = NULL;
	uint dist;
	uint best_dist = (uint)-1;
	TileIndex tile;
	TileIndex tile2 = v->tile;

	if (_patches.new_pathfinding_all) {
		NPFFoundTargetData ftd;
		byte trackdir = GetVehicleTrackdir(v);
		ftd = NPFRouteToDepotTrialError(v->tile, trackdir, TRANSPORT_WATER, v->owner, INVALID_RAILTYPE);
		if (ftd.best_bird_dist == 0) {
			best_depot = GetDepotByTile(ftd.node.tile); /* Found target */
		} else {
			best_depot = NULL; /* Did not find target */
		}
	} else {
		FOR_ALL_DEPOTS(depot) {
			tile = depot->xy;
			if (IsValidDepot(depot) && IsTileDepotType(tile, TRANSPORT_WATER) && IsTileOwner(tile, v->owner)) {
				dist = DistanceManhattan(tile, tile2);
				if (dist < best_dist) {
					best_dist = dist;
					best_depot = depot;
				}
			}
		}
	}
	return best_depot;
}

static void CheckIfShipNeedsService(Vehicle *v)
{
	const Depot* depot;

	if (_patches.servint_ships == 0) return;
	if (!VehicleNeedsService(v))     return;
	if (v->vehstatus & VS_STOPPED)   return;

	if (v->current_order.type == OT_GOTO_DEPOT &&
			v->current_order.flags & OF_HALT_IN_DEPOT)
		return;

	if (_patches.gotodepot && VehicleHasDepotOrders(v)) return;

	depot = FindClosestShipDepot(v);

	if (depot == NULL || DistanceManhattan(v->tile, depot->xy) > 12) {
		if (v->current_order.type == OT_GOTO_DEPOT) {
			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		}
		return;
	}

	v->current_order.type = OT_GOTO_DEPOT;
	v->current_order.flags = OF_NON_STOP;
	v->current_order.station = depot->index;
	v->dest_tile = depot->xy;
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
}

void OnNewDay_Ship(Vehicle *v)
{
	int32 cost;

	if ((++v->day_counter & 7) == 0)
		DecreaseVehicleValue(v);

	CheckVehicleBreakdown(v);
	AgeVehicle(v);
	CheckIfShipNeedsService(v);

	CheckOrders(v->index, OC_INIT);

	if (v->vehstatus & VS_STOPPED) return;

	cost = ShipVehInfo(v->engine_type)->running_cost * _price.ship_running / 364;
	v->profit_this_year -= cost >> 8;

	SET_EXPENSES_TYPE(EXPENSES_SHIP_RUN);
	SubtractMoneyFromPlayerFract(v->owner, cost);

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	//we need this for the profit
	InvalidateWindowClasses(WC_SHIPS_LIST);
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

		SndPlayVehicleFx((_opt.landscape != LT_CANDY) ?
			SND_10_TRAIN_BREAKDOWN : SND_3A_COMEDY_BREAKDOWN_2, v);

		if (!(v->vehstatus & VS_HIDDEN)) {
			Vehicle *u = CreateEffectVehicleRel(v, 4, 4, 5, EV_BREAKDOWN_SMOKE);
			if (u != NULL) u->u.special.unk0 = v->breakdown_delay * 2;
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
	SndPlayVehicleFx(ShipVehInfo(v->engine_type)->sfx, v);
}

static const TileIndexDiffC _dock_offs[] = {
	{ 2,  0},
	{-2,  0},
	{ 0,  2},
	{ 2,  0},
	{ 0, -2},
	{ 0,  0},
	{ 0,  0},
	{ 0,  0}
};

static void ProcessShipOrder(Vehicle *v)
{
	const Order *order;

	switch (v->current_order.type) {
		case OT_GOTO_DEPOT:
			if (!(v->current_order.flags & OF_PART_OF_ORDERS)) return;
			if (v->current_order.flags & OF_SERVICE_IF_NEEDED &&
					!VehicleNeedsService(v)) {
				v->cur_order_index++;
			}
			break;

		case OT_LOADING:
		case OT_LEAVESTATION:
			return;
	}

	if (v->cur_order_index >= v->num_orders) v->cur_order_index = 0;

	order = GetVehicleOrder(v, v->cur_order_index);

	if (order == NULL) {
		v->current_order.type  = OT_NOTHING;
		v->current_order.flags = 0;
		v->dest_tile = 0;
		return;
	}

	if (order->type    == v->current_order.type &&
			order->flags   == v->current_order.flags &&
			order->station == v->current_order.station)
		return;

	v->current_order = *order;

	if (order->type == OT_GOTO_STATION) {
		const Station *st;

		if (order->station == v->last_station_visited)
			v->last_station_visited = INVALID_STATION;

		st = GetStation(order->station);
		if (st->dock_tile != 0) {
			v->dest_tile = TILE_ADD(st->dock_tile, ToTileIndexDiff(_dock_offs[_m[st->dock_tile].m5-0x4B]));
		}
	} else if (order->type == OT_GOTO_DEPOT) {
		v->dest_tile = GetDepot(order->station)->xy;
	} else {
		v->dest_tile = 0;
	}

	InvalidateVehicleOrder(v);

	InvalidateWindowClasses(WC_SHIPS_LIST);
}

static void HandleShipLoading(Vehicle *v)
{
	if (v->current_order.type == OT_NOTHING) return;

	if (v->current_order.type != OT_DUMMY) {
		if (v->current_order.type != OT_LOADING) return;
		if (--v->load_unload_time_rem) return;

		if (v->current_order.flags & OF_FULL_LOAD && CanFillVehicle(v)) {
			SET_EXPENSES_TYPE(EXPENSES_SHIP_INC);
			if (LoadUnloadVehicle(v)) {
				InvalidateWindow(WC_SHIPS_LIST, v->owner);
				MarkShipDirty(v);
			}
			return;
		}
		PlayShipSound(v);

		{
			Order b = v->current_order;
			v->current_order.type = OT_LEAVESTATION;
			v->current_order.flags = 0;
			if (!(b.flags & OF_NON_STOP)) return;
		}
	}

	v->cur_order_index++;
	InvalidateVehicleOrder(v);
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
	v->x_offs        = GB(x,  0, 8);
	v->y_offs        = GB(x,  8, 8);
	v->sprite_width  = GB(x, 16, 8);
	v->sprite_height = GB(x, 24, 8);
}

static void RecalcShipStuff(Vehicle *v)
{
	UpdateShipDeltaXY(v, v->direction);
	v->cur_image = GetShipImage(v, v->direction);
	MarkShipDirty(v);
	InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
}

static const TileIndexDiffC _ship_leave_depot_offs[] = {
	{-1,  0},
	{ 0, -1}
};

static void CheckShipLeaveDepot(Vehicle *v)
{
	TileIndex tile;
	int d;
	uint m;

	if (v->u.ship.state != 0x80) return;

	tile = v->tile;
	d = (_m[tile].m5&2) ? 1 : 0;

	// Check first side
	if (_ship_sometracks[d] & GetTileShipTrackStatus(TILE_ADD(tile, ToTileIndexDiff(_ship_leave_depot_offs[d])))) {
		m = (d==0) ? 0x101 : 0x207;
	// Check second side
	} else if (_ship_sometracks[d+2] & GetTileShipTrackStatus(TILE_ADD(tile, -2 * ToTileIndexDiff(_ship_leave_depot_offs[d])))) {
		m = (d==0) ? 0x105 : 0x203;
	} else {
		return;
	}
	v->direction    = GB(m, 0, 8);
	v->u.ship.state = GB(m, 8, 8);
	v->vehstatus &= ~VS_HIDDEN;

	v->cur_speed = 0;
	RecalcShipStuff(v);

	PlayShipSound(v);
	VehicleServiceInDepot(v);
	InvalidateWindowClasses(WC_SHIPS_LIST);
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
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	}

	// Decrease somewhat when turning
	if (!(v->direction & 1)) spd = spd * 3 / 4;

	if (spd == 0) return false;
	if ((byte)++spd == 0) return true;

	v->progress = (t = v->progress) - (byte)spd;

	return (t < v->progress);
}

static int32 EstimateShipCost(EngineID engine_type)
{
	return ShipVehInfo(engine_type)->base_cost * (_price.ship_base>>3)>>5;
}

static void ShipEnterDepot(Vehicle *v)
{
	v->u.ship.state = 0x80;
	v->vehstatus |= VS_HIDDEN;
	v->cur_speed = 0;
	RecalcShipStuff(v);

	VehicleServiceInDepot(v);

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	TriggerVehicle(v, VEHICLE_TRIGGER_DEPOT);

	if (v->current_order.type == OT_GOTO_DEPOT) {
		Order t;

		InvalidateWindow(WC_VEHICLE_VIEW, v->index);

		t = v->current_order;
		v->current_order.type = OT_DUMMY;
		v->current_order.flags = 0;

		if (HASBIT(t.flags, OFB_PART_OF_ORDERS)) {
			v->cur_order_index++;
		} else if (HASBIT(t.flags, OFB_HALT_IN_DEPOT)) {
			v->vehstatus |= VS_STOPPED;
			if (v->owner == _local_player) {
				SetDParam(0, v->unitnumber);
				AddNewsItem(
					STR_981C_SHIP_IS_WAITING_IN_DEPOT,
					NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0),
					v->index,
					0);
			}
		}
	}
	InvalidateWindowClasses(WC_SHIPS_LIST);
}

static void ShipArrivesAt(const Vehicle* v, Station* st)
{
	/* Check if station was ever visited before */
	if (!(st->had_vehicle_of_type & HVOT_SHIP)) {
		uint32 flags;

		st->had_vehicle_of_type |= HVOT_SHIP;

		SetDParam(0, st->index);
		flags = (v->owner == _local_player) ? NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_PLAYER, 0) : NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_OTHER, 0);
		AddNewsItem(
			STR_9833_CITIZENS_CELEBRATE_FIRST,
			flags,
			v->index,
			0);
	}
}

typedef struct {
	TileIndex skiptile;
	TileIndex dest_coords;
	uint best_bird_dist;
	uint best_length;
} PathFindShip;

static bool ShipTrackFollower(TileIndex tile, PathFindShip *pfs, int track, uint length, byte *state)
{
	// Found dest?
	if (tile == pfs->dest_coords) {
		pfs->best_bird_dist = 0;

		pfs->best_length = minu(pfs->best_length, length);
		return true;
	}

	// Skip this tile in the calculation
	if (tile != pfs->skiptile) {
		pfs->best_bird_dist = minu(pfs->best_bird_dist, DistanceMaxPlusManhattan(pfs->dest_coords, tile));
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

static uint FindShipTrack(Vehicle *v, TileIndex tile, int dir, uint bits, TileIndex skiptile, int *track)
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
			r = GB(Random(), 0, 8);
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

/* returns the track to choose on the next tile, or -1 when it's better to
 * reverse. The tile given is the tile we are about to enter, enterdir is the
 * direction in which we are entering the tile */
static int ChooseShipTrack(Vehicle *v, TileIndex tile, int enterdir, uint tracks)
{
	assert(enterdir>=0 && enterdir<=3);

	if (_patches.new_pathfinding_all) {
		NPFFindStationOrTileData fstd;
		NPFFoundTargetData ftd;
		TileIndex src_tile = TILE_ADD(tile, TileOffsByDir(ReverseDiagdir(enterdir)));
		byte trackdir = GetVehicleTrackdir(v);
		assert (trackdir != 0xFF); /* Check that we are not in a depot */

		NPFFillWithOrderData(&fstd, v);

		ftd = NPFRouteToStationOrTile(src_tile, trackdir, &fstd, TRANSPORT_WATER, v->owner, INVALID_RAILTYPE);

		if (ftd.best_trackdir != 0xff) {
			/* If ftd.best_bird_dist is 0, we found our target and ftd.best_trackdir contains
			the direction we need to take to get there, if ftd.best_bird_dist is not 0,
			we did not find our target, but ftd.best_trackdir contains the direction leading
			to the tile closest to our target. */
			return ftd.best_trackdir & 7; /* TODO: Wrapper function? */
		} else {
			return -1; /* Already at target, reverse? */
		}
	} else {
		uint b;
		uint tot_dist, dist;
		int track;
		TileIndex tile2;

		tile2 = TILE_ADD(tile, -TileOffsByDir(enterdir));
		tot_dist = (uint)-1;

		/* Let's find out how far it would be if we would reverse first */
		b = GetTileShipTrackStatus(tile2) & _ship_sometracks[ReverseDiagdir(enterdir)] & v->u.ship.state;
		if (b != 0) {
			dist = FindShipTrack(v, tile2, ReverseDiagdir(enterdir), b, tile, &track);
			if (dist != (uint)-1)
				tot_dist = dist + 1;
		}
		/* And if we would not reverse? */
		dist = FindShipTrack(v, tile, enterdir, tracks, 0, &track);
		if (dist > tot_dist)
			/* We could better reverse */
			return -1;
		return track;
	}
}

static const byte _new_vehicle_direction_table[11] = {
	0, 7, 6, 0,
	1, 0, 5, 0,
	2, 3, 4,
};

static int ShipGetNewDirectionFromTiles(TileIndex new_tile, TileIndex old_tile)
{
	uint offs = (TileY(new_tile) - TileY(old_tile) + 1) * 4 +
							TileX(new_tile) - TileX(old_tile) + 1;
	assert(offs < 11 && offs != 3 && offs != 7);
	return _new_vehicle_direction_table[offs];
}

static int ShipGetNewDirection(Vehicle *v, int x, int y)
{
	uint offs = (y - v->y_pos + 1) * 4 + (x - v->x_pos + 1);
	assert(offs < 11 && offs != 3 && offs != 7);
	return _new_vehicle_direction_table[offs];
}

static int GetAvailShipTracks(TileIndex tile, int dir)
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

	if (v->vehstatus & VS_STOPPED) return;

	ProcessShipOrder(v);
	HandleShipLoading(v);

	if (v->current_order.type == OT_LOADING) return;

	CheckShipLeaveDepot(v);

	if (!ShipAccelerate(v)) return;

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

			/* A leave station order only needs one tick to get processed, so we can
			 * always skip ahead. */
			if (v->current_order.type == OT_LEAVESTATION) {
				v->current_order.type = OT_NOTHING;
				v->current_order.flags = 0;
				InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
			} else if (v->dest_tile != 0) {
				/* We have a target, let's see if we reached it... */
				if (v->current_order.type == OT_GOTO_STATION &&
						IsBuoyTile(v->dest_tile) &&
						DistanceManhattan(v->dest_tile, gp.new_tile) <= 3) {
					/* We got within 3 tiles of our target buoy, so let's skip to our
					 * next order */
					v->cur_order_index++;
					v->current_order.type = OT_DUMMY;
					InvalidateVehicleOrder(v);
				} else {
					/* Non-buoy orders really need to reach the tile */
					if (v->dest_tile == gp.new_tile) {
						if (v->current_order.type == OT_GOTO_DEPOT) {
							if ((gp.x&0xF)==8 && (gp.y&0xF)==8) {
								ShipEnterDepot(v);
								return;
							}
						} else if (v->current_order.type == OT_GOTO_STATION) {
							Station *st;

							v->last_station_visited = v->current_order.station;

							/* Process station in the orderlist. */
							st = GetStation(v->current_order.station);
							if (st->facilities & FACIL_DOCK) { /* ugly, ugly workaround for problem with ships able to drop off cargo at wrong stations */
								v->current_order.type = OT_LOADING;
								v->current_order.flags &= OF_FULL_LOAD | OF_UNLOAD;
								v->current_order.flags |= OF_NON_STOP;
								ShipArrivesAt(v, st);

								SET_EXPENSES_TYPE(EXPENSES_SHIP_INC);
								if (LoadUnloadVehicle(v)) {
									InvalidateWindow(WC_SHIPS_LIST, v->owner);
									MarkShipDirty(v);
								}
								InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
							} else { /* leave stations without docks right aways */
								v->current_order.type = OT_LEAVESTATION;
								v->current_order.flags = 0;
								v->cur_order_index++;
								InvalidateVehicleOrder(v);
							}
						}
					}
				}
			}
		}
	} else {
		// new tile
		if (TileX(gp.new_tile) >= MapMaxX() || TileY(gp.new_tile) >= MapMaxY())
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
	if (_age_cargo_skip_counter != 0) return;
	if (v->cargo_days != 255) v->cargo_days++;
}

void Ship_Tick(Vehicle *v)
{
	AgeShipCargo(v);
	ShipController(v);
}


void ShipsYearlyLoop(void)
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

/** Build a ship.
 * @param x,y tile coordinates of depot where ship is built
 * @param p1 ship type being built (engine)
 * @param p2 unused
 */
int32 CmdBuildShip(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	int32 value;
	Vehicle *v;
	UnitID unit_num;
	TileIndex tile = TileVirtXY(x, y);
	Engine *e;

	if (!IsEngineBuildable(p1, VEH_Ship)) return_cmd_error(STR_ENGINE_NOT_BUILDABLE);

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	value = EstimateShipCost(p1);
	if (flags & DC_QUERY_COST) return value;

	/* The ai_new queries the vehicle cost before building the route,
	 * so we must check against cheaters no sooner than now. --pasky */
	if (!IsTileDepotType(tile, TRANSPORT_WATER)) return CMD_ERROR;
	if (!IsTileOwner(tile, _current_player)) return CMD_ERROR;

	v = AllocateVehicle();
	if (v == NULL || IsOrderPoolFull() ||
			(unit_num = GetFreeUnitNumber(VEH_Ship)) > _patches.max_ships)
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

	if (flags & DC_EXEC) {
		const ShipVehicleInfo *svi = ShipVehInfo(p1);

		v->unitnumber = unit_num;

		v->owner = _current_player;
		v->tile = tile;
		x = TileX(tile) * 16 + 8;
		y = TileY(tile) * 16 + 8;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopeZ(x,y);

		v->z_height = 6;
		v->sprite_width = 6;
		v->sprite_height = 6;
		v->x_offs = -3;
		v->y_offs = -3;
		v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;

		v->spritenum = svi->image_index;
		v->cargo_type = svi->cargo_type;
		v->cargo_cap = svi->capacity;
		v->value = value;

		v->last_station_visited = INVALID_STATION;
		v->max_speed = svi->max_speed;
		v->engine_type = p1;

		e = GetEngine(p1);
		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->lifelength * 366;
		_new_ship_id = v->index;
		_new_vehicle_id = v->index;

		v->string_id = STR_SV_SHIP_NAME;
		v->u.ship.state = 0x80;

		v->service_interval = _patches.servint_ships;
		v->date_of_last_service = _date;
		v->build_year = _cur_year;
		v->cur_image = 0x0E5E;
		v->type = VEH_Ship;
		v->random_bits = VehicleRandomBits();

		VehiclePositionChanged(v);

		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
		InvalidateWindow(WC_COMPANY, v->owner);
		if (IsLocalPlayer())
			InvalidateWindow(WC_REPLACE_VEHICLE, VEH_Ship); // updates the replace Ship window
	}

	return value;
}

/** Sell a ship.
 * @param x,y unused
 * @param p1 vehicle ID to be sold
 * @param p2 unused
 */
int32 CmdSellShip(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Ship || !CheckOwnership(v->owner)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	if (!IsTileDepotType(v->tile, TRANSPORT_WATER) || v->u.road.state != 0x80 || !(v->vehstatus&VS_STOPPED))
		return_cmd_error(STR_980B_SHIP_MUST_BE_STOPPED_IN);

	if (flags & DC_EXEC) {
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
		InvalidateWindow(WC_COMPANY, v->owner);
		DeleteWindowById(WC_VEHICLE_VIEW, v->index);
		DeleteVehicle(v);
		if (IsLocalPlayer())
			InvalidateWindow(WC_REPLACE_VEHICLE, VEH_Ship); // updates the replace Ship window
	}

	return -(int32)v->value;
}

/** Start/Stop a ship.
 * @param x,y unused
 * @param p1 ship ID to start/stop
 * @param p2 unused
 */
int32 CmdStartStopShip(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Ship || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->vehstatus ^= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		InvalidateWindowClasses(WC_SHIPS_LIST);
	}

	return 0;
}

/** Send a ship to the depot.
 * @param x,y unused
 * @param p1 vehicle ID to send to the depot
 * @param p2 unused
 */
int32 CmdSendShipToDepot(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	const Depot *dep;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Ship || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->vehstatus & VS_CRASHED) return CMD_ERROR;

	/* If the current orders are already goto-depot */
	if (v->current_order.type == OT_GOTO_DEPOT) {
		if (flags & DC_EXEC) {
			/* If the orders to 'goto depot' are in the orders list (forced servicing),
			 * then skip to the next order; effectively cancelling this forced service */
			if (HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS))
				v->cur_order_index++;

			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		}
		return 0;
	}

	dep = FindClosestShipDepot(v);
	if (dep == NULL)
		return_cmd_error(STR_981A_UNABLE_TO_FIND_LOCAL_DEPOT);

	if (flags & DC_EXEC) {
		v->dest_tile = dep->xy;
		v->current_order.type = OT_GOTO_DEPOT;
		v->current_order.flags = OF_NON_STOP | OF_HALT_IN_DEPOT;
		v->current_order.station = dep->index;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	}

	return 0;
}


/** Refits a ship to the specified cargo type.
 * @param x,y unused
 * @param p1 vehicle ID of the ship to refit
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0-7) - the new cargo type to refit to (p2 & 0xFF)
 */
int32 CmdRefitShip(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	int32 cost;
	CargoID new_cid = p2 & 0xFF; //gets the cargo number

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Ship || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (!IsTileDepotType(v->tile, TRANSPORT_WATER) || !(v->vehstatus&VS_STOPPED) || v->u.ship.state != 0x80)
			return_cmd_error(STR_980B_SHIP_MUST_BE_STOPPED_IN);


	/* Check cargo */
	if (!ShipVehInfo(v->engine_type)->refittable) return CMD_ERROR;
	if (new_cid > NUM_CARGO || !CanRefitTo(v->engine_type, new_cid)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_SHIP_RUN);

	cost = 0;
	if (IS_HUMAN_PLAYER(v->owner) && new_cid != v->cargo_type) {
		cost = _price.ship_base >> 7;
	}

	if (flags & DC_EXEC) {
		v->cargo_count = 0;
		v->cargo_type = new_cid;
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	}

	return cost;

}
