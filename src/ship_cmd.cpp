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
#include "news_func.h"
#include "company_func.h"
#include "depot_base.h"
#include "station_base.h"
#include "newgrf_engine.h"
#include "pathfinder/yapf/yapf.h"
#include "pathfinder/yapf/yapf_ship_regions.h"
#include "newgrf_sound.h"
#include "spritecache.h"
#include "strings_func.h"
#include "window_func.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_economy.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "ai/ai.hpp"
#include "game/game.hpp"
#include "engine_base.h"
#include "company_base.h"
#include "tunnelbridge_map.h"
#include "zoom_func.h"
#include "framerate_type.h"
#include "industry.h"
#include "industry_map.h"
#include "ship_cmd.h"
#include "command_func.h"

#include "table/strings.h"

#include <unordered_set>

#include "safeguards.h"

/** Max distance in tiles (as the crow flies) to search for depots when user clicks "go to depot". */
constexpr int MAX_SHIP_DEPOT_SEARCH_DISTANCE = 80;

/**
 * Determine the effective #WaterClass for a ship travelling on a tile.
 * @param tile Tile of interest
 * @return the waterclass to be used by the ship.
 */
WaterClass GetEffectiveWaterClass(TileIndex tile)
{
	if (HasTileWaterClass(tile)) return GetWaterClass(tile);
	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		assert(GetTunnelBridgeTransportType(tile) == TRANSPORT_WATER);
		return WATER_CLASS_CANAL;
	}
	if (IsTileType(tile, MP_RAILWAY)) {
		assert(GetRailGroundType(tile) == RAIL_GROUND_WATER);
		return WATER_CLASS_SEA;
	}
	NOT_REACHED();
}

static const uint16_t _ship_sprites[] = {0x0E5D, 0x0E55, 0x0E65, 0x0E6D};

template <>
bool IsValidImageIndex<VEH_SHIP>(uint8_t image_index)
{
	return image_index < lengthof(_ship_sprites);
}

static inline TrackBits GetTileShipTrackStatus(TileIndex tile)
{
	return TrackStatusToTrackBits(GetTileTrackStatus(tile, TRANSPORT_WATER, 0));
}

static void GetShipIcon(EngineID engine, EngineImageType image_type, VehicleSpriteSeq *result)
{
	const Engine *e = Engine::Get(engine);
	uint8_t spritenum = e->u.ship.image_index;

	if (is_custom_sprite(spritenum)) {
		GetCustomVehicleIcon(engine, DIR_W, image_type, result);
		if (result->IsValid()) return;

		spritenum = e->original_image_index;
	}

	assert(IsValidImageIndex<VEH_SHIP>(spritenum));
	result->Set(DIR_W + _ship_sprites[spritenum]);
}

void DrawShipEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal, EngineImageType image_type)
{
	VehicleSpriteSeq seq;
	GetShipIcon(engine, image_type, &seq);

	Rect rect;
	seq.GetBounds(&rect);
	preferred_x = Clamp(preferred_x,
			left - UnScaleGUI(rect.left),
			right - UnScaleGUI(rect.right));

	seq.Draw(preferred_x, y, pal, pal == PALETTE_CRASH);
}

/**
 * Get the size of the sprite of a ship sprite heading west (used for lists).
 * @param engine The engine to get the sprite from.
 * @param[out] width The width of the sprite.
 * @param[out] height The height of the sprite.
 * @param[out] xoffs Number of pixels to shift the sprite to the right.
 * @param[out] yoffs Number of pixels to shift the sprite downwards.
 * @param image_type Context the sprite is used in.
 */
void GetShipSpriteSize(EngineID engine, uint &width, uint &height, int &xoffs, int &yoffs, EngineImageType image_type)
{
	VehicleSpriteSeq seq;
	GetShipIcon(engine, image_type, &seq);

	Rect rect;
	seq.GetBounds(&rect);

	width  = UnScaleGUI(rect.Width());
	height = UnScaleGUI(rect.Height());
	xoffs  = UnScaleGUI(rect.left);
	yoffs  = UnScaleGUI(rect.top);
}

void Ship::GetImage(Direction direction, EngineImageType image_type, VehicleSpriteSeq *result) const
{
	uint8_t spritenum = this->spritenum;

	if (image_type == EIT_ON_MAP) direction = this->rotation;

	if (is_custom_sprite(spritenum)) {
		GetCustomVehicleSprite(this, direction, image_type, result);
		if (result->IsValid()) return;

		spritenum = this->GetEngine()->original_image_index;
	}

	assert(IsValidImageIndex<VEH_SHIP>(spritenum));
	result->Set(_ship_sprites[spritenum] + direction);
}

static const Depot *FindClosestShipDepot(const Vehicle *v, uint max_distance)
{
	const int max_region_distance = (max_distance / WATER_REGION_EDGE_LENGTH) + 1;

	static std::unordered_set<int> visited_patch_hashes;
	static std::deque<WaterRegionPatchDesc> patches_to_search;
	visited_patch_hashes.clear();
	patches_to_search.clear();

	/* Step 1: find a set of reachable Water Region Patches using BFS. */
	const WaterRegionPatchDesc start_patch = GetWaterRegionPatchInfo(v->tile);
	patches_to_search.push_back(start_patch);
	visited_patch_hashes.insert(CalculateWaterRegionPatchHash(start_patch));

	while (!patches_to_search.empty()) {
		/* Remove first patch from the queue and make it the current patch. */
		const WaterRegionPatchDesc current_node = patches_to_search.front();
		patches_to_search.pop_front();

		/* Add neighbors of the current patch to the search queue. */
		TVisitWaterRegionPatchCallBack visitFunc = [&](const WaterRegionPatchDesc &water_region_patch) {
			/* Note that we check the max distance per axis, not the total distance. */
			if (std::abs(water_region_patch.x - start_patch.x) > max_region_distance ||
					std::abs(water_region_patch.y - start_patch.y) > max_region_distance) return;

			const int hash = CalculateWaterRegionPatchHash(water_region_patch);
			if (visited_patch_hashes.count(hash) == 0) {
				visited_patch_hashes.insert(hash);
				patches_to_search.push_back(water_region_patch);
			}
		};

		VisitWaterRegionPatchNeighbors(current_node, visitFunc);
	}

	/* Step 2: Find the closest depot within the reachable Water Region Patches. */
	const Depot *best_depot = nullptr;
	uint best_dist_sq = std::numeric_limits<uint>::max();
	for (const Depot *depot : Depot::Iterate()) {
		if (depot->veh_type != VEH_SHIP || depot->owner != v->owner || !depot->IsInUse()) continue;

		const TileIndex tile = depot->xy;
		const uint dist_sq = DistanceSquare(tile, v->tile);
		if (dist_sq < best_dist_sq && dist_sq <= max_distance * max_distance &&
				visited_patch_hashes.count(CalculateWaterRegionPatchHash(GetWaterRegionPatchInfo(tile))) > 0) {
			best_dist_sq = dist_sq;
			best_depot = depot;
		}
	}

	return best_depot;
}

static void CheckIfShipNeedsService(Vehicle *v)
{
	if (Company::Get(v->owner)->settings.vehicle.servint_ships == 0 || !v->NeedsAutomaticServicing()) return;
	if (v->IsChainInDepot()) {
		VehicleServiceInDepot(v);
		return;
	}

	uint max_distance = _settings_game.pf.yapf.maximum_go_to_depot_penalty / YAPF_TILE_LENGTH;

	const Depot *depot = FindClosestShipDepot(v, max_distance);

	if (depot == nullptr) {
		if (v->current_order.IsType(OT_GOTO_DEPOT)) {
			v->current_order.MakeDummy();
			SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);
		}
		return;
	}

	v->current_order.MakeGoToDepot(depot->index, ODTFB_SERVICE);
	v->SetDestTile(depot->GetBestDepotTile(v));
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);
}

/**
 * Update the caches of this ship.
 */
void Ship::UpdateCache()
{
	const ShipVehicleInfo *svi = ShipVehInfo(this->engine_type);

	/* Get speed fraction for the current water type. Aqueducts are always canals. */
	bool is_ocean = GetEffectiveWaterClass(this->tile) == WATER_CLASS_SEA;
	uint raw_speed = GetVehicleProperty(this, PROP_SHIP_SPEED, svi->max_speed);
	this->vcache.cached_max_speed = svi->ApplyWaterClassSpeedFrac(raw_speed, is_ocean);

	/* Update cargo aging period. */
	this->vcache.cached_cargo_age_period = GetVehicleProperty(this, PROP_SHIP_CARGO_AGE_PERIOD, EngInfo(this->engine_type)->cargo_age_period);

	this->UpdateVisualEffect();
}

Money Ship::GetRunningCost() const
{
	const Engine *e = this->GetEngine();
	uint cost_factor = GetVehicleProperty(this, PROP_SHIP_RUNNING_COST_FACTOR, e->u.ship.running_cost);
	return GetPrice(PR_RUNNING_SHIP, cost_factor, e->GetGRF());
}

/** Calendar day handler. */
void Ship::OnNewCalendarDay()
{
	AgeVehicle(this);
}

/** Economy day handler. */
void Ship::OnNewEconomyDay()
{
	EconomyAgeVehicle(this);

	if ((++this->day_counter & 7) == 0) {
		DecreaseVehicleValue(this);
	}

	CheckVehicleBreakdown(this);
	CheckIfShipNeedsService(this);

	CheckOrders(this);

	if (this->running_ticks == 0) return;

	CommandCost cost(EXPENSES_SHIP_RUN, this->GetRunningCost() * this->running_ticks / (CalendarTime::DAYS_IN_YEAR * Ticks::DAY_TICKS));

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
		/* Only old depots need it. */
		/* We'll assume the ship is facing outwards. */
		if (this->state == TRACK_BIT_DEPOT) return DiagDirToDiagTrackdir(GetShipDepotDirection(this->tile));
	}

	if (this->state == TRACK_BIT_WORMHOLE || this->IsInDepot()) {
		/* ship on aqueduct, so just use its direction and assume a diagonal track */
		return DiagDirToDiagTrackdir(DirToDiagDir(this->direction));
	}

	return TrackDirectionToTrackdir(FindFirstTrack(this->state), this->direction);
}

Ship::~Ship()
{
	if (CleaningPool()) return;

	if (this->IsInDepot()) SetDepotReservation(this->tile, DEPOT_RESERVATION_EMPTY);

	this->PreDestructor();
}

void Ship::MarkDirty()
{
	this->colourmap = PAL_NONE;
	this->UpdateViewport(true, false);
	this->UpdateCache();
}

void Ship::PlayLeaveStationSound(bool force) const
{
	if (PlayVehicleSound(this, VSE_START, force)) return;
	SndPlayVehicleFx(ShipVehInfo(this->engine_type)->sfx, this);
}

TileIndex Ship::GetOrderStationLocation(StationID station)
{
	if (station == this->last_station_visited) this->last_station_visited = INVALID_STATION;

	const Station *st = Station::Get(station);
	if (CanVehicleUseStation(this, st)) {
		return st->xy;
	} else {
		this->IncrementRealOrderIndex();
		return 0;
	}
}

void Ship::UpdateDeltaXY()
{
	static const int8_t _delta_xy_table[8][4] = {
		/* y_extent, x_extent, y_offs, x_offs */
		{ 6,  6,  -3,  -3}, // N
		{ 6, 32,  -3, -16}, // NE
		{ 6,  6,  -3,  -3}, // E
		{32,  6, -16,  -3}, // SE
		{ 6,  6,  -3,  -3}, // S
		{ 6, 32,  -3, -16}, // SW
		{ 6,  6,  -3,  -3}, // W
		{32,  6, -16,  -3}, // NW
	};

	const int8_t *bb = _delta_xy_table[this->rotation];
	this->x_offs        = bb[3];
	this->y_offs        = bb[2];
	this->x_extent      = bb[1];
	this->y_extent      = bb[0];
	this->z_extent      = 6;

	if (this->direction != this->rotation) {
		/* If we are rotating, then it is possible the ship was moved to its next position. In that
		 * case, because we are still showing the old direction, the ship will appear to glitch sideways
		 * slightly. We can work around this by applying an additional offset to make the ship appear
		 * where it was before it moved. */
		this->x_offs -= this->x_pos - this->rotation_x_pos;
		this->y_offs -= this->y_pos - this->rotation_y_pos;
	}
}

/**
 * Test-procedure for HasVehicleOnPos to check for any ships which are moving.
 */
static Vehicle *EnsureNoMovingShipProc(Vehicle *v, void *)
{
	return v->type == VEH_SHIP && v->cur_speed != 0 ? v : nullptr;
}

static bool CheckReverseShip(const Ship *v, Trackdir *trackdir = nullptr)
{
	/* Ask pathfinder for best direction */
	return YapfShipCheckReverse(v, trackdir);
}

static bool CheckPlaceShipOnDepot(TileIndex tile)
{
	assert(IsShipDepotTile(tile));
	return !IsExtendedDepot(tile) || IsExtendedDepotEmpty(tile);
}

void HandleShipEnterDepot(Ship *v)
{
	assert(IsShipDepotTile(v->tile));

	if (IsExtendedDepot(v->tile)) {
		SetDepotReservation(v->tile, DEPOT_RESERVATION_IN_USE);
		v->state |= TRACK_BIT_DEPOT;
		v->cur_speed = 0;
		v->UpdateCache();
		v->UpdateViewport(true, true);
		SetWindowClassesDirty(WC_SHIPS_LIST);
		SetWindowDirty(WC_VEHICLE_VIEW, v->index);

		InvalidateWindowData(WC_VEHICLE_DEPOT, GetDepotIndex(v->tile));
		v->StartService();
	} else {
		VehicleEnterDepot(v);
	}
}

static bool CheckShipLeaveDepot(Ship *v)
{
	if (!v->IsChainInDepot()) return false;

	/* Check if we should wait here for unbunching. */
	if (v->IsWaitingForUnbunching()) return true;

	/* We are leaving a depot, but have to go to the exact same one; re-enter */
	if (v->current_order.IsType(OT_GOTO_DEPOT) &&
			IsShipDepotTile(v->tile) && GetDepotIndex(v->tile) == v->current_order.GetDestination()) {
		HandleShipEnterDepot(v);
		return true;
	}

	/* Don't leave depot if no destination set */
	if (v->dest_tile == 0) return true;

	if (IsExtendedDepot(v->tile)) {
		SetDepotReservation(v->tile, DEPOT_RESERVATION_EMPTY);
	} else {
		/* Don't leave depot if another vehicle is already entering/leaving */
		/* This helps avoid CPU load if many ships are set to start at the same time */
		if (HasVehicleOnPos(v->tile, nullptr, &EnsureNoMovingShipProc)) return true;

		TileIndex tile = v->tile;
		Axis axis = GetShipDepotAxis(tile);
		bool reverse = false;

		DiagDirection north_dir = ReverseDiagDir(AxisToDiagDir(axis));
		TileIndex north_neighbour = TileAdd(tile, TileOffsByDiagDir(north_dir));
		DiagDirection south_dir = AxisToDiagDir(axis);
		TileIndex south_neighbour = TileAdd(tile, 2 * TileOffsByDiagDir(south_dir));

		TrackBits north_tracks = DiagdirReachesTracks(north_dir) & GetTileShipTrackStatus(north_neighbour);
		TrackBits south_tracks = DiagdirReachesTracks(south_dir) & GetTileShipTrackStatus(south_neighbour);
		if (north_tracks && south_tracks) {
			if (CheckReverseShip(v)) north_tracks = TRACK_BIT_NONE;
		}

		if (north_tracks) {
			/* Leave towards north */
			v->rotation = v->direction = DiagDirToDir(north_dir);
		} else if (south_tracks) {
			/* Leave towards south */
			v->rotation = v->direction = DiagDirToDir(south_dir);
		} else {
			/* Both ways blocked */
			return false;
		}

		v->state = AxisToTrackBits(axis);
		v->vehstatus &= ~VS_HIDDEN;

		/* Leave towards south if reverse. */
		v->rotation = v->direction = DiagDirToDir(reverse ? south_dir : north_dir);

		v->state = AxisToTrackBits(axis);
		v->vehstatus &= ~VS_HIDDEN;
	}

	v->state &= ~TRACK_BIT_DEPOT;
	v->cur_speed = 0;
	v->UpdateViewport(true, true);
	DepotID depot_id = GetDepotIndex(v->tile);
	SetWindowDirty(WC_VEHICLE_DEPOT, depot_id);

	VehicleServiceInDepot(v);
	v->LeaveUnbunchingDepot();
	v->PlayLeaveStationSound();
	InvalidateWindowData(WC_VEHICLE_DEPOT, depot_id);
	SetWindowClassesDirty(WC_SHIPS_LIST);

	return false;
}

/**
 * Accelerates the ship towards its target speed.
 * @param v Ship to accelerate.
 * @return Number of steps to move the ship.
 */
static uint ShipAccelerate(Vehicle *v)
{
	uint speed;
	speed = std::min<uint>(v->cur_speed + v->acceleration, v->vcache.cached_max_speed);
	speed = std::min<uint>(speed, v->current_order.GetMaxSpeed() * 2);

	/* updates statusbar only if speed have changed to save CPU time */
	if (speed != v->cur_speed) {
		v->cur_speed = speed;
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);
	}

	const uint advance_speed = v->GetAdvanceSpeed(speed);
	const uint number_of_steps = (advance_speed + v->progress) / v->GetAdvanceDistance();
	const uint remainder = (advance_speed + v->progress) % v->GetAdvanceDistance();
	assert(remainder <= std::numeric_limits<uint8_t>::max());
	v->progress = static_cast<uint8_t>(remainder);
	return number_of_steps;
}

/**
 * Ship arrives at a dock. If it is the first time, send out a news item.
 * @param v  Ship that arrived.
 * @param st Station being visited.
 */
static void ShipArrivesAt(const Vehicle *v, Station *st)
{
	/* Check if station was ever visited before */
	if (!(st->had_vehicle_of_type & HVOT_SHIP)) {
		st->had_vehicle_of_type |= HVOT_SHIP;

		SetDParam(0, st->index);
		AddVehicleNewsItem(
			STR_NEWS_FIRST_SHIP_ARRIVAL,
			(v->owner == _local_company) ? NT_ARRIVAL_COMPANY : NT_ARRIVAL_OTHER,
			v->index,
			st->index
		);
		AI::NewEvent(v->owner, new ScriptEventStationFirstVehicle(st->index, v->index));
		Game::NewEvent(new ScriptEventStationFirstVehicle(st->index, v->index));
	}
}


/**
 * Runs the pathfinder to choose a track to continue along.
 *
 * @param v Ship to navigate
 * @param tile Tile, the ship is about to enter
 * @param tracks Available track choices on \a tile
 * @return Track to choose, or INVALID_TRACK when to reverse.
 */
static Track ChooseShipTrack(Ship *v, TileIndex tile, TrackBits tracks)
{
	/* Before choosing a track, if close to the destination station or depot (not an oil rig)... */
	if (DistanceManhattan(v->dest_tile, tile) <= 5 && (v->current_order.IsType(OT_GOTO_DEPOT) &&
			(!IsShipDepotTile(v->dest_tile) || (IsExtendedDepotTile(v->dest_tile) && !IsExtendedDepotEmpty(v->dest_tile))))) {
		/* Try to get a depot tile. */
		v->dest_tile = Depot::Get(v->current_order.GetDestination())->GetBestDepotTile(v);
	}

	bool path_found = true;
	Track track;

	if (v->dest_tile == 0) {
		/* No destination, don't invoke pathfinder. */
		track = TrackBitsToTrack(v->state);
		if (!IsDiagonalTrack(track)) track = TrackToOppositeTrack(track);
		if (!HasBit(tracks, track)) track = FindFirstTrack(tracks);
		path_found = false;
	} else {
		/* Attempt to follow cached path. */
		if (!v->path.empty()) {
			track = TrackdirToTrack(v->path.front());

			if (HasBit(tracks, track)) {
				v->path.pop_front();
				/* HandlePathfindResult() is not called here because this is not a new pathfinder result. */
				return track;
			}

			/* Cached path is invalid so continue with pathfinder. */
			v->path.clear();
		}

		track = YapfShipChooseTrack(v, tile, path_found, v->path);
	}

	v->HandlePathfindingResult(path_found);
	return track;
}

/**
 * Get the available water tracks on a tile for a ship entering a tile.
 * @param tile The tile about to enter.
 * @param dir The entry direction.
 * @return The available trackbits on the next tile.
 */
static inline TrackBits GetAvailShipTracks(TileIndex tile, DiagDirection dir)
{
	TrackBits tracks = GetTileShipTrackStatus(tile) & DiagdirReachesTracks(dir);

	return tracks;
}

/** Structure for ship sub-coordinate data for moving into a new tile via a Diagdir onto a Track. */
struct ShipSubcoordData {
	uint8_t x_subcoord; ///< New X sub-coordinate on the new tile
	uint8_t y_subcoord; ///< New Y sub-coordinate on the new tile
	Direction dir;   ///< New Direction to move in on the new track
};
/** Ship sub-coordinate data for moving into a new tile via a Diagdir onto a Track.
 * Array indexes are Diagdir, Track.
 * There will always be three possible tracks going into an adjacent tile via a Diagdir,
 * so each Diagdir sub-array will have three valid and three invalid structures per Track.
 */
static const ShipSubcoordData _ship_subcoord[DIAGDIR_END][TRACK_END] = {
	// DIAGDIR_NE
	{
		{15,  8, DIR_NE},      // TRACK_X
		{ 0,  0, INVALID_DIR}, // TRACK_Y
		{ 0,  0, INVALID_DIR}, // TRACK_UPPER
		{15,  8, DIR_E},       // TRACK_LOWER
		{15,  7, DIR_N},       // TRACK_LEFT
		{ 0,  0, INVALID_DIR}, // TRACK_RIGHT
	},
	// DIAGDIR_SE
	{
		{ 0,  0, INVALID_DIR}, // TRACK_X
		{ 8,  0, DIR_SE},      // TRACK_Y
		{ 7,  0, DIR_E},       // TRACK_UPPER
		{ 0,  0, INVALID_DIR}, // TRACK_LOWER
		{ 8,  0, DIR_S},       // TRACK_LEFT
		{ 0,  0, INVALID_DIR}, // TRACK_RIGHT
	},
	// DIAGDIR_SW
	{
		{ 0,  8, DIR_SW},      // TRACK_X
		{ 0,  0, INVALID_DIR}, // TRACK_Y
		{ 0,  7, DIR_W},       // TRACK_UPPER
		{ 0,  0, INVALID_DIR}, // TRACK_LOWER
		{ 0,  0, INVALID_DIR}, // TRACK_LEFT
		{ 0,  8, DIR_S},       // TRACK_RIGHT
	},
	// DIAGDIR_NW
	{
		{ 0,  0, INVALID_DIR}, // TRACK_X
		{ 8, 15, DIR_NW},      // TRACK_Y
		{ 0,  0, INVALID_DIR}, // TRACK_UPPER
		{ 8, 15, DIR_W},       // TRACK_LOWER
		{ 0,  0, INVALID_DIR}, // TRACK_LEFT
		{ 7, 15, DIR_N},       // TRACK_RIGHT
	}
};

/**
 * Test if a ship is in the centre of a lock and should move up or down.
 * @param v Ship being tested.
 * @return 0 if ship is not moving in lock, or -1 to move down, 1 to move up.
 */
static int ShipTestUpDownOnLock(const Ship *v)
{
	/* Suitable tile? */
	if (!IsTileType(v->tile, MP_WATER) || !IsLock(v->tile) || GetLockPart(v->tile) != LOCK_PART_MIDDLE) return 0;

	/* Must be at the centre of the lock */
	if ((v->x_pos & 0xF) != 8 || (v->y_pos & 0xF) != 8) return 0;

	DiagDirection diagdir = GetInclinedSlopeDirection(GetTileSlope(v->tile));
	assert(IsValidDiagDirection(diagdir));

	if (DirToDiagDir(v->direction) == diagdir) {
		/* Move up */
		return (v->z_pos < GetTileMaxZ(v->tile) * (int)TILE_HEIGHT) ? 1 : 0;
	} else {
		/* Move down */
		return (v->z_pos > GetTileZ(v->tile) * (int)TILE_HEIGHT) ? -1 : 0;
	}
}

/**
 * Test and move a ship up or down in a lock.
 * @param v Ship to move.
 * @return true iff ship is moving up or down in a lock.
 */
static bool ShipMoveUpDownOnLock(Ship *v)
{
	/* Moving up/down through lock */
	int dz = ShipTestUpDownOnLock(v);
	if (dz == 0) return false;

	if (v->cur_speed != 0) {
		v->cur_speed = 0;
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);
	}

	if ((v->tick_counter & 7) == 0) {
		v->z_pos += dz;
		v->UpdatePosition();
		v->UpdateViewport(true, true);
	}

	return true;
}

/**
 * Test if a tile is a docking tile for the given station.
 * @param tile Docking tile to test.
 * @param station Destination station.
 * @return true iff docking tile is next to station.
 */
bool IsShipDestinationTile(TileIndex tile, StationID station)
{
	assert(IsDockingTile(tile));
	/* Check each tile adjacent to docking tile. */
	for (DiagDirection d = DIAGDIR_BEGIN; d != DIAGDIR_END; d++) {
		TileIndex t = tile + TileOffsByDiagDir(d);
		if (!IsValidTile(t)) continue;
		if (IsDockTile(t) && GetStationIndex(t) == station && IsDockWaterPart(t)) return true;
		if (IsTileType(t, MP_INDUSTRY)) {
			const Industry *i = Industry::GetByTile(t);
			if (i->neutral_station != nullptr && i->neutral_station->index == station) return true;
		}
		if (IsTileType(t, MP_STATION) && IsOilRig(t) && GetStationIndex(t) == station) return true;
	}
	return false;
}

static void ReverseShipIntoTrackdir(Ship *v, Trackdir trackdir)
{
	static constexpr Direction _trackdir_to_direction[] = {
		DIR_NE, DIR_SE, DIR_E, DIR_E, DIR_S, DIR_S, INVALID_DIR, INVALID_DIR,
		DIR_SW, DIR_NW, DIR_W, DIR_W, DIR_N, DIR_N, INVALID_DIR, INVALID_DIR,
	};

	v->direction = _trackdir_to_direction[trackdir];
	assert(v->direction != INVALID_DIR);
	v->state = TrackdirBitsToTrackBits(TrackdirToTrackdirBits(trackdir));

	/* Remember our current location to avoid movement glitch */
	v->rotation_x_pos = v->x_pos;
	v->rotation_y_pos = v->y_pos;
	v->cur_speed = 0;
	v->path.clear();

	v->UpdatePosition();
	v->UpdateViewport(true, true);
}

static void ReverseShip(Ship *v)
{
	v->direction = ReverseDir(v->direction);

	/* Remember our current location to avoid movement glitch */
	v->rotation_x_pos = v->x_pos;
	v->rotation_y_pos = v->y_pos;
	v->cur_speed = 0;
	v->path.clear();

	v->UpdatePosition();
	v->UpdateViewport(true, true);
}

static void ShipController(Ship *v)
{
	v->tick_counter++;
	v->current_order_time++;

	if (v->HandleBreakdown()) return;

	if (v->vehstatus & VS_STOPPED) return;

	if (v->ContinueServicing()) return;

	if (ProcessOrders(v) && CheckReverseShip(v)) return ReverseShip(v);

	v->HandleLoading();

	if (v->current_order.IsType(OT_LOADING)) return;

	if (CheckShipLeaveDepot(v)) return;

	v->ShowVisualEffect();

	/* Rotating on spot */
	if (v->direction != v->rotation) {
		if ((v->tick_counter & 7) == 0) {
			DirDiff diff = DirDifference(v->direction, v->rotation);
			v->rotation = ChangeDir(v->rotation, diff > DIRDIFF_REVERSE ? DIRDIFF_45LEFT : DIRDIFF_45RIGHT);
			/* Invalidate the sprite cache direction to force recalculation of viewport */
			v->sprite_cache.last_direction = INVALID_DIR;
			v->UpdateViewport(true, true);
		}
		return;
	}

	if (ShipMoveUpDownOnLock(v)) return;

	const uint number_of_steps = ShipAccelerate(v);
	for (uint i = 0; i < number_of_steps; ++i) {
		if (ShipMoveUpDownOnLock(v)) return;

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
					const VehicleEnterTileStatus r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
					if (HasBit(r, VETS_CANNOT_ENTER)) return ReverseShip(v);

					/* A leave station order only needs one tick to get processed, so we can
					 * always skip ahead. */
					if (v->current_order.IsType(OT_LEAVESTATION)) {
						v->current_order.Free();
						SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);
						/* Test if continuing forward would lead to a dead-end, moving into the dock. */
						const DiagDirection exitdir = VehicleExitDir(v->direction, v->state);
						const TileIndex tile = TileAddByDiagDir(v->tile, exitdir);
						if (TrackStatusToTrackBits(GetTileTrackStatus(tile, TRANSPORT_WATER, 0, exitdir)) == TRACK_BIT_NONE) return ReverseShip(v);
					} else if (v->dest_tile != 0) {
						/* We have a target, let's see if we reached it... */
						if (v->current_order.IsType(OT_GOTO_WAYPOINT) &&
								DistanceManhattan(v->dest_tile, gp.new_tile) <= 3) {
							/* We got within 3 tiles of our target buoy, so let's skip to our
							 * next order */
							UpdateVehicleTimetable(v, true);
							v->IncrementRealOrderIndex();
							v->current_order.MakeDummy();
						} else if (v->current_order.IsType(OT_GOTO_STATION) && IsDockingTile(gp.new_tile)) {
							/* Process station in the orderlist. */
							Station *st = Station::Get(v->current_order.GetDestination());
							if (st->docking_station.Contains(gp.new_tile) && IsShipDestinationTile(gp.new_tile, st->index)) {
								v->last_station_visited = st->index;
								if (st->facilities & FACIL_DOCK) { // ugly, ugly workaround for problem with ships able to drop off cargo at wrong stations
									ShipArrivesAt(v, st);
									v->BeginLoading();
								} else { // leave stations without docks right away
									v->current_order.MakeLeaveStation();
									v->IncrementRealOrderIndex();
								}
							}
						}
					}
				}
			} else {
				/* New tile */
				if (!IsValidTile(gp.new_tile)) return ReverseShip(v);

				if (v->current_order.IsType(OT_GOTO_DEPOT) &&
							IsShipDepotTile(gp.new_tile) &&
							GetOtherShipDepotTile(gp.new_tile) == gp.old_tile &&
							v->current_order.GetDestination() == GetDepotIndex(gp.new_tile)) {
					if (CheckPlaceShipOnDepot(v->tile)) {
						HandleShipEnterDepot(v);
						v->UpdatePosition();
						v->UpdateViewport(true, true);
						return;
					}
				}

				const DiagDirection diagdir = DiagdirBetweenTiles(gp.old_tile, gp.new_tile);
				assert(diagdir != INVALID_DIAGDIR);
				const TrackBits tracks = GetAvailShipTracks(gp.new_tile, diagdir);
				if (tracks == TRACK_BIT_NONE) {
					Trackdir trackdir = INVALID_TRACKDIR;
					CheckReverseShip(v, &trackdir);
					if (trackdir == INVALID_TRACKDIR) return ReverseShip(v);
					return ReverseShipIntoTrackdir(v, trackdir);
				}

				/* Choose a direction, and continue if we find one */
				const Track track = ChooseShipTrack(v, gp.new_tile, tracks);
				if (track == INVALID_TRACK) return ReverseShip(v);

				const ShipSubcoordData &b = _ship_subcoord[diagdir][track];

				gp.x = (gp.x & ~0xF) | b.x_subcoord;
				gp.y = (gp.y & ~0xF) | b.y_subcoord;

				/* Call the landscape function and tell it that the vehicle entered the tile */
				const VehicleEnterTileStatus r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
				if (HasBit(r, VETS_CANNOT_ENTER)) return ReverseShip(v);

				if (!HasBit(r, VETS_ENTERED_WORMHOLE)) {
					v->tile = gp.new_tile;
					v->state = TrackToTrackBits(track);

					/* Update ship cache when the water class changes. Aqueducts are always canals. */
					if (GetEffectiveWaterClass(gp.old_tile) != GetEffectiveWaterClass(gp.new_tile)) v->UpdateCache();
				}

				const Direction new_direction = b.dir;
				const DirDiff diff = DirDifference(new_direction, v->direction);
				switch (diff) {
					case DIRDIFF_SAME:
					case DIRDIFF_45RIGHT:
					case DIRDIFF_45LEFT:
						/* Continue at speed */
						v->rotation = v->direction = new_direction;
						break;

					default:
						/* Stop for rotation */
						v->cur_speed = 0;
						v->direction = new_direction;
						/* Remember our current location to avoid movement glitch */
						v->rotation_x_pos = v->x_pos;
						v->rotation_y_pos = v->y_pos;
						break;
				}
			}
		} else {
			/* On a bridge */
			if (!IsTileType(gp.new_tile, MP_TUNNELBRIDGE) || !HasBit(VehicleEnterTile(v, gp.new_tile, gp.x, gp.y), VETS_ENTERED_WORMHOLE)) {
				v->x_pos = gp.x;
				v->y_pos = gp.y;
				v->UpdatePosition();
				if ((v->vehstatus & VS_HIDDEN) == 0) v->Vehicle::UpdateViewport(true);
				return;
			}

			/* Ship is back on the bridge head, we need to consume its path
			 * cache entry here as we didn't have to choose a ship track. */
			if (!v->path.empty()) v->path.pop_front();
		}

		/* update image of ship, as well as delta XY */
		v->x_pos = gp.x;
		v->y_pos = gp.y;

		v->UpdatePosition();
		v->UpdateViewport(true, true);
	}
}

bool Ship::Tick()
{
	PerformanceAccumulator framerate(PFE_GL_SHIPS);

	if (!(this->vehstatus & VS_STOPPED)) this->running_ticks++;

	ShipController(this);

	return true;
}

void Ship::SetDestTile(TileIndex tile)
{
	if (tile == this->dest_tile) return;
	this->path.clear();
	this->dest_tile = tile;
}

/**
 * Build a ship.
 * @param flags    type of operation.
 * @param tile     tile of the depot where ship is built.
 * @param e        the engine to build.
 * @param[out] ret the vehicle that has been built.
 * @return the cost of this operation or an error.
 */
CommandCost CmdBuildShip(DoCommandFlag flags, TileIndex tile, const Engine *e, Vehicle **ret)
{
	assert(IsShipDepotTile(tile));
	if (!(flags & DC_AUTOREPLACE)) {
		std::vector<TileIndex> *depot_tiles = &(Depot::GetByTile(tile)->depot_tiles);
		tile = INVALID_TILE;
		for (std::vector<TileIndex>::iterator it = depot_tiles->begin(); it != depot_tiles->end(); ++it) {
			if (CheckPlaceShipOnDepot(*it)) {
				tile = *it;
				break;
			}
		}
		if (tile == INVALID_TILE) return_cmd_error(STR_ERROR_NO_FREE_DEPOT);
	}

	if (flags & DC_EXEC) {
		bool is_extended_depot = IsExtendedDepot(tile);
		TileIndexDiffC offset = TileIndexDiffCByDiagDir(ReverseDiagDir(GetShipDepotDirection(tile)));

		const ShipVehicleInfo *svi = &e->u.ship;

		Ship *v = new Ship();
		*ret = v;

		v->owner = _current_company;
		v->tile = tile;
		v->x_pos = TileX(tile) * TILE_SIZE + TILE_SIZE / 2 + offset.x * (TILE_SIZE / 2 - 1);
		v->y_pos = TileY(tile) * TILE_SIZE + TILE_SIZE / 2 + offset.y * (TILE_SIZE / 2 - 1);
		v->z_pos = GetSlopePixelZ(v->x_pos, v->y_pos);
		v->state = TRACK_BIT_DEPOT;

		if (is_extended_depot) {
			v->state |= AxisToTrackBits(GetShipDepotAxis(tile));
			v->direction = AxisToDirection(GetShipDepotAxis(v->tile));
			SetDepotReservation(v->tile, DEPOT_RESERVATION_FULL_STOPPED_VEH);
		} else {
			v->vehstatus |= VS_HIDDEN;
		}

		v->rotation = v->direction;
		v->UpdateDeltaXY();
		v->vehstatus |= VS_STOPPED | VS_DEFPAL;

		v->spritenum = svi->image_index;
		v->cargo_type = e->GetDefaultCargoType();
		assert(IsValidCargoID(v->cargo_type));
		v->cargo_cap = svi->capacity;
		v->refit_cap = 0;

		v->last_station_visited = INVALID_STATION;
		v->last_loading_station = INVALID_STATION;
		v->engine_type = e->index;

		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->GetLifeLengthInDays();

		v->SetServiceInterval(Company::Get(_current_company)->settings.vehicle.servint_ships);
		v->date_of_last_service = TimerGameEconomy::date;
		v->date_of_last_service_newgrf = TimerGameCalendar::date;
		v->build_year = TimerGameCalendar::year;
		v->sprite_cache.sprite_seq.Set(SPR_IMG_QUERY);
		v->random_bits = Random();

		v->acceleration = svi->acceleration;
		v->UpdateCache();

		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) SetBit(v->vehicle_flags, VF_BUILT_AS_PROTOTYPE);
		v->SetServiceIntervalIsPercent(Company::Get(_current_company)->settings.vehicle.servint_ispercent);

		v->InvalidateNewGRFCacheOfChain();

		v->cargo_cap = e->DetermineCapacity(v);

		v->InvalidateNewGRFCacheOfChain();

		v->UpdatePosition();

		if (is_extended_depot) v->MarkDirty();
	}

	return CommandCost();
}

ClosestDepot Ship::FindClosestDepot()
{
	const Depot *depot = FindClosestShipDepot(this, MAX_SHIP_DEPOT_SEARCH_DISTANCE);
	if (depot == nullptr) return ClosestDepot();

	return ClosestDepot(depot->GetBestDepotTile(this), depot->index);
}
