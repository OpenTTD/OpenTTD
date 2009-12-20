/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ship_cmd.cpp Handling of ships. */

#include "stdafx.h"
#include "ship.h"
#include "landscape.h"
#include "timetable.h"
#include "command_func.h"
#include "news_func.h"
#include "company_func.h"
#include "pathfinder/npf/npf_func.h"
#include "depot_base.h"
#include "station_base.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "pathfinder/yapf/yapf.h"
#include "newgrf_sound.h"
#include "spritecache.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "autoreplace_gui.h"
#include "gfx_func.h"
#include "effectvehicle_func.h"
#include "ai/ai.hpp"
#include "pathfinder/opf/opf_ship.h"
#include "landscape_type.h"

#include "table/strings.h"
#include "table/sprites.h"

static const uint16 _ship_sprites[] = {0x0E5D, 0x0E55, 0x0E65, 0x0E6D};

static inline TrackBits GetTileShipTrackStatus(TileIndex tile)
{
	return TrackStatusToTrackBits(GetTileTrackStatus(tile, TRANSPORT_WATER, 0));
}

static SpriteID GetShipIcon(EngineID engine)
{
	const Engine *e = Engine::Get(engine);
	uint8 spritenum = e->u.ship.image_index;

	if (is_custom_sprite(spritenum)) {
		SpriteID sprite = GetCustomVehicleIcon(engine, DIR_W);
		if (sprite != 0) return sprite;

		spritenum = e->original_image_index;
	}

	return DIR_W + _ship_sprites[spritenum];
}

void DrawShipEngine(int left, int right, int preferred_x, int y, EngineID engine, SpriteID pal)
{
	SpriteID sprite = GetShipIcon(engine);
	const Sprite *real_sprite = GetSprite(sprite, ST_NORMAL);
	preferred_x = Clamp(preferred_x, left - real_sprite->x_offs, right - real_sprite->width - real_sprite->x_offs);
	DrawSprite(sprite, pal, preferred_x, y);
}

/** Get the size of the sprite of a ship sprite heading west (used for lists)
 * @param engine The engine to get the sprite from
 * @param width The width of the sprite
 * @param height The height of the sprite
 */
void GetShipSpriteSize(EngineID engine, uint &width, uint &height)
{
	const Sprite *spr = GetSprite(GetShipIcon(engine), ST_NORMAL);

	width  = spr->width;
	height = spr->height;
}

SpriteID Ship::GetImage(Direction direction) const
{
	uint8 spritenum = this->spritenum;

	if (is_custom_sprite(spritenum)) {
		SpriteID sprite = GetCustomVehicleSprite(this, direction);
		if (sprite != 0) return sprite;

		spritenum = Engine::Get(this->engine_type)->original_image_index;
	}

	return _ship_sprites[spritenum] + direction;
}

static const Depot *FindClosestShipDepot(const Vehicle *v, uint max_distance)
{
	/* Find the closest depot */
	const Depot *depot;
	const Depot *best_depot = NULL;
	/* If we don't have a maximum distance, i.e. distance = 0,
	 * we want to find any depot so the best distance of no
	 * depot must be more than any correct distance. On the
	 * other hand if we have set a maximum distance, any depot
	 * further away than max_distance can safely be ignored. */
	uint best_dist = max_distance == 0 ? UINT_MAX : max_distance + 1;

	FOR_ALL_DEPOTS(depot) {
		TileIndex tile = depot->xy;
		if (IsShipDepotTile(tile) && IsTileOwner(tile, v->owner)) {
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
	if (Company::Get(v->owner)->settings.vehicle.servint_ships == 0 || !v->NeedsAutomaticServicing()) return;
	if (v->IsInDepot()) {
		VehicleServiceInDepot(v);
		return;
	}

	uint max_distance;
	switch (_settings_game.pf.pathfinder_for_ships) {
		case VPF_OPF:  max_distance = 12; break;
		case VPF_NPF:  max_distance = _settings_game.pf.npf.maximum_go_to_depot_penalty  / NPF_TILE_LENGTH;  break;
		case VPF_YAPF: max_distance = _settings_game.pf.yapf.maximum_go_to_depot_penalty / YAPF_TILE_LENGTH; break;
		default: NOT_REACHED();
	}

	const Depot *depot = FindClosestShipDepot(v, max_distance);

	if (depot == NULL) {
		if (v->current_order.IsType(OT_GOTO_DEPOT)) {
			v->current_order.MakeDummy();
			SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		}
		return;
	}

	v->current_order.MakeGoToDepot(depot->index, ODTFB_SERVICE);
	v->dest_tile = depot->xy;
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
}

Money Ship::GetRunningCost() const
{
	const Engine *e = Engine::Get(this->engine_type);
	uint cost_factor = GetVehicleProperty(this, PROP_SHIP_RUNNING_COST_FACTOR, e->u.ship.running_cost);
	return GetPrice(PR_RUNNING_SHIP, cost_factor, e->grffile);
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

	CommandCost cost(EXPENSES_SHIP_RUN, this->GetRunningCost() * this->running_ticks / (DAYS_IN_YEAR * DAY_TICKS));

	this->profit_this_year -= cost.GetCost();
	this->running_ticks = 0;

	SubtractMoneyFromCompanyFract(this->owner, cost);

	SetWindowDirty(WC_VEHICLE_DETAILS, this->index);
	/* we need this for the profit */
	SetWindowClassesDirty(WC_SHIPS_LIST);
}

Trackdir Ship::GetVehicleTrackdir() const
{
	if (this->vehstatus & VS_CRASHED) return INVALID_TRACKDIR;

	if (this->IsInDepot()) {
		/* We'll assume the ship is facing outwards */
		return DiagDirToDiagTrackdir(GetShipDepotDirection(this->tile));
	}

	if (this->state == TRACK_BIT_WORMHOLE) {
		/* ship on aqueduct, so just use his direction and assume a diagonal track */
		return DiagDirToDiagTrackdir(DirToDiagDir(this->direction));
	}

	return TrackDirectionToTrackdir(FindFirstTrack(this->state), this->direction);
}

static void HandleBrokenShip(Vehicle *v)
{
	if (v->breakdown_ctr != 1) {
		v->breakdown_ctr = 1;
		v->cur_speed = 0;

		if (v->breakdowns_since_last_service != 255)
			v->breakdowns_since_last_service++;

		v->MarkDirty();
		SetWindowDirty(WC_VEHICLE_VIEW, v->index);
		SetWindowDirty(WC_VEHICLE_DETAILS, v->index);

		if (!PlayVehicleSound(v, VSE_BREAKDOWN)) {
			SndPlayVehicleFx((_settings_game.game_creation.landscape != LT_TOYLAND) ?
				SND_10_TRAIN_BREAKDOWN : SND_3A_COMEDY_BREAKDOWN_2, v);
		}

		if (!(v->vehstatus & VS_HIDDEN)) {
			EffectVehicle *u = CreateEffectVehicleRel(v, 4, 4, 5, EV_BREAKDOWN_SMOKE);
			if (u != NULL) u->animation_state = v->breakdown_delay * 2;
		}
	}

	if (!(v->tick_counter & 1)) {
		if (!--v->breakdown_delay) {
			v->breakdown_ctr = 0;
			v->MarkDirty();
			SetWindowDirty(WC_VEHICLE_VIEW, v->index);
		}
	}
}

void Ship::MarkDirty()
{
	this->UpdateViewport(false, false);
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

TileIndex Ship::GetOrderStationLocation(StationID station)
{
	if (station == this->last_station_visited) this->last_station_visited = INVALID_STATION;

	const Station *st = Station::Get(station);
	if (st->dock_tile != INVALID_TILE) {
		return TILE_ADD(st->dock_tile, ToTileIndexDiff(GetDockOffset(st->dock_tile)));
	} else {
		this->IncrementOrderIndex();
		return 0;
	}
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
	this->x_extent      = GB(x, 16, 8);
	this->y_extent      = GB(x, 24, 8);
	this->z_extent      = 6;
}

void RecalcShipStuff(Vehicle *v)
{
	v->UpdateViewport(false, true);
	SetWindowDirty(WC_VEHICLE_DEPOT, v->tile);
}

static const TileIndexDiffC _ship_leave_depot_offs[] = {
	{-1,  0},
	{ 0, -1}
};

static void CheckShipLeaveDepot(Ship *v)
{
	if (!v->IsInDepot()) return;

	TileIndex tile = v->tile;
	Axis axis = GetShipDepotAxis(tile);

	/* Check first (north) side */
	if (DiagdirReachesTracks((DiagDirection)axis) & GetTileShipTrackStatus(TILE_ADD(tile, ToTileIndexDiff(_ship_leave_depot_offs[axis])))) {
		v->direction = ReverseDir(AxisToDirection(axis));
	/* Check second (south) side */
	} else if (DiagdirReachesTracks((DiagDirection)(axis + 2)) & GetTileShipTrackStatus(TILE_ADD(tile, -2 * ToTileIndexDiff(_ship_leave_depot_offs[axis])))) {
		v->direction = AxisToDirection(axis);
	} else {
		return;
	}

	v->state = AxisToTrackBits(axis);
	v->vehstatus &= ~VS_HIDDEN;

	v->cur_speed = 0;
	RecalcShipStuff(v);

	PlayShipSound(v);
	VehicleServiceInDepot(v);
	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	SetWindowClassesDirty(WC_SHIPS_LIST);
}

static bool ShipAccelerate(Vehicle *v)
{
	uint spd;
	byte t;

	spd = min(v->cur_speed + 1, GetVehicleProperty(v, PROP_SHIP_SPEED, v->max_speed));

	/* updates statusbar only if speed have changed to save CPU time */
	if (spd != v->cur_speed) {
		v->cur_speed = spd;
		if (_settings_client.gui.vehicle_speed)
			SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}

	/* Decrease somewhat when turning */
	if (!(v->direction & 1)) spd = spd * 3 / 4;

	if (spd == 0) return false;
	if ((byte)++spd == 0) return true;

	v->progress = (t = v->progress) - (byte)spd;

	return (t < v->progress);
}

static void ShipArrivesAt(const Vehicle *v, Station *st)
{
	/* Check if station was ever visited before */
	if (!(st->had_vehicle_of_type & HVOT_SHIP)) {
		st->had_vehicle_of_type |= HVOT_SHIP;

		SetDParam(0, st->index);
		AddVehicleNewsItem(
			STR_NEWS_FIRST_SHIP_ARRIVAL,
			(v->owner == _local_company) ? NS_ARRIVAL_COMPANY : NS_ARRIVAL_OTHER,
			v->index,
			st->index
		);
		AI::NewEvent(v->owner, new AIEventStationFirstVehicle(st->index, v->index));
	}
}


/** returns the track to choose on the next tile, or -1 when it's better to
 * reverse. The tile given is the tile we are about to enter, enterdir is the
 * direction in which we are entering the tile */
static Track ChooseShipTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks)
{
	assert(IsValidDiagDirection(enterdir));

	switch (_settings_game.pf.pathfinder_for_ships) {
		case VPF_OPF: return OPFShipChooseTrack(v, tile, enterdir, tracks);
		case VPF_NPF: return NPFShipChooseTrack(v, tile, enterdir, tracks);
		case VPF_YAPF: return YapfShipChooseTrack(v, tile, enterdir, tracks);
		default: NOT_REACHED();
	}
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

static inline TrackBits GetAvailShipTracks(TileIndex tile, DiagDirection dir)
{
	return GetTileShipTrackStatus(tile) & DiagdirReachesTracks(dir);
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

static void ShipController(Ship *v)
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
		if (!v->current_order.IsType(OT_LOADING)) v->breakdown_ctr--;
	}

	if (v->vehstatus & VS_STOPPED) return;

	ProcessOrders(v);
	v->HandleLoading();

	if (v->current_order.IsType(OT_LOADING)) return;

	CheckShipLeaveDepot(v);

	if (!ShipAccelerate(v)) return;

	GetNewVehiclePosResult gp = GetNewVehiclePos(v);
	if (v->state != TRACK_BIT_WORMHOLE) {
		/* Not on a bridge */
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
				if (v->current_order.IsType(OT_LEAVESTATION)) {
					v->current_order.Free();
					SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
				} else if (v->dest_tile != 0) {
					/* We have a target, let's see if we reached it... */
					if (v->current_order.IsType(OT_GOTO_WAYPOINT) &&
							DistanceManhattan(v->dest_tile, gp.new_tile) <= 3) {
						/* We got within 3 tiles of our target buoy, so let's skip to our
						 * next order */
						UpdateVehicleTimetable(v, true);
						v->IncrementOrderIndex();
						v->current_order.MakeDummy();
					} else {
						/* Non-buoy orders really need to reach the tile */
						if (v->dest_tile == gp.new_tile) {
							if (v->current_order.IsType(OT_GOTO_DEPOT)) {
								if ((gp.x & 0xF) == 8 && (gp.y & 0xF) == 8) {
									VehicleEnterDepot(v);
									return;
								}
							} else if (v->current_order.IsType(OT_GOTO_STATION)) {
								v->last_station_visited = v->current_order.GetDestination();

								/* Process station in the orderlist. */
								Station *st = Station::Get(v->current_order.GetDestination());
								if (st->facilities & FACIL_DOCK) { // ugly, ugly workaround for problem with ships able to drop off cargo at wrong stations
									ShipArrivesAt(v, st);
									v->BeginLoading();
								} else { // leave stations without docks right aways
									v->current_order.MakeLeaveStation();
									v->IncrementOrderIndex();
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
				v->state = TrackToTrackBits(track);
			}

			v->direction = (Direction)b[2];
		}
	} else {
		/* On a bridge */
		if (!IsTileType(gp.new_tile, MP_TUNNELBRIDGE) || !HasBit(VehicleEnterTile(v, gp.new_tile, gp.x, gp.y), VETS_ENTERED_WORMHOLE)) {
			v->x_pos = gp.x;
			v->y_pos = gp.y;
			VehicleMove(v, !(v->vehstatus & VS_HIDDEN));
			return;
		}
	}

	/* update image of ship, as well as delta XY */
	dir = ShipGetNewDirection(v, gp.x, gp.y);
	v->x_pos = gp.x;
	v->y_pos = gp.y;
	v->z_pos = GetSlopeZ(gp.x, gp.y);

getout:
	v->UpdateViewport(true, true);
	return;

reverse_direction:
	dir = ReverseDir(v->direction);
	v->direction = dir;
	goto getout;
}

bool Ship::Tick()
{
	if (!(this->vehstatus & VS_STOPPED)) this->running_ticks++;

	ShipController(this);

	return true;
}

/** Build a ship.
 * @param tile tile of depot where ship is built
 * @param flags type of operation
 * @param p1 ship type being built (engine)
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildShip(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	UnitID unit_num;

	if (!IsEngineBuildable(p1, VEH_SHIP, _current_company)) return_cmd_error(STR_ERROR_SHIP_NOT_AVAILABLE);

	const Engine *e = Engine::Get(p1);
	CommandCost value(EXPENSES_NEW_VEHICLES, e->GetCost());

	/* Engines without valid cargo should not be available */
	if (e->GetDefaultCargoType() == CT_INVALID) return CMD_ERROR;

	if (flags & DC_QUERY_COST) return value;

	/* The ai_new queries the vehicle cost before building the route,
	 * so we must check against cheaters no sooner than now. --pasky */
	if (!IsShipDepotTile(tile)) return CMD_ERROR;
	if (!IsTileOwner(tile, _current_company)) return CMD_ERROR;

	unit_num = (flags & DC_AUTOREPLACE) ? 0 : GetFreeUnitNumber(VEH_SHIP);

	if (!Vehicle::CanAllocateItem() || unit_num > _settings_game.vehicle.max_ships)
		return_cmd_error(STR_ERROR_TOO_MANY_VEHICLES_IN_GAME);

	if (flags & DC_EXEC) {
		int x;
		int y;

		const ShipVehicleInfo *svi = &e->u.ship;

		Ship *v = new Ship();
		v->unitnumber = unit_num;

		v->owner = _current_company;
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
		v->cargo_type = e->GetDefaultCargoType();
		v->cargo_subtype = 0;
		v->cargo_cap = svi->capacity;
		v->value = value.GetCost();

		v->last_station_visited = INVALID_STATION;
		v->max_speed = svi->max_speed;
		v->engine_type = p1;

		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->GetLifeLengthInDays();
		_new_vehicle_id = v->index;

		v->name = NULL;
		v->state = TRACK_BIT_DEPOT;

		v->service_interval = Company::Get(_current_company)->settings.vehicle.servint_ships;
		v->date_of_last_service = _date;
		v->build_year = _cur_year;
		v->cur_image = SPR_IMG_QUERY;
		v->random_bits = VehicleRandomBits();

		v->vehicle_flags = 0;
		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) SetBit(v->vehicle_flags, VF_BUILT_AS_PROTOTYPE);

		v->InvalidateNewGRFCacheOfChain();

		v->cargo_cap = GetVehicleCapacity(v);

		v->InvalidateNewGRFCacheOfChain();

		VehicleMove(v, false);

		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
		InvalidateWindowClassesData(WC_SHIPS_LIST, 0);
		SetWindowDirty(WC_COMPANY, v->owner);
		if (IsLocalCompany())
			InvalidateAutoreplaceWindow(v->engine_type, v->group_id); // updates the replace Ship window

		Company::Get(_current_company)->num_engines[p1]++;
	}

	return value;
}

/** Sell a ship.
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID to be sold
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdSellShip(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Ship *v = Ship::GetIfValid(p1);
	if (v == NULL || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->vehstatus & VS_CRASHED) return_cmd_error(STR_ERROR_CAN_T_SELL_DESTROYED_VEHICLE);

	if (!v->IsStoppedInDepot()) {
		return_cmd_error(STR_ERROR_SHIP_MUST_BE_STOPPED_IN_DEPOT);
	}

	CommandCost ret(EXPENSES_NEW_VEHICLES, -v->value);

	if (flags & DC_EXEC) {
		delete v;
	}

	return ret;
}

bool Ship::FindClosestDepot(TileIndex *location, DestinationID *destination, bool *reverse)
{
	const Depot *depot = FindClosestShipDepot(this, 0);

	if (depot == NULL) return false;

	if (location    != NULL) *location    = depot->xy;
	if (destination != NULL) *destination = depot->index;

	return true;
}

/** Send a ship to the depot.
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID to send to the depot
 * @param p2 various bitmasked elements
 * - p2 bit 0-3 - DEPOT_ flags (see vehicle.h)
 * - p2 bit 8-10 - VLW flag (for mass goto depot)
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdSendShipToDepot(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (p2 & DEPOT_MASS_SEND) {
		/* Mass goto depot requested */
		if (!ValidVLWFlags(p2 & VLW_MASK)) return CMD_ERROR;
		return SendAllVehiclesToDepot(VEH_SHIP, flags, p2 & DEPOT_SERVICE, _current_company, (p2 & VLW_MASK), p1);
	}

	Ship *v = Ship::GetIfValid(p1);
	if (v == NULL) return CMD_ERROR;

	return v->SendToDepot(flags, (DepotCommand)(p2 & DEPOT_COMMAND_MASK));
}


/** Refits a ship to the specified cargo type.
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID of the ship to refit
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0-7) - the new cargo type to refit to (p2 & 0xFF)
 * - p2 = (bit 8-15) - the new cargo subtype to refit to
 * - p2 = (bit 16) - refit only this vehicle (ignored)
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdRefitShip(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CargoID new_cid = GB(p2, 0, 8); // gets the cargo number
	byte new_subtype = GB(p2, 8, 8);

	Ship *v = Ship::GetIfValid(p1);

	if (v == NULL || !CheckOwnership(v->owner)) return CMD_ERROR;
	if (!v->IsStoppedInDepot()) return_cmd_error(STR_ERROR_SHIP_MUST_BE_STOPPED_IN_DEPOT);
	if (v->vehstatus & VS_CRASHED) return_cmd_error(STR_ERROR_CAN_T_REFIT_DESTROYED_VEHICLE);

	/* Check cargo */
	if (new_cid >= NUM_CARGO) return CMD_ERROR;

	CommandCost cost = RefitVehicle(v, true, new_cid, new_subtype, flags);

	if (flags & DC_EXEC) {
		v->colourmap = PAL_NONE; // invalidate vehicle colour map
		SetWindowDirty(WC_VEHICLE_DETAILS, v->index);
		SetWindowDirty(WC_VEHICLE_DEPOT, v->tile);
		InvalidateWindowClassesData(WC_SHIPS_LIST, 0);
	}
	v->InvalidateNewGRFCacheOfChain(); // always invalidate; querycost might have filled it

	return cost;

}
