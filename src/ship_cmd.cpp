/* $Id$ */

/** @file ship_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "ship.h"
#include "tile_cmd.h"
#include "landscape.h"
#include "timetable.h"
#include "command_func.h"
#include "pathfind.h"
#include "station_map.h"
#include "station.h"
#include "news.h"
#include "engine.h"
#include "player_func.h"
#include "player_base.h"
#include "npf.h"
#include "depot.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "water_map.h"
#include "yapf/yapf.h"
#include "debug.h"
#include "newgrf_callbacks.h"
#include "newgrf_text.h"
#include "newgrf_sound.h"
#include "spritecache.h"
#include "misc/autoptr.hpp"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "variables.h"
#include "autoreplace_gui.h"
#include "gfx_func.h"
#include "settings_type.h"

#include "table/strings.h"

static const uint16 _ship_sprites[] = {0x0E5D, 0x0E55, 0x0E65, 0x0E6D};

static const TrackBits _ship_sometracks[4] = {
	TRACK_BIT_X | TRACK_BIT_LOWER | TRACK_BIT_LEFT,  // 0x19, // DIAGDIR_NE
	TRACK_BIT_Y | TRACK_BIT_UPPER | TRACK_BIT_LEFT,  // 0x16, // DIAGDIR_SE
	TRACK_BIT_X | TRACK_BIT_UPPER | TRACK_BIT_RIGHT, // 0x25, // DIAGDIR_SW
	TRACK_BIT_Y | TRACK_BIT_LOWER | TRACK_BIT_RIGHT, // 0x2A, // DIAGDIR_NW
};

static TrackBits GetTileShipTrackStatus(TileIndex tile)
{
	uint32 r = GetTileTrackStatus(tile, TRANSPORT_WATER, 0);
	return TrackdirBitsToTrackBits((TrackdirBits)(TRACKDIR_BIT_MASK & (r | r >> 8)));
}

void DrawShipEngine(int x, int y, EngineID engine, SpriteID pal)
{
	int spritenum = ShipVehInfo(engine)->image_index;

	if (is_custom_sprite(spritenum)) {
		int sprite = GetCustomVehicleIcon(engine, DIR_W);

		if (sprite != 0) {
			DrawSprite(sprite, pal, x, y);
			return;
		}
		spritenum = _orig_ship_vehicle_info[engine - SHIP_ENGINES_INDEX].image_index;
	}
	DrawSprite(6 + _ship_sprites[spritenum], pal, x, y);
}

/** Get the size of the sprite of a ship sprite heading west (used for lists)
 * @param engine The engine to get the sprite from
 * @param width The width of the sprite
 * @param height The height of the sprite
 */
void GetShipSpriteSize(EngineID engine, uint &width, uint &height)
{
	SpriteID spritenum = ShipVehInfo(engine)->image_index;
	SpriteID custom_sprite = 0;

	if (is_custom_sprite(spritenum)) {
		custom_sprite = GetCustomVehicleIcon(engine, DIR_W);
		spritenum = _orig_ship_vehicle_info[engine - SHIP_ENGINES_INDEX].image_index;
	}
	if (custom_sprite == 0) {
		spritenum = 6 + _ship_sprites[spritenum];
	} else {
		spritenum = custom_sprite;
	}

	const Sprite *spr = GetSprite(spritenum);

	width  = spr->width;
	height = spr->height;
}

int Ship::GetImage(Direction direction) const
{
	int spritenum = this->spritenum;

	if (is_custom_sprite(spritenum)) {
		int sprite = GetCustomVehicleSprite(this, direction);

		if (sprite != 0) return sprite;
		spritenum = _orig_ship_vehicle_info[this->engine_type - SHIP_ENGINES_INDEX].image_index;
	}
	return _ship_sprites[spritenum] + direction;
}

static const Depot* FindClosestShipDepot(const Vehicle* v)
{
	if (_patches.pathfinder_for_ships == VPF_NPF) { /* NPF is used */
		Trackdir trackdir = GetVehicleTrackdir(v);
		NPFFoundTargetData ftd = NPFRouteToDepotTrialError(v->tile, trackdir, false, TRANSPORT_WATER, 0, v->owner, INVALID_RAILTYPES);

		if (ftd.best_bird_dist == 0) return GetDepotByTile(ftd.node.tile); /* Found target */

		return NULL; /* Did not find target */
	}

	/* OPF or YAPF - find the closest depot */

	const Depot* depot;
	const Depot* best_depot = NULL;
	uint best_dist = UINT_MAX;

	FOR_ALL_DEPOTS(depot) {
		TileIndex tile = depot->xy;
		if (IsTileDepotType(tile, TRANSPORT_WATER) && IsTileOwner(tile, v->owner)) {
			uint dist = DistanceManhattan(tile, v->tile);
			if (dist < best_dist) {
				best_dist = dist;
				best_depot = depot;
			}
		}
	}

	return best_depot;
}

static void CheckIfShipNeedsService(Vehicle *v)
{
	if (_patches.servint_ships == 0 || !VehicleNeedsService(v)) return;
	if (v->IsInDepot()) {
		VehicleServiceInDepot(v);
		return;
	}

	const Depot *depot = FindClosestShipDepot(v);

	if (depot == NULL || DistanceManhattan(v->tile, depot->xy) > 12) {
		if (v->current_order.type == OT_GOTO_DEPOT) {
			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		}
		return;
	}

	v->current_order.type = OT_GOTO_DEPOT;
	v->current_order.flags = OFB_NON_STOP;
	v->current_order.dest = depot->index;
	v->dest_tile = depot->xy;
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
}

void Ship::OnNewDay()
{
	if ((++this->day_counter & 7) == 0)
		DecreaseVehicleValue(this);

	CheckVehicleBreakdown(this);
	AgeVehicle(this);
	CheckIfShipNeedsService(this);

	CheckOrders(this);

	if (this->running_ticks == 0) return;

	CommandCost cost(EXPENSES_SHIP_RUN, GetVehicleProperty(this, 0x0F, ShipVehInfo(this->engine_type)->running_cost) * _price.ship_running * this->running_ticks / (364 * DAY_TICKS));

	this->profit_this_year -= cost.GetCost();
	this->running_ticks = 0;

	SubtractMoneyFromPlayerFract(this->owner, cost);

	InvalidateWindow(WC_VEHICLE_DETAILS, this->index);
	/* we need this for the profit */
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

		if (!PlayVehicleSound(v, VSE_BREAKDOWN)) {
			SndPlayVehicleFx((_opt.landscape != LT_TOYLAND) ?
				SND_10_TRAIN_BREAKDOWN : SND_3A_COMEDY_BREAKDOWN_2, v);
		}

		if (!(v->vehstatus & VS_HIDDEN)) {
			Vehicle *u = CreateEffectVehicleRel(v, 4, 4, 5, EV_BREAKDOWN_SMOKE);
			if (u != NULL) u->u.special.animation_state = v->breakdown_delay * 2;
		}
	}

	if (!(v->tick_counter & 1)) {
		if (!--v->breakdown_delay) {
			v->breakdown_ctr = 0;
			InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		}
	}
}

void Ship::MarkDirty()
{
	this->cur_image = this->GetImage(this->direction);
	MarkSingleVehicleDirty(this);
}

static void PlayShipSound(const Vehicle *v)
{
	if (!PlayVehicleSound(v, VSE_START)) {
		SndPlayVehicleFx(ShipVehInfo(v->engine_type)->sfx, v);
	}
}

void Ship::PlayLeaveStationSound() const
{
	PlayShipSound(this);
}

static void ProcessShipOrder(Vehicle *v)
{
	const Order *order;

	switch (v->current_order.type) {
		case OT_GOTO_DEPOT:
			if (!(v->current_order.flags & OFB_PART_OF_ORDERS)) return;
			if (v->current_order.flags & OFB_SERVICE_IF_NEEDED &&
					!VehicleNeedsService(v)) {
				UpdateVehicleTimetable(v, true);
				v->cur_order_index++;
			}
			break;

		case OT_LOADING:
		case OT_LEAVESTATION:
			return;

		default: break;
	}

	if (v->cur_order_index >= v->num_orders) v->cur_order_index = 0;

	order = GetVehicleOrder(v, v->cur_order_index);

	if (order == NULL) {
		v->current_order.Free();
		v->dest_tile = 0;
		return;
	}

	if (order->type  == v->current_order.type &&
			order->flags == v->current_order.flags &&
			order->dest  == v->current_order.dest &&
			(order->type != OT_GOTO_STATION || GetStation(order->dest)->dock_tile != 0))
		return;

	v->current_order = *order;

	if (order->type == OT_GOTO_STATION) {
		const Station *st;

		if (order->dest == v->last_station_visited)
			v->last_station_visited = INVALID_STATION;

		st = GetStation(order->dest);
		if (st->dock_tile != 0) {
			v->dest_tile = TILE_ADD(st->dock_tile, ToTileIndexDiff(GetDockOffset(st->dock_tile)));
		} else {
			v->cur_order_index++;
		}
	} else if (order->type == OT_GOTO_DEPOT) {
		v->dest_tile = GetDepot(order->dest)->xy;
	} else {
		v->dest_tile = 0;
	}

	InvalidateVehicleOrder(v);

	InvalidateWindowClasses(WC_SHIPS_LIST);
}

void Ship::UpdateDeltaXY(Direction direction)
{
#define MKIT(a, b, c, d) ((a & 0xFF) << 24) | ((b & 0xFF) << 16) | ((c & 0xFF) << 8) | ((d & 0xFF) << 0)
	static const uint32 _delta_xy_table[8] = {
		MKIT( 6,  6,  -3,  -3),
		MKIT( 6, 32,  -3, -16),
		MKIT( 6,  6,  -3,  -3),
		MKIT(32,  6, -16,  -3),
		MKIT( 6,  6,  -3,  -3),
		MKIT( 6, 32,  -3, -16),
		MKIT( 6,  6,  -3,  -3),
		MKIT(32,  6, -16,  -3),
	};
#undef MKIT

	uint32 x = _delta_xy_table[direction];
	this->x_offs        = GB(x,  0, 8);
	this->y_offs        = GB(x,  8, 8);
	this->sprite_width  = GB(x, 16, 8);
	this->sprite_height = GB(x, 24, 8);
	this->z_height      = 6;
}

void RecalcShipStuff(Vehicle *v)
{
	v->UpdateDeltaXY(v->direction);
	v->cur_image = v->GetImage(v->direction);
	v->MarkDirty();
	InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
}

static const TileIndexDiffC _ship_leave_depot_offs[] = {
	{-1,  0},
	{ 0, -1}
};

static void CheckShipLeaveDepot(Vehicle *v)
{
	TileIndex tile;
	Axis axis;
	uint m;

	if (!v->IsInDepot()) return;

	tile = v->tile;
	axis = GetShipDepotAxis(tile);

	/* Check first side */
	if (_ship_sometracks[axis] & GetTileShipTrackStatus(TILE_ADD(tile, ToTileIndexDiff(_ship_leave_depot_offs[axis])))) {
		m = (axis == AXIS_X) ? 0x101 : 0x207;
	/* Check second side */
	} else if (_ship_sometracks[axis + 2] & GetTileShipTrackStatus(TILE_ADD(tile, -2 * ToTileIndexDiff(_ship_leave_depot_offs[axis])))) {
		m = (axis == AXIS_X) ? 0x105 : 0x203;
	} else {
		return;
	}
	v->direction    = (Direction)GB(m, 0, 8);
	v->u.ship.state = (TrackBits)GB(m, 8, 8);
	v->vehstatus &= ~VS_HIDDEN;

	v->cur_speed = 0;
	RecalcShipStuff(v);

	PlayShipSound(v);
	VehicleServiceInDepot(v);
	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	InvalidateWindowClasses(WC_SHIPS_LIST);
}

static bool ShipAccelerate(Vehicle *v)
{
	uint spd;
	byte t;

	spd = min(v->cur_speed + 1, GetVehicleProperty(v, 0x0B, v->max_speed));

	/*updates statusbar only if speed have changed to save CPU time */
	if (spd != v->cur_speed) {
		v->cur_speed = spd;
		if (_patches.vehicle_speed)
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}

	/* Decrease somewhat when turning */
	if (!(v->direction & 1)) spd = spd * 3 / 4;

	if (spd == 0) return false;
	if ((byte)++spd == 0) return true;

	v->progress = (t = v->progress) - (byte)spd;

	return (t < v->progress);
}

static CommandCost EstimateShipCost(EngineID engine_type)
{
	return CommandCost(EXPENSES_NEW_VEHICLES, GetEngineProperty(engine_type, 0x0A, ShipVehInfo(engine_type)->base_cost) * (_price.ship_base >> 3) >> 5);
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

struct PathFindShip {
	TileIndex skiptile;
	TileIndex dest_coords;
	uint best_bird_dist;
	uint best_length;
};

static bool ShipTrackFollower(TileIndex tile, PathFindShip *pfs, int track, uint length, byte *state)
{
	/* Found dest? */
	if (tile == pfs->dest_coords) {
		pfs->best_bird_dist = 0;

		pfs->best_length = minu(pfs->best_length, length);
		return true;
	}

	/* Skip this tile in the calculation */
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

static uint FindShipTrack(Vehicle *v, TileIndex tile, DiagDirection dir, TrackBits bits, TileIndex skiptile, Track *track)
{
	PathFindShip pfs;
	Track i, best_track;
	uint best_bird_dist = 0;
	uint best_length    = 0;
	uint r;
	byte ship_dir = v->direction & 3;

	pfs.dest_coords = v->dest_tile;
	pfs.skiptile = skiptile;

	best_track = INVALID_TRACK;

	do {
		i = RemoveFirstTrack(&bits);

		pfs.best_bird_dist = (uint)-1;
		pfs.best_length = (uint)-1;

		FollowTrack(tile, 0x1800 | TRANSPORT_WATER, 0, (DiagDirection)_ship_search_directions[i][dir], (TPFEnumProc*)ShipTrackFollower, NULL, &pfs);

		if (best_track != INVALID_TRACK) {
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

static inline NPFFoundTargetData PerfNPFRouteToStationOrTile(TileIndex tile, Trackdir trackdir, bool ignore_start_tile, NPFFindStationOrTileData* target, TransportType type, Owner owner, RailTypes railtypes)
{

	void* perf = NpfBeginInterval();
	NPFFoundTargetData ret = NPFRouteToStationOrTile(tile, trackdir, ignore_start_tile, target, type, 0, owner, railtypes);
	int t = NpfEndInterval(perf);
	DEBUG(yapf, 4, "[NPFW] %d us - %d rounds - %d open - %d closed -- ", t, 0, _aystar_stats_open_size, _aystar_stats_closed_size);
	return ret;
}

/** returns the track to choose on the next tile, or -1 when it's better to
 * reverse. The tile given is the tile we are about to enter, enterdir is the
 * direction in which we are entering the tile */
static Track ChooseShipTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks)
{
	assert(IsValidDiagDirection(enterdir));

	switch (_patches.pathfinder_for_ships) {
		case VPF_YAPF: { /* YAPF */
			Trackdir trackdir = YapfChooseShipTrack(v, tile, enterdir, tracks);
			if (trackdir != INVALID_TRACKDIR) return TrackdirToTrack(trackdir);
		} break;

		case VPF_NPF: { /* NPF */
			NPFFindStationOrTileData fstd;
			Trackdir trackdir = GetVehicleTrackdir(v);
			assert(trackdir != INVALID_TRACKDIR); // Check that we are not in a depot

			NPFFillWithOrderData(&fstd, v);

			NPFFoundTargetData ftd = PerfNPFRouteToStationOrTile(tile - TileOffsByDiagDir(enterdir), trackdir, true, &fstd, TRANSPORT_WATER, v->owner, INVALID_RAILTYPES);

			/* If ftd.best_bird_dist is 0, we found our target and ftd.best_trackdir contains
			 * the direction we need to take to get there, if ftd.best_bird_dist is not 0,
			 * we did not find our target, but ftd.best_trackdir contains the direction leading
			 * to the tile closest to our target. */
			if (ftd.best_trackdir != 0xff) return TrackdirToTrack(ftd.best_trackdir); /* TODO: Wrapper function? */
		} break;

		default:
		case VPF_OPF: { /* OPF */
			TileIndex tile2 = TILE_ADD(tile, -TileOffsByDiagDir(enterdir));
			Track track;

			/* Let's find out how far it would be if we would reverse first */
			TrackBits b = GetTileShipTrackStatus(tile2) & _ship_sometracks[ReverseDiagDir(enterdir)] & v->u.ship.state;

			uint distr = UINT_MAX; // distance if we reversed
			if (b != 0) {
				distr = FindShipTrack(v, tile2, ReverseDiagDir(enterdir), b, tile, &track);
				if (distr != UINT_MAX) distr++; // penalty for reversing
			}

			/* And if we would not reverse? */
			uint dist = FindShipTrack(v, tile, enterdir, tracks, 0, &track);

			if (dist <= distr) return track;
		} break;
	}

	return INVALID_TRACK; /* We could better reverse */
}

static const Direction _new_vehicle_direction_table[] = {
	DIR_N , DIR_NW, DIR_W , INVALID_DIR,
	DIR_NE, DIR_N , DIR_SW, INVALID_DIR,
	DIR_E , DIR_SE, DIR_S
};

static Direction ShipGetNewDirectionFromTiles(TileIndex new_tile, TileIndex old_tile)
{
	uint offs = (TileY(new_tile) - TileY(old_tile) + 1) * 4 +
							TileX(new_tile) - TileX(old_tile) + 1;
	assert(offs < 11 && offs != 3 && offs != 7);
	return _new_vehicle_direction_table[offs];
}

static Direction ShipGetNewDirection(Vehicle *v, int x, int y)
{
	uint offs = (y - v->y_pos + 1) * 4 + (x - v->x_pos + 1);
	assert(offs < 11 && offs != 3 && offs != 7);
	return _new_vehicle_direction_table[offs];
}

static TrackBits GetAvailShipTracks(TileIndex tile, int dir)
{
	uint32 r = GetTileTrackStatus(tile, TRANSPORT_WATER, 0);
	return (TrackBits)((r | r >> 8) & _ship_sometracks[dir]);
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
		{ 8, 15, 7},
		{ 0, 0, 0},
		{ 8, 15, 6},
		{ 0, 0, 0},
		{ 7, 15, 0},
	}
};

static void ShipController(Vehicle *v)
{
	uint32 r;
	const byte *b;
	Direction dir;
	Track track;
	TrackBits tracks;

	v->tick_counter++;
	v->current_order_time++;

	if (v->breakdown_ctr != 0) {
		if (v->breakdown_ctr <= 2) {
			HandleBrokenShip(v);
			return;
		}
		v->breakdown_ctr--;
	}

	if (v->vehstatus & VS_STOPPED) return;

	ProcessShipOrder(v);
	v->HandleLoading();

	if (v->current_order.type == OT_LOADING) return;

	CheckShipLeaveDepot(v);

	if (!ShipAccelerate(v)) return;

	BeginVehicleMove(v);

	GetNewVehiclePosResult gp = GetNewVehiclePos(v);
	if (gp.old_tile == gp.new_tile) {
		/* Staying in tile */
		if (v->IsInDepot()) {
			gp.x = v->x_pos;
			gp.y = v->y_pos;
		} else {
			/* Not inside depot */
			r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
			if (HasBit(r, VETS_CANNOT_ENTER)) goto reverse_direction;

			/* A leave station order only needs one tick to get processed, so we can
			 * always skip ahead. */
			if (v->current_order.type == OT_LEAVESTATION) {
				v->current_order.Free();
				InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
			} else if (v->dest_tile != 0) {
				/* We have a target, let's see if we reached it... */
				if (v->current_order.type == OT_GOTO_STATION &&
						IsBuoyTile(v->dest_tile) &&
						DistanceManhattan(v->dest_tile, gp.new_tile) <= 3) {
					/* We got within 3 tiles of our target buoy, so let's skip to our
					 * next order */
					UpdateVehicleTimetable(v, true);
					v->cur_order_index++;
					v->current_order.type = OT_DUMMY;
					InvalidateVehicleOrder(v);
				} else {
					/* Non-buoy orders really need to reach the tile */
					if (v->dest_tile == gp.new_tile) {
						if (v->current_order.type == OT_GOTO_DEPOT) {
							if ((gp.x & 0xF) == 8 && (gp.y & 0xF) == 8) {
								VehicleEnterDepot(v);
								return;
							}
						} else if (v->current_order.type == OT_GOTO_STATION) {
							Station *st;

							v->last_station_visited = v->current_order.dest;

							/* Process station in the orderlist. */
							st = GetStation(v->current_order.dest);
							if (st->facilities & FACIL_DOCK) { // ugly, ugly workaround for problem with ships able to drop off cargo at wrong stations
								ShipArrivesAt(v, st);
								v->BeginLoading();
							} else { // leave stations without docks right aways
								v->current_order.type = OT_LEAVESTATION;
								v->cur_order_index++;
								InvalidateVehicleOrder(v);
							}
						}
					}
				}
			}
		}
	} else {
		DiagDirection diagdir;
		/* New tile */
		if (TileX(gp.new_tile) >= MapMaxX() || TileY(gp.new_tile) >= MapMaxY()) {
			goto reverse_direction;
		}

		dir = ShipGetNewDirectionFromTiles(gp.new_tile, gp.old_tile);
		assert(dir == DIR_NE || dir == DIR_SE || dir == DIR_SW || dir == DIR_NW);
		diagdir = DirToDiagDir(dir);
		tracks = GetAvailShipTracks(gp.new_tile, diagdir);
		if (tracks == TRACK_BIT_NONE) goto reverse_direction;

		/* Choose a direction, and continue if we find one */
		track = ChooseShipTrack(v, gp.new_tile, diagdir, tracks);
		if (track == INVALID_TRACK) goto reverse_direction;

		b = _ship_subcoord[diagdir][track];

		gp.x = (gp.x & ~0xF) | b[0];
		gp.y = (gp.y & ~0xF) | b[1];

		/* Call the landscape function and tell it that the vehicle entered the tile */
		r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
		if (HasBit(r, VETS_CANNOT_ENTER)) goto reverse_direction;

		if (!HasBit(r, VETS_ENTERED_WORMHOLE)) {
			v->tile = gp.new_tile;
			v->u.ship.state = TrackToTrackBits(track);
		}

		v->direction = (Direction)b[2];
	}

	/* update image of ship, as well as delta XY */
	dir = ShipGetNewDirection(v, gp.x, gp.y);
	v->x_pos = gp.x;
	v->y_pos = gp.y;
	v->z_pos = GetSlopeZ(gp.x, gp.y);

getout:
	v->UpdateDeltaXY(dir);
	v->cur_image = v->GetImage(dir);
	VehiclePositionChanged(v);
	EndVehicleMove(v);
	return;

reverse_direction:
	dir = ReverseDir(v->direction);
	v->direction = dir;
	goto getout;
}

static void AgeShipCargo(Vehicle *v)
{
	if (_age_cargo_skip_counter != 0) return;
	v->cargo.AgeCargo();
}

void Ship::Tick()
{
	if (!(this->vehstatus & VS_STOPPED)) this->running_ticks++;

	AgeShipCargo(this);
	ShipController(this);
}


void ShipsYearlyLoop()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_SHIP) {
			v->profit_last_year = v->profit_this_year;
			v->profit_this_year = 0;
			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		}
	}
}

/** Build a ship.
 * @param tile tile of depot where ship is built
 * @param flags type of operation
 * @param p1 ship type being built (engine)
 * @param p2 bit 0 when set, the unitnumber will be 0, otherwise it will be a free number
 */
CommandCost CmdBuildShip(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost value;
	UnitID unit_num;
	Engine *e;

	if (!IsEngineBuildable(p1, VEH_SHIP, _current_player)) return_cmd_error(STR_SHIP_NOT_AVAILABLE);

	value = EstimateShipCost(p1);
	if (flags & DC_QUERY_COST) return value;

	/* The ai_new queries the vehicle cost before building the route,
	 * so we must check against cheaters no sooner than now. --pasky */
	if (!IsTileDepotType(tile, TRANSPORT_WATER)) return CMD_ERROR;
	if (!IsTileOwner(tile, _current_player)) return CMD_ERROR;

	unit_num = HasBit(p2, 0) ? 0 : GetFreeUnitNumber(VEH_SHIP);

	if (!Vehicle::AllocateList(NULL, 1) || unit_num > _patches.max_ships)
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

	if (flags & DC_EXEC) {
		int x;
		int y;

		const ShipVehicleInfo *svi = ShipVehInfo(p1);

		Vehicle *v = new Ship();
		v->unitnumber = unit_num;

		v->owner = _current_player;
		v->tile = tile;
		x = TileX(tile) * TILE_SIZE + TILE_SIZE / 2;
		y = TileY(tile) * TILE_SIZE + TILE_SIZE / 2;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopeZ(x, y);

		v->running_ticks = 0;

		v->UpdateDeltaXY(v->direction);
		v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;

		v->spritenum = svi->image_index;
		v->cargo_type = svi->cargo_type;
		v->cargo_subtype = 0;
		v->cargo_cap = svi->capacity;
		v->value = value.GetCost();

		v->last_station_visited = INVALID_STATION;
		v->max_speed = svi->max_speed;
		v->engine_type = p1;

		e = GetEngine(p1);
		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->lifelength * 366;
		_new_vehicle_id = v->index;

		v->name = NULL;
		v->u.ship.state = TRACK_BIT_DEPOT;

		v->service_interval = _patches.servint_ships;
		v->date_of_last_service = _date;
		v->build_year = _cur_year;
		v->cur_image = 0x0E5E;
		v->random_bits = VehicleRandomBits();

		v->vehicle_flags = 0;
		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) SetBit(v->vehicle_flags, VF_BUILT_AS_PROTOTYPE);

		v->cargo_cap = GetVehicleProperty(v, 0x0D, svi->capacity);

		VehiclePositionChanged(v);

		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
		InvalidateWindow(WC_COMPANY, v->owner);
		if (IsLocalPlayer())
			InvalidateAutoreplaceWindow(v->engine_type, v->group_id); // updates the replace Ship window

		GetPlayer(_current_player)->num_engines[p1]++;
	}

	return value;
}

/** Sell a ship.
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID to be sold
 * @param p2 unused
 */
CommandCost CmdSellShip(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_SHIP || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (HASBITS(v->vehstatus, VS_CRASHED)) return_cmd_error(STR_CAN_T_SELL_DESTROYED_VEHICLE);

	if (!v->IsStoppedInDepot()) {
		return_cmd_error(STR_980B_SHIP_MUST_BE_STOPPED_IN);
	}

	CommandCost ret(EXPENSES_NEW_VEHICLES, -v->value);

	if (flags & DC_EXEC) {
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
		InvalidateWindow(WC_COMPANY, v->owner);
		DeleteWindowById(WC_VEHICLE_VIEW, v->index);
		DeleteDepotHighlightOfVehicle(v);
		delete v;
	}

	return ret;
}

/** Start/Stop a ship.
 * @param tile unused
 * @param flags type of operation
 * @param p1 ship ID to start/stop
 * @param p2 unused
 */
CommandCost CmdStartStopShip(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	uint16 callback;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_SHIP || !CheckOwnership(v->owner)) return CMD_ERROR;

	/* Check if this ship can be started/stopped. The callback will fail or
	 * return 0xFF if it can. */
	callback = GetVehicleCallback(CBID_VEHICLE_START_STOP_CHECK, 0, 0, v->engine_type, v);
	if (callback != CALLBACK_FAILED && callback != 0xFF) {
		StringID error = GetGRFStringID(GetEngineGRFID(v->engine_type), 0xD000 + callback);
		return_cmd_error(error);
	}

	if (flags & DC_EXEC) {
		if (v->IsStoppedInDepot()) {
			DeleteVehicleNews(p1, STR_981C_SHIP_IS_WAITING_IN_DEPOT);
		}

		v->vehstatus ^= VS_STOPPED;
		v->cur_speed = 0;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		InvalidateWindowClasses(WC_SHIPS_LIST);
	}

	return CommandCost();
}

/** Send a ship to the depot.
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID to send to the depot
 * @param p2 various bitmasked elements
 * - p2 bit 0-3 - DEPOT_ flags (see vehicle.h)
 * - p2 bit 8-10 - VLW flag (for mass goto depot)
 */
CommandCost CmdSendShipToDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	const Depot *dep;

	if (p2 & DEPOT_MASS_SEND) {
		/* Mass goto depot requested */
		if (!ValidVLWFlags(p2 & VLW_MASK)) return CMD_ERROR;
		return SendAllVehiclesToDepot(VEH_SHIP, flags, p2 & DEPOT_SERVICE, _current_player, (p2 & VLW_MASK), p1);
	}

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_SHIP || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->vehstatus & VS_CRASHED) return CMD_ERROR;

	if (v->IsInDepot()) return CMD_ERROR;

	/* If the current orders are already goto-depot */
	if (v->current_order.type == OT_GOTO_DEPOT) {
		if (!!(p2 & DEPOT_SERVICE) == HasBit(v->current_order.flags, OF_HALT_IN_DEPOT)) {
			/* We called with a different DEPOT_SERVICE setting.
			 * Now we change the setting to apply the new one and let the vehicle head for the same depot.
			 * Note: the if is (true for requesting service == true for ordered to stop in depot)          */
			if (flags & DC_EXEC) {
				ClrBit(v->current_order.flags, OF_PART_OF_ORDERS);
				ToggleBit(v->current_order.flags, OF_HALT_IN_DEPOT);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
			}
			return CommandCost();
		}

		if (p2 & DEPOT_DONT_CANCEL) return CMD_ERROR; // Requested no cancelation of depot orders
		if (flags & DC_EXEC) {
			/* If the orders to 'goto depot' are in the orders list (forced servicing),
			 * then skip to the next order; effectively cancelling this forced service */
			if (HasBit(v->current_order.flags, OF_PART_OF_ORDERS))
				v->cur_order_index++;

			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		}
		return CommandCost();
	}

	dep = FindClosestShipDepot(v);
	if (dep == NULL) return_cmd_error(STR_981A_UNABLE_TO_FIND_LOCAL_DEPOT);

	if (flags & DC_EXEC) {
		if (v->current_order.type == OT_LOADING) v->LeaveStation();

		v->dest_tile = dep->xy;
		v->current_order.type = OT_GOTO_DEPOT;
		v->current_order.flags = OFB_NON_STOP;
		if (!(p2 & DEPOT_SERVICE)) SetBit(v->current_order.flags, OF_HALT_IN_DEPOT);
		v->current_order.refit_cargo = CT_INVALID;
		v->current_order.dest = dep->index;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}

	return CommandCost();
}


/** Refits a ship to the specified cargo type.
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID of the ship to refit
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0-7) - the new cargo type to refit to (p2 & 0xFF)
 * - p2 = (bit 8-15) - the new cargo subtype to refit to
 * - p2 = (bit 16) - refit only this vehicle (ignored)
 * @return cost of refit or error
 */
CommandCost CmdRefitShip(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	CommandCost cost(EXPENSES_SHIP_RUN);
	CargoID new_cid = GB(p2, 0, 8); //gets the cargo number
	byte new_subtype = GB(p2, 8, 8);
	uint16 capacity = CALLBACK_FAILED;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_SHIP || !CheckOwnership(v->owner)) return CMD_ERROR;
	if (!v->IsStoppedInDepot()) return_cmd_error(STR_980B_SHIP_MUST_BE_STOPPED_IN);
	if (v->vehstatus & VS_CRASHED) return_cmd_error(STR_CAN_T_REFIT_DESTROYED_VEHICLE);

	/* Check cargo */
	if (!ShipVehInfo(v->engine_type)->refittable) return CMD_ERROR;
	if (new_cid >= NUM_CARGO || !CanRefitTo(v->engine_type, new_cid)) return CMD_ERROR;

	/* Check the refit capacity callback */
	if (HasBit(EngInfo(v->engine_type)->callbackmask, CBM_VEHICLE_REFIT_CAPACITY)) {
		/* Back up the existing cargo type */
		CargoID temp_cid = v->cargo_type;
		byte temp_subtype = v->cargo_subtype;
		v->cargo_type = new_cid;
		v->cargo_subtype = new_subtype;

		capacity = GetVehicleCallback(CBID_VEHICLE_REFIT_CAPACITY, 0, 0, v->engine_type, v);

		/* Restore the cargo type */
		v->cargo_type = temp_cid;
		v->cargo_subtype = temp_subtype;
	}

	if (capacity == CALLBACK_FAILED) {
		capacity = GetVehicleProperty(v, 0x0D, ShipVehInfo(v->engine_type)->capacity);
	}
	_returned_refit_capacity = capacity;

	if (IsHumanPlayer(v->owner) && new_cid != v->cargo_type) {
		cost = GetRefitCost(v->engine_type);
	}

	if (flags & DC_EXEC) {
		v->cargo_cap = capacity;
		v->cargo.Truncate((v->cargo_type == new_cid) ? capacity : 0);
		v->cargo_type = new_cid;
		v->cargo_subtype = new_subtype;
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
	}

	return cost;

}
