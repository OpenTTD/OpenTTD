/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "aircraft.h"
#include "debug.h"
#include "functions.h"
#include "station_map.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "vehicle.h"
#include "depot.h"
#include "engine.h"
#include "command.h"
#include "station.h"
#include "news.h"
#include "sound.h"
#include "player.h"
#include "airport.h"
#include "vehicle_gui.h"
#include "table/sprites.h"
#include "newgrf_engine.h"
#include "newgrf_callbacks.h"
#include "newgrf_text.h"
#include "newgrf_sound.h"
#include "date.h"

static bool AirportMove(Vehicle *v, const AirportFTAClass *apc);
static bool AirportSetBlocks(Vehicle *v, AirportFTA *current_pos, const AirportFTAClass *apc);
static bool AirportHasBlock(Vehicle *v, const AirportFTA *current_pos, const AirportFTAClass *apc);
static bool AirportFindFreeTerminal(Vehicle *v, const AirportFTAClass *apc);
static bool AirportFindFreeHelipad(Vehicle *v, const AirportFTAClass *apc);
static void AirportGoToNextPosition(Vehicle *v, const AirportFTAClass *apc);
static void CrashAirplane(Vehicle *v);

static void AircraftNextAirportPos_and_Order(Vehicle *v);
static byte GetAircraftFlyingAltitude(const Vehicle *v);

static const SpriteID _aircraft_sprite[] = {
	0x0EB5, 0x0EBD, 0x0EC5, 0x0ECD,
	0x0ED5, 0x0EDD, 0x0E9D, 0x0EA5,
	0x0EAD, 0x0EE5, 0x0F05, 0x0F0D,
	0x0F15, 0x0F1D, 0x0F25, 0x0F2D,
	0x0EED, 0x0EF5, 0x0EFD, 0x0F35,
	0x0E9D, 0x0EA5, 0x0EAD, 0x0EB5,
	0x0EBD, 0x0EC5
};

/* Helicopter rotor animation states */
enum HelicopterRotorStates {
	HRS_ROTOR_STOPPED,
	HRS_ROTOR_MOVING_1,
	HRS_ROTOR_MOVING_2,
	HRS_ROTOR_MOVING_3,
};

/* Find the nearest hangar to v
 * INVALID_STATION is returned, if the player does not have any suitable
 * airports (like helipads only)
 */
static StationID FindNearestHangar(const Vehicle *v)
{
	const Station *st;
	uint best = 0;
	StationID index = INVALID_STATION;
	TileIndex vtile = TileVirtXY(v->x_pos, v->y_pos);

	FOR_ALL_STATIONS(st) {
		if (st->owner == v->owner && st->facilities & FACIL_AIRPORT &&
				GetAirport(st->airport_type)->nof_depots > 0) {
			uint distance;

			// don't crash the plane if we know it can't land at the airport
			if ((AircraftVehInfo(v->engine_type)->subtype & AIR_FAST) &&
					(st->airport_type == AT_SMALL || st->airport_type == AT_COMMUTER) &&
					!_cheats.no_jetcrash.value)
				continue;

			// v->tile can't be used here, when aircraft is flying v->tile is set to 0
			distance = DistanceSquare(vtile, st->airport_tile);
			if (distance < best || index == INVALID_STATION) {
				best = distance;
				index = st->index;
			}
		}
	}
	return index;
}

#if 0
// returns true if vehicle v have an airport in the schedule, that has a hangar
static bool HaveHangarInOrderList(Vehicle *v)
{
	const Order *order;

	FOR_VEHICLE_ORDERS(v, order) {
		const Station *st = GetStation(order->station);
		if (st->owner == v->owner && st->facilities & FACIL_AIRPORT) {
			// If an airport doesn't have a hangar, skip it
			if (GetAirport(st->airport_type)->nof_depots != 0)
				return true;
		}
	}

	return false;
}
#endif

int GetAircraftImage(const Vehicle* v, Direction direction)
{
	int spritenum = v->spritenum;

	if (is_custom_sprite(spritenum)) {
		int sprite = GetCustomVehicleSprite(v, direction);

		if (sprite != 0) return sprite;
		spritenum = orig_aircraft_vehicle_info[v->engine_type - AIRCRAFT_ENGINES_INDEX].image_index;
	}
	return direction + _aircraft_sprite[spritenum];
}

SpriteID GetRotorImage(const Vehicle *v)
{
	const Vehicle *w;

	assert((v->subtype & 1) == 0);

	w = v->next->next;
	if (is_custom_sprite(v->spritenum)) {
		SpriteID spritenum = GetCustomRotorSprite(v, false);
		if (spritenum != 0) return spritenum;
	}

	/* Return standard rotor sprites if there are no custom sprites for this helicopter */
	return SPR_ROTOR_STOPPED + w->u.air.state;
}

void DrawAircraftEngine(int x, int y, EngineID engine, uint32 image_ormod)
{
	const AircraftVehicleInfo* avi = AircraftVehInfo(engine);
	int spritenum = avi->image_index;
	int sprite = (6 + _aircraft_sprite[spritenum]);

	if (is_custom_sprite(spritenum)) {
		sprite = GetCustomVehicleIcon(engine, DIR_W);
		if (sprite == 0) {
			spritenum = orig_aircraft_vehicle_info[engine - AIRCRAFT_ENGINES_INDEX].image_index;
			sprite = (6 + _aircraft_sprite[spritenum]);
		}
	}

	DrawSprite(sprite | image_ormod, x, y);

	if (!(avi->subtype & AIR_CTOL)) {
		SpriteID rotor_sprite = GetCustomRotorIcon(engine);
		if (rotor_sprite == 0) rotor_sprite = SPR_ROTOR_STOPPED;
		DrawSprite(rotor_sprite, x, y - 5);
	}
}

static int32 EstimateAircraftCost(EngineID engine_type)
{
	return AircraftVehInfo(engine_type)->base_cost * (_price.aircraft_base>>3)>>5;
}


/**
 * Calculates cargo capacity based on an aircraft's passenger
 * and mail capacities.
 * @param cid Which cargo type to calculate a capacity for.
 * @param engine Which engine to find a cargo capacity for.
 * @return New cargo capacity value.
 */
uint16 AircraftDefaultCargoCapacity(CargoID cid, EngineID engine_type)
{
	const AircraftVehicleInfo *avi = AircraftVehInfo(engine_type);

	assert(cid != CT_INVALID);

	/* An aircraft can carry twice as much goods as normal cargo,
	 * and four times as many passengers. */
	switch (cid) {
		case CT_PASSENGERS:
			return avi->passenger_capacity;
		case CT_MAIL:
			return avi->passenger_capacity + avi->mail_capacity;
		case CT_GOODS:
			return (avi->passenger_capacity + avi->mail_capacity) / 2;
		default:
			return (avi->passenger_capacity + avi->mail_capacity) / 4;
	}
}


/** Build an aircraft.
 * @param tile tile of depot where aircraft is built
 * @param p1 aircraft type being built (engine)
 * @param p2 bit 0 when set, the unitnumber will be 0, otherwise it will be a free number
 */
int32 CmdBuildAircraft(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int32 value;
	Vehicle *vl[3], *v, *u, *w;
	UnitID unit_num;
	const AircraftVehicleInfo *avi;
	const AirportFTAClass* ap;
	Engine *e;

	if (!IsEngineBuildable(p1, VEH_Aircraft, _current_player)) return_cmd_error(STR_ENGINE_NOT_BUILDABLE);

	value = EstimateAircraftCost(p1);

	// to just query the cost, it is not neccessary to have a valid tile (automation/AI)
	if (flags & DC_QUERY_COST) return value;

	if (!IsHangarTile(tile) || !IsTileOwner(tile, _current_player)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	avi = AircraftVehInfo(p1);

	// Prevent building aircraft types at places which can't handle them
	ap = GetAirport(GetStationByTile(tile)->airport_type);
	if ((avi->subtype & AIR_CTOL ? HELICOPTERS_ONLY : AIRCRAFT_ONLY) == ap->acc_planes) {
		return CMD_ERROR;
	}

	// allocate 2 or 3 vehicle structs, depending on type
	if (!AllocateVehicles(vl, avi->subtype & AIR_CTOL ? 2 : 3) ||
			IsOrderPoolFull()) {
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);
	}

	unit_num = HASBIT(p2, 0) ? 0 : GetFreeUnitNumber(VEH_Aircraft);
	if (unit_num > _patches.max_aircraft)
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

	if (flags & DC_EXEC) {
		CargoID cargo;
		uint x;
		uint y;

		v = vl[0];
		u = vl[1];

		v->unitnumber = unit_num;
		v->type = u->type = VEH_Aircraft;
		v->direction = 3;

		v->owner = u->owner = _current_player;

		v->tile = tile;
//		u->tile = 0;

		x = TileX(tile) * TILE_SIZE + 5;
		y = TileY(tile) * TILE_SIZE + 3;

		v->x_pos = u->x_pos = x;
		v->y_pos = u->y_pos = y;

		u->z_pos = GetSlopeZ(x, y);
		v->z_pos = u->z_pos + 1;

		v->x_offs = v->y_offs = -1;
//		u->delta_x = u->delta_y = 0;

		v->sprite_width = v->sprite_height = 2;
		v->z_height = 5;

		u->sprite_width = u->sprite_height = 2;
		u->z_height = 1;

		v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;
		u->vehstatus = VS_HIDDEN | VS_UNCLICKABLE | VS_SHADOW;

		v->spritenum = avi->image_index;
//		v->cargo_count = u->number_of_pieces = 0;

		v->cargo_cap = avi->passenger_capacity;
		u->cargo_cap = avi->mail_capacity;

		v->cargo_type = CT_PASSENGERS;
		u->cargo_type = CT_MAIL;

		v->cargo_subtype = 0;

		v->string_id = STR_SV_AIRCRAFT_NAME;
//		v->next_order_param = v->next_order = 0;

//		v->load_unload_time_rem = 0;
//		v->progress = 0;
		v->last_station_visited = INVALID_STATION;
//		v->destination_coords = 0;

		v->max_speed = avi->max_speed;
		v->acceleration = avi->acceleration;
		v->engine_type = p1;

		v->subtype = (avi->subtype & AIR_CTOL ? 2 : 0);
		v->value = value;

		u->subtype = 4;

		/* Danger, Will Robinson!
		 * If the aircraft is refittable, but cannot be refitted to
		 * passengers, we select the cargo type from the refit mask.
		 * This is a fairly nasty hack to get around the fact that TTD
		 * has no default cargo type specifier for planes... */
		cargo = FindFirstRefittableCargo(p1);
		if (cargo != CT_INVALID && cargo != CT_PASSENGERS) {
			uint16 callback = CALLBACK_FAILED;

			v->cargo_type = cargo;

			if (HASBIT(EngInfo(p1)->callbackmask, CBM_REFIT_CAPACITY)) {
				callback = GetVehicleCallback(CBID_VEHICLE_REFIT_CAPACITY, 0, 0, v->engine_type, v);
			}

			if (callback == CALLBACK_FAILED) {
				/* Callback failed, or not executed; use the default cargo capacity */
				v->cargo_cap = AircraftDefaultCargoCapacity(v->cargo_type, v->engine_type);
			} else {
				v->cargo_cap = callback;
			}

			/* Set the 'second compartent' capacity to none */
			u->cargo_cap = 0;
		}

		e = GetEngine(p1);
		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->lifelength * 366;

		_new_vehicle_id = v->index;

		v->u.air.pos = MAX_ELEMENTS;

		/* When we click on hangar we know the tile it is on. By that we know
		 * its position in the array of depots the airport has.....we can search
		 * layout for #th position of depot. Since layout must start with a listing
		 * of all depots, it is simple */
		{
			const Station* st = GetStationByTile(tile);
			const AirportFTAClass* apc = GetAirport(st->airport_type);
			uint i;

			for (i = 0; i < apc->nof_depots; i++) {
				if (st->airport_tile + ToTileIndexDiff(apc->airport_depots[i]) == tile) {
					assert(apc->layout[i].heading == HANGAR);
					v->u.air.pos = apc->layout[i].position;
					break;
				}
			}
			// to ensure v->u.air.pos has been given a value
			assert(v->u.air.pos != MAX_ELEMENTS);
		}

		v->u.air.state = HANGAR;
		v->u.air.previous_pos = v->u.air.pos;
		v->u.air.targetairport = GetStationIndex(tile);
		v->next = u;

		v->service_interval = _patches.servint_aircraft;

		v->date_of_last_service = _date;
		v->build_year = u->build_year = _cur_year;

		v->cur_image = u->cur_image = 0xEA0;

		v->random_bits = VehicleRandomBits();
		u->random_bits = VehicleRandomBits();

		VehiclePositionChanged(v);
		VehiclePositionChanged(u);

		// Aircraft with 3 vehicles (chopper)?
		if (v->subtype == 0) {
			w = vl[2];

			u->next = w;

			w->type = VEH_Aircraft;
			w->direction = 0;
			w->owner = _current_player;
			w->x_pos = v->x_pos;
			w->y_pos = v->y_pos;
			w->z_pos = v->z_pos + 5;
			w->x_offs = w->y_offs = -1;
			w->sprite_width = w->sprite_height = 2;
			w->z_height = 1;
			w->vehstatus = VS_HIDDEN | VS_UNCLICKABLE;
			w->spritenum = 0xFF;
			w->subtype = 6;
			w->cur_image = SPR_ROTOR_STOPPED;
			w->random_bits = VehicleRandomBits();
			/* Use rotor's air.state to store the rotor animation frame */
			w->u.air.state = HRS_ROTOR_STOPPED;
			VehiclePositionChanged(w);
		}
		GetPlayer(_current_player)->num_engines[p1]++;

		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
		InvalidateWindow(WC_COMPANY, v->owner);
		if (IsLocalPlayer())
			InvalidateWindow(WC_REPLACE_VEHICLE, VEH_Aircraft); //updates the replace Aircraft window
	}

	return value;
}


static void DoDeleteAircraft(Vehicle *v)
{
	DeleteWindowById(WC_VEHICLE_VIEW, v->index);
	RebuildVehicleLists();
	InvalidateWindow(WC_COMPANY, v->owner);
	DeleteDepotHighlightOfVehicle(v);
	DeleteVehicleChain(v);
	InvalidateWindowClasses(WC_AIRCRAFT_LIST);
}

/** Sell an aircraft.
 * @param tile unused
 * @param p1 vehicle ID to be sold
 * @param p2 unused
 */
int32 CmdSellAircraft(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Aircraft || !CheckOwnership(v->owner)) return CMD_ERROR;
	if (!IsAircraftInHangarStopped(v)) return_cmd_error(STR_A01B_AIRCRAFT_MUST_BE_STOPPED);

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	if (flags & DC_EXEC) {
		// Invalidate depot
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		DoDeleteAircraft(v);
		if (IsLocalPlayer())
			InvalidateWindow(WC_REPLACE_VEHICLE, VEH_Aircraft); // updates the replace Aircraft window
	}

	return -(int32)v->value;
}

/** Start/Stop an aircraft.
 * @param tile unused
 * @param p1 aircraft ID to start/stop
 * @param p2 unused
 */
int32 CmdStartStopAircraft(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	uint16 callback;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Aircraft || !CheckOwnership(v->owner)) return CMD_ERROR;

	// cannot stop airplane when in flight, or when taking off / landing
	if (v->u.air.state >= STARTTAKEOFF && v->u.air.state < TERM7)
		return_cmd_error(STR_A017_AIRCRAFT_IS_IN_FLIGHT);

	/* Check if this aircraft can be started/stopped. The callback will fail or
	 * return 0xFF if it can. */
	callback = GetVehicleCallback(CBID_VEHICLE_START_STOP_CHECK, 0, 0, v->engine_type, v);
	if (callback != CALLBACK_FAILED && callback != 0xFF) {
		StringID error = GetGRFStringID(GetEngineGRFID(v->engine_type), 0xD000 + callback);
		return_cmd_error(error);
	}

	if (flags & DC_EXEC) {
		if (IsAircraftInHangarStopped(v)) {
			DeleteVehicleNews(p1, STR_A014_AIRCRAFT_IS_WAITING_IN);
		}

		v->vehstatus ^= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		InvalidateWindowClasses(WC_AIRCRAFT_LIST);
	}

	return 0;
}

/** Send an aircraft to the hangar.
 * @param tile unused
 * @param p1 vehicle ID to send to the hangar
 * @param p2 various bitmasked elements
 * - p2 bit 0-3 - DEPOT_ flags (see vehicle.h)
 * - p2 bit 8-10 - VLW flag (for mass goto depot)
 */
int32 CmdSendAircraftToHangar(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (p2 & DEPOT_MASS_SEND) {
		/* Mass goto depot requested */
		if (!ValidVLWFlags(p2 & VLW_MASK)) return CMD_ERROR;
		return SendAllVehiclesToDepot(VEH_Aircraft, flags, p2 & DEPOT_SERVICE, _current_player, (p2 & VLW_MASK), p1);
	}

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Aircraft || !CheckOwnership(v->owner) || IsAircraftInHangar(v)) return CMD_ERROR;

	if (v->current_order.type == OT_GOTO_DEPOT && !(p2 & DEPOT_LOCATE_HANGAR)) {
		if (!!(p2 & DEPOT_SERVICE) == HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT)) {
			/* We called with a different DEPOT_SERVICE setting.
			 * Now we change the setting to apply the new one and let the vehicle head for the same hangar.
			 * Note: the if is (true for requesting service == true for ordered to stop in hangar) */
			if (flags & DC_EXEC) {
				TOGGLEBIT(v->current_order.flags, OFB_HALT_IN_DEPOT);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
			}
			return 0;
		}

		if (p2 & DEPOT_DONT_CANCEL) return CMD_ERROR; // Requested no cancelation of hangar orders
		if (flags & DC_EXEC) {
			if (v->current_order.flags & OF_UNLOAD) v->cur_order_index++;
			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		}
	} else {
		bool next_airport_has_hangar = true;
		StationID next_airport_index = v->u.air.targetairport;
		const Station *st = GetStation(next_airport_index);
		/* If the station is not a valid airport or if it has no hangars */
		if (!IsValidStation(st) || st->airport_tile == 0 || GetAirport(st->airport_type)->nof_depots == 0) {
			StationID station;

			// the aircraft has to search for a hangar on its own
			station = FindNearestHangar(v);

			next_airport_has_hangar = false;
			if (station == INVALID_STATION) return CMD_ERROR;
			st = GetStation(station);
			next_airport_index = station;

		}

		if (flags & DC_EXEC) {
			v->current_order.type = OT_GOTO_DEPOT;
			v->current_order.flags = OF_NON_STOP;
			if (!(p2 & DEPOT_SERVICE)) SETBIT(v->current_order.flags, OFB_HALT_IN_DEPOT);
			v->current_order.refit_cargo = CT_INVALID;
			v->current_order.dest = next_airport_index;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
			if (v->u.air.state == FLYING && !next_airport_has_hangar) {
				/* The aircraft is now heading for a different hangar than the next in the orders */
				AircraftNextAirportPos_and_Order(v);
				v->u.air.targetairport = next_airport_index;
			}
		}
	}

	return 0;
}


/** Refits an aircraft to the specified cargo type.
 * @param tile unused
 * @param p1 vehicle ID of the aircraft to refit
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0-7) - the new cargo type to refit to
 * - p2 = (bit 8-15) - the new cargo subtype to refit to
 */
int32 CmdRefitAircraft(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	int pass, mail;
	int32 cost;
	CargoID new_cid = GB(p2, 0, 8);
	byte new_subtype = GB(p2, 8, 8);
	const AircraftVehicleInfo *avi;
	uint16 callback = CALLBACK_FAILED;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Aircraft || !CheckOwnership(v->owner)) return CMD_ERROR;
	if (!IsAircraftInHangarStopped(v)) return_cmd_error(STR_A01B_AIRCRAFT_MUST_BE_STOPPED);

	avi = AircraftVehInfo(v->engine_type);

	/* Check cargo */
	if (new_cid > NUM_CARGO || !CanRefitTo(v->engine_type, new_cid)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_AIRCRAFT_RUN);

	/* Check the refit capacity callback */
	if (HASBIT(EngInfo(v->engine_type)->callbackmask, CBM_REFIT_CAPACITY)) {
		/* Back up the existing cargo type */
		CargoID temp_cid = v->cargo_type;
		byte temp_subtype = v->cargo_subtype;
		v->cargo_type = new_cid;
		v->cargo_subtype = new_subtype;

		callback = GetVehicleCallback(CBID_VEHICLE_REFIT_CAPACITY, 0, 0, v->engine_type, v);

		/* Restore the cargo type */
		v->cargo_type = temp_cid;
		v->cargo_subtype = temp_subtype;
	}

	if (callback == CALLBACK_FAILED) {
		/* If the callback failed, or wasn't executed, use the aircraft's
		 * default cargo capacity */
		pass = AircraftDefaultCargoCapacity(new_cid, v->engine_type);
	} else {
		pass = callback;
	}
	_returned_refit_capacity = pass;

	cost = 0;
	if (IsHumanPlayer(v->owner) && new_cid != v->cargo_type) {
		cost = GetRefitCost(v->engine_type);
	}

	if (flags & DC_EXEC) {
		Vehicle *u;
		v->cargo_cap = pass;

		u = v->next;
		mail = (new_cid != CT_PASSENGERS) ? 0 : avi->mail_capacity;
		u->cargo_cap = mail;
		if (v->cargo_type == new_cid) {
			v->cargo_count = min(pass, v->cargo_count);
			u->cargo_count = min(mail, u->cargo_count);
		} else {
			v->cargo_count = 0;
			u->cargo_count = 0;
		}
		v->cargo_type = new_cid;
		v->cargo_subtype = new_subtype;
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
	}

	return cost;
}


static void CheckIfAircraftNeedsService(Vehicle *v)
{
	const Station* st;

	if (_patches.servint_aircraft == 0) return;
	if (!VehicleNeedsService(v)) return;
	if (v->vehstatus & VS_STOPPED) return;

	if (v->current_order.type == OT_GOTO_DEPOT &&
			v->current_order.flags & OF_HALT_IN_DEPOT)
		return;

	if (_patches.gotodepot && VehicleHasDepotOrders(v)) return;

	if (IsAircraftInHangar(v)) {
		VehicleServiceInDepot(v);
		return;
	}

	st = GetStation(v->current_order.dest);
	// only goto depot if the target airport has terminals (eg. it is airport)
	if (IsValidStation(st) && st->airport_tile != 0 && GetAirport(st->airport_type)->terminals != NULL) {
//		printf("targetairport = %d, st->index = %d\n", v->u.air.targetairport, st->index);
//		v->u.air.targetairport = st->index;
		v->current_order.type = OT_GOTO_DEPOT;
		v->current_order.flags = OF_NON_STOP;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	} else if (v->current_order.type == OT_GOTO_DEPOT) {
		v->current_order.type = OT_DUMMY;
		v->current_order.flags = 0;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	}
}

void OnNewDay_Aircraft(Vehicle *v)
{
	int32 cost;

	if (v->subtype > 2) return;

	if ((++v->day_counter & 7) == 0) DecreaseVehicleValue(v);

	CheckOrders(v);

	CheckVehicleBreakdown(v);
	AgeVehicle(v);
	CheckIfAircraftNeedsService(v);

	if (v->vehstatus & VS_STOPPED) return;

	cost = AircraftVehInfo(v->engine_type)->running_cost * _price.aircraft_running / 364;

	v->profit_this_year -= cost >> 8;

	SET_EXPENSES_TYPE(EXPENSES_AIRCRAFT_RUN);
	SubtractMoneyFromPlayerFract(v->owner, cost);

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	InvalidateWindowClasses(WC_AIRCRAFT_LIST);
}

void AircraftYearlyLoop(void)
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Aircraft && v->subtype <= 2) {
			v->profit_last_year = v->profit_this_year;
			v->profit_this_year = 0;
			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		}
	}
}

static void AgeAircraftCargo(Vehicle *v)
{
	if (_age_cargo_skip_counter != 0) return;

	do {
		if (v->cargo_days != 0xFF) v->cargo_days++;
		v = v->next;
	} while (v != NULL);
}

static void HelicopterTickHandler(Vehicle *v)
{
	Vehicle *u;
	int tick,spd;
	SpriteID img;

	u = v->next->next;

	if (u->vehstatus & VS_HIDDEN) return;

	// if true, helicopter rotors do not rotate. This should only be the case if a helicopter is
	// loading/unloading at a terminal or stopped
	if (v->current_order.type == OT_LOADING || (v->vehstatus & VS_STOPPED)) {
		if (u->cur_speed != 0) {
			u->cur_speed++;
			if (u->cur_speed >= 0x80 && u->u.air.state == HRS_ROTOR_MOVING_3) {
				u->cur_speed = 0;
			}
		}
	} else {
		if (u->cur_speed == 0)
			u->cur_speed = 0x70;

		if (u->cur_speed >= 0x50)
			u->cur_speed--;
	}

	tick = ++u->tick_counter;
	spd = u->cur_speed >> 4;

	if (spd == 0) {
		u->u.air.state = HRS_ROTOR_STOPPED;
		img = GetRotorImage(v);
		if (u->cur_image == img) return;
	} else if (tick >= spd) {
		u->tick_counter = 0;
		u->u.air.state++;
		if (u->u.air.state > HRS_ROTOR_MOVING_3) u->u.air.state = HRS_ROTOR_MOVING_1;
		img = GetRotorImage(v);
	} else {
		return;
	}

	u->cur_image = img;

	BeginVehicleMove(u);
	VehiclePositionChanged(u);
	EndVehicleMove(u);
}

static void SetAircraftPosition(Vehicle *v, int x, int y, int z)
{
	Vehicle *u;
	int safe_x;
	int safe_y;

	v->x_pos = x;
	v->y_pos = y;
	v->z_pos = z;

	v->cur_image = GetAircraftImage(v, v->direction);
	if (v->subtype == 0) v->next->next->cur_image = GetRotorImage(v);

	BeginVehicleMove(v);
	VehiclePositionChanged(v);
	EndVehicleMove(v);

	u = v->next;

	safe_x = clamp(x, 0, MapMaxX() * TILE_SIZE);
	safe_y = clamp(y - 1, 0, MapMaxY() * TILE_SIZE);
	u->x_pos = x;
	u->y_pos = y - ((v->z_pos-GetSlopeZ(safe_x, safe_y)) >> 3);;

	safe_y = clamp(u->y_pos, 0, MapMaxY() * TILE_SIZE);
	u->z_pos = GetSlopeZ(safe_x, safe_y);
	u->cur_image = v->cur_image;

	BeginVehicleMove(u);
	VehiclePositionChanged(u);
	EndVehicleMove(u);

	u = u->next;
	if (u != NULL) {
		u->x_pos = x;
		u->y_pos = y;
		u->z_pos = z + 5;

		BeginVehicleMove(u);
		VehiclePositionChanged(u);
		EndVehicleMove(u);
	}
}

/** Handle Aircraft specific tasks when a an Aircraft enters a hangar
 * @param *v Vehicle that enters the hangar
 */
void HandleAircraftEnterHangar(Vehicle *v)
{
	Vehicle *u;

	v->subspeed = 0;
	v->progress = 0;

	u = v->next;
	u->vehstatus |= VS_HIDDEN;
	u = u->next;
	if (u != NULL) {
		u->vehstatus |= VS_HIDDEN;
		u->cur_speed = 0;
	}

	SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos);
}

static void PlayAircraftSound(const Vehicle* v)
{
	if (!PlayVehicleSound(v, VSE_START)) {
		SndPlayVehicleFx(AircraftVehInfo(v->engine_type)->sfx, v);
	}
}

static bool UpdateAircraftSpeed(Vehicle *v)
{
	uint spd = v->acceleration * 2;
	byte t;

	v->subspeed = (t=v->subspeed) + (byte)spd;
	spd = min(v->cur_speed + (spd >> 8) + (v->subspeed < t), v->max_speed);

	// adjust speed for broken vehicles
	if (v->vehstatus & VS_AIRCRAFT_BROKEN) spd = min(spd, 27);

	//updates statusbar only if speed have changed to save CPU time
	if (spd != v->cur_speed) {
		v->cur_speed = spd;
		if (_patches.vehicle_speed)
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	}

	if (!(v->direction & 1)) spd = spd * 3 / 4;

	if (spd == 0) return false;

	if ((byte)++spd == 0) return true;

	v->progress = (t = v->progress) - (byte)spd;

	return t < v->progress;
}

// get Aircraft running altitude
static byte GetAircraftFlyingAltitude(const Vehicle *v)
{
	switch (v->max_speed) {
		case 37: return 162;
		case 74: return 171;
		default: return 180;
	}
}

static bool AircraftController(Vehicle *v)
{
	Station *st;
	const AirportMovingData *amd;
	Vehicle *u;
	byte z,newdir,maxz,curz;
	GetNewVehiclePosResult gp;
	uint dist;
	int x,y;

	st = GetStation(v->u.air.targetairport);

	// prevent going to 0,0 if airport is deleted.
	{
		TileIndex tile = st->airport_tile;

		if (tile == 0) tile = st->xy;
		// xy of destination
		x = TileX(tile) * TILE_SIZE;
		y = TileY(tile) * TILE_SIZE;
	}

	// get airport moving data
	amd = GetAirportMovingData(st->airport_type, v->u.air.pos);

	// Helicopter raise
	if (amd->flag & AMED_HELI_RAISE) {
		u = v->next->next;

		// Make sure the rotors don't rotate too fast
		if (u->cur_speed > 32) {
			v->cur_speed = 0;
			if (--u->cur_speed == 32) SndPlayVehicleFx(SND_18_HELICOPTER, v);
		} else {
			u->cur_speed = 32;
			if (UpdateAircraftSpeed(v)) {
				v->tile = 0;

				// Reached altitude?
				if (v->z_pos >= 184) {
					v->cur_speed = 0;
					return true;
				}
				SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos+1);
			}
		}
		return false;
	}

	// Helicopter landing.
	if (amd->flag & AMED_HELI_LOWER) {
		if (UpdateAircraftSpeed(v)) {
			if (st->airport_tile == 0) {
				// FIXME - AircraftController -> if station no longer exists, do not land
				// helicopter will circle until sign disappears, then go to next order
				// * what to do when it is the only order left, right now it just stays in 1 place
				v->u.air.state = FLYING;
				AircraftNextAirportPos_and_Order(v);
				return false;
			}

			// Vehicle is now at the airport.
			v->tile = st->airport_tile;

			// Find altitude of landing position.
			z = GetSlopeZ(x, y) + 1;
			if (st->airport_type == AT_OILRIG) z += 54;
			if (st->airport_type == AT_HELIPORT) z += 60;

			if (z == v->z_pos) {
				u = v->next->next;

				// Increase speed of rotors. When speed is 80, we've landed.
				if (u->cur_speed >= 80) return true;
				u->cur_speed += 4;
			} else if (v->z_pos > z) {
				SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos-1);
			} else {
				SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos+1);
			}
		}
		return false;
	}

	// Get distance from destination pos to current pos.
	dist = myabs(x + amd->x - v->x_pos) +  myabs(y + amd->y - v->y_pos);

	// Need exact position?
	if (!(amd->flag & AMED_EXACTPOS) && dist <= (amd->flag & AMED_SLOWTURN ? 8U : 4U))
		return true;

	// At final pos?
	if (dist == 0) {
		DirDiff dirdiff;

		if (v->cur_speed > 12) v->cur_speed = 12;

		// Change direction smoothly to final direction.
		dirdiff = DirDifference(amd->direction, v->direction);
		// if distance is 0, and plane points in right direction, no point in calling
		// UpdateAircraftSpeed(). So do it only afterwards
		if (dirdiff == DIRDIFF_SAME) {
			v->cur_speed = 0;
			return true;
		}

		if (!UpdateAircraftSpeed(v)) return false;

		v->direction = ChangeDir(v->direction, dirdiff > DIRDIFF_REVERSE ? DIRDIFF_45LEFT : DIRDIFF_45RIGHT);
		v->cur_speed >>= 1;

		SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos);
		return false;
	}

	if (!(amd->flag & AMED_NOSPDCLAMP) && v->cur_speed > 12) v->cur_speed = 12;

	if (!UpdateAircraftSpeed(v)) return false;

	if (v->load_unload_time_rem != 0) v->load_unload_time_rem--;

	// Turn. Do it slowly if in the air.
	newdir = GetDirectionTowards(v, x + amd->x, y + amd->y);
	if (newdir != v->direction) {
		if (amd->flag & AMED_SLOWTURN) {
			if (v->load_unload_time_rem == 0) v->load_unload_time_rem = 8;
			v->direction = newdir;
		} else {
			v->cur_speed >>= 1;
			v->direction = newdir;
		}
	}

	// Move vehicle.
	GetNewVehiclePos(v, &gp);
	v->tile = gp.new_tile;

	// If vehicle is in the air, use tile coordinate 0.
	if (amd->flag & (AMED_TAKEOFF | AMED_SLOWTURN | AMED_LAND)) v->tile = 0;

	// Adjust Z for land or takeoff?
	z = v->z_pos;

	if (amd->flag & AMED_TAKEOFF) {
		z += 2;
		maxz = GetAircraftFlyingAltitude(v);
		if (z > maxz) z = maxz;
	}

	if (amd->flag & AMED_LAND) {
		if (st->airport_tile == 0) {
			v->u.air.state = FLYING;
			AircraftNextAirportPos_and_Order(v);
			// get aircraft back on running altitude
			SetAircraftPosition(v, gp.x, gp.y, GetAircraftFlyingAltitude(v));
			return false;
		}

		curz = GetSlopeZ(x, y) + 1;

		if (curz > z) {
			z++;
		} else {
			int t = max(1, dist - 4);

			z -= ((z - curz) + t - 1) / t;
			if (z < curz) z = curz;
		}
	}

	// We've landed. Decrase speed when we're reaching end of runway.
	if (amd->flag & AMED_BRAKE) {
		curz = GetSlopeZ(x, y) + 1;

		if (z > curz) {
			z--;
		} else if (z < curz) {
			z++;
		}

		if (dist < 64 && v->cur_speed > 12) v->cur_speed -= 4;
	}

	SetAircraftPosition(v, gp.x, gp.y, z);
	return false;
}


static void HandleCrashedAircraft(Vehicle *v)
{
	uint32 r;
	Station *st;
	int z;

	v->u.air.crashed_counter++;

	st = GetStation(v->u.air.targetairport);

	// make aircraft crash down to the ground
	if (v->u.air.crashed_counter < 500 && st->airport_tile==0 && ((v->u.air.crashed_counter % 3) == 0) ) {
		z = GetSlopeZ(v->x_pos, v->y_pos);
		v->z_pos -= 1;
		if (v->z_pos == z) {
			v->u.air.crashed_counter = 500;
			v->z_pos++;
		}
	}

	if (v->u.air.crashed_counter < 650) {
		if (CHANCE16R(1,32,r)) {
			static const DirDiff delta[] = {
				DIRDIFF_45LEFT, DIRDIFF_SAME, DIRDIFF_SAME, DIRDIFF_45RIGHT
			};

			v->direction = ChangeDir(v->direction, delta[GB(r, 16, 2)]);
			SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos);
			r = Random();
			CreateEffectVehicleRel(v,
				GB(r, 0, 4) + 4,
				GB(r, 4, 4) + 4,
				GB(r, 8, 4),
				EV_EXPLOSION_SMALL);
		}
	} else if (v->u.air.crashed_counter >= 10000) {
		// remove rubble of crashed airplane

		// clear runway-in on all airports, set by crashing plane
		// small airports use AIRPORT_BUSY, city airports use RUNWAY_IN_OUT_block, etc.
		// but they all share the same number
		CLRBITS(st->airport_flags, RUNWAY_IN_block);
		CLRBITS(st->airport_flags, RUNWAY_IN_OUT_block); // commuter airport
		CLRBITS(st->airport_flags, RUNWAY_IN2_block); // intercontinental

		BeginVehicleMove(v);
		EndVehicleMove(v);

		DoDeleteAircraft(v);
	}
}

static void HandleBrokenAircraft(Vehicle *v)
{
	if (v->breakdown_ctr != 1) {
		v->breakdown_ctr = 1;
		v->vehstatus |= VS_AIRCRAFT_BROKEN;

		if (v->breakdowns_since_last_service != 255)
			v->breakdowns_since_last_service++;
		InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	}
}


static void HandleAircraftSmoke(Vehicle *v)
{
	static const struct {
		int8 x;
		int8 y;
	} smoke_pos[] = {
		{  5,  5 },
		{  6,  0 },
		{  5, -5 },
		{  0, -6 },
		{ -5, -5 },
		{ -6,  0 },
		{ -5,  5 },
		{  0,  6 }
	};

	if (!(v->vehstatus & VS_AIRCRAFT_BROKEN)) return;

	if (v->cur_speed < 10) {
		v->vehstatus &= ~VS_AIRCRAFT_BROKEN;
		v->breakdown_ctr = 0;
		return;
	}

	if ((v->tick_counter & 0x1F) == 0) {
		CreateEffectVehicleRel(v,
			smoke_pos[v->direction].x,
			smoke_pos[v->direction].y,
			2,
			EV_SMOKE
		);
	}
}

static void ProcessAircraftOrder(Vehicle *v)
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

		case OT_LOADING: return;

		default: break;
	}

	if (v->cur_order_index >= v->num_orders) v->cur_order_index = 0;

	order = GetVehicleOrder(v, v->cur_order_index);

	if (order == NULL) {
		v->current_order.type = OT_NOTHING;
		v->current_order.flags = 0;
		return;
	}

	if (order->type == OT_DUMMY && !CheckForValidOrders(v)) CrashAirplane(v);

	if (order->type  == v->current_order.type  &&
			order->flags == v->current_order.flags &&
			order->dest  == v->current_order.dest)
		return;

	v->current_order = *order;

	// orders are changed in flight, ensure going to the right station
	if (order->type == OT_GOTO_STATION && v->u.air.state == FLYING) {
		AircraftNextAirportPos_and_Order(v);
		v->u.air.targetairport = order->dest;
	}

	InvalidateVehicleOrder(v);

	InvalidateWindowClasses(WC_AIRCRAFT_LIST);
}

/** Mark all views dirty for an aircraft.
 * @param v vehicle to be redrawn.
 */
static void MarkAircraftDirty(Vehicle *v)
{
		v->cur_image = GetAircraftImage(v, v->direction);
		if (v->subtype == 0) {
			v->next->next->cur_image = GetRotorImage(v);
		}
		MarkAllViewportsDirty(v->left_coord, v->top_coord, v->right_coord + 1, v->bottom_coord + 1);
}

static void HandleAircraftLoading(Vehicle *v, int mode)
{
	if (v->current_order.type == OT_NOTHING) return;

	if (v->current_order.type != OT_DUMMY) {
		if (v->current_order.type != OT_LOADING) return;
		if (mode != 0) return;
		if (--v->load_unload_time_rem != 0) return;

		if (CanFillVehicle(v) && (v->current_order.flags & OF_FULL_LOAD ||
				(_patches.gradual_loading && !HASBIT(v->load_status, LS_LOADING_FINISHED)))) {
			SET_EXPENSES_TYPE(EXPENSES_AIRCRAFT_INC);
			if (LoadUnloadVehicle(v, false)) {
				InvalidateWindow(WC_AIRCRAFT_LIST, v->owner);
				MarkAircraftDirty(v);
			}
			return;
		}

		{
			Order b = v->current_order;
			v->current_order.type = OT_NOTHING;
			v->current_order.flags = 0;
			MarkAircraftDirty(v);
			if (!(b.flags & OF_NON_STOP)) return;
		}
	}
	v->cur_order_index++;
	InvalidateVehicleOrder(v);
}

static void CrashAirplane(Vehicle *v)
{
	uint16 amt;
	Station *st;
	StringID newsitem;

	v->vehstatus |= VS_CRASHED;
	v->u.air.crashed_counter = 0;

	CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);

	InvalidateWindow(WC_VEHICLE_VIEW, v->index);

	amt = 2;
	if (v->cargo_type == CT_PASSENGERS) amt += v->cargo_count;
	SetDParam(0, amt);

	v->cargo_count = 0;
	v->next->cargo_count = 0,
	st = GetStation(v->u.air.targetairport);
	if (st->airport_tile == 0) {
		newsitem = STR_PLANE_CRASH_OUT_OF_FUEL;
	} else {
		SetDParam(1, st->index);
		newsitem = STR_A034_PLANE_CRASH_DIE_IN_FIREBALL;
	}

	SetDParam(1, st->index);
	AddNewsItem(newsitem,
		NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ACCIDENT, 0),
		v->index,
		0);

	SndPlayVehicleFx(SND_12_EXPLOSION, v);
}

static void MaybeCrashAirplane(Vehicle *v)
{
	Station *st;
	uint16 prob;
	uint i;

	st = GetStation(v->u.air.targetairport);

	//FIXME -- MaybeCrashAirplane -> increase crashing chances of very modern airplanes on smaller than AT_METROPOLITAN airports
	prob = 0x10000 / 1500;
	if (((st->airport_type == AT_SMALL) || (st->airport_type == AT_COMMUTER)) && (AircraftVehInfo(v->engine_type)->subtype & AIR_FAST) && !_cheats.no_jetcrash.value) {
		prob = 0x10000 / 20;
	}

	if (GB(Random(), 0, 16) > prob) return;

	// Crash the airplane. Remove all goods stored at the station.
	for (i = 0; i != NUM_CARGO; i++) {
		st->goods[i].rating = 1;
		SB(st->goods[i].waiting_acceptance, 0, 12, 0);
	}

	CrashAirplane(v);
}

// we've landed and just arrived at a terminal
static void AircraftEntersTerminal(Vehicle *v)
{
	Station *st;
	Order old_order;

	if (v->current_order.type == OT_GOTO_DEPOT) return;

	st = GetStation(v->u.air.targetairport);
	v->last_station_visited = v->u.air.targetairport;

	/* Check if station was ever visited before */
	if (!(st->had_vehicle_of_type & HVOT_AIRCRAFT)) {
		uint32 flags;

		st->had_vehicle_of_type |= HVOT_AIRCRAFT;
		SetDParam(0, st->index);
		// show newsitem of celebrating citizens
		flags = (v->owner == _local_player) ? NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_PLAYER, 0) : NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_OTHER, 0);
		AddNewsItem(
			STR_A033_CITIZENS_CELEBRATE_FIRST,
			flags,
			v->index,
			0);
	}

	old_order = v->current_order;
	v->current_order.type = OT_LOADING;
	v->current_order.flags = 0;

	if (old_order.type == OT_GOTO_STATION &&
			v->current_order.dest == v->last_station_visited) {
		v->current_order.flags =
			(old_order.flags & (OF_FULL_LOAD | OF_UNLOAD | OF_TRANSFER)) | OF_NON_STOP;
	}

	SET_EXPENSES_TYPE(EXPENSES_AIRCRAFT_INC);
	LoadUnloadVehicle(v, true);
	MarkAircraftDirty(v);
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	InvalidateWindowClasses(WC_AIRCRAFT_LIST);
}

static void AircraftLand(Vehicle *v)
{
	v->sprite_width = v->sprite_height = 2;
}

static void AircraftLandAirplane(Vehicle *v)
{
	AircraftLand(v);
	if (!PlayVehicleSound(v, VSE_TOUCHDOWN)) {
		SndPlayVehicleFx(SND_17_SKID_PLANE, v);
	}
	MaybeCrashAirplane(v);
}

// set the right pos when heading to other airports after takeoff
static void AircraftNextAirportPos_and_Order(Vehicle *v)
{
	const Station* st;
	const AirportFTAClass *apc;

	if (v->current_order.type == OT_GOTO_STATION ||
			v->current_order.type == OT_GOTO_DEPOT)
		v->u.air.targetairport = v->current_order.dest;

	st = GetStation(v->u.air.targetairport);
	apc = GetAirport(st->airport_type);
	v->u.air.pos = v->u.air.previous_pos = apc->entry_point;
}

static void AircraftLeaveHangar(Vehicle *v)
{
	v->cur_speed = 0;
	v->subspeed = 0;
	v->progress = 0;
	v->direction = 3;
	v->vehstatus &= ~VS_HIDDEN;
	{
		Vehicle *u = v->next;
		u->vehstatus &= ~VS_HIDDEN;

		// Rotor blades
		u = u->next;
		if (u != NULL) {
			u->vehstatus &= ~VS_HIDDEN;
			u->cur_speed = 80;
		}
	}

	VehicleServiceInDepot(v);
	SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos);
	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	InvalidateWindowClasses(WC_AIRCRAFT_LIST);
}


////////////////////////////////////////////////////////////////////////////////
///////////////////   AIRCRAFT MOVEMENT SCHEME  ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static void AircraftEventHandler_EnterTerminal(Vehicle *v, const AirportFTAClass *apc)
{
	AircraftEntersTerminal(v);
	v->u.air.state = apc->layout[v->u.air.pos].heading;
}

static void AircraftEventHandler_EnterHangar(Vehicle *v, const AirportFTAClass *apc)
{
	VehicleEnterDepot(v);
	v->u.air.state = apc->layout[v->u.air.pos].heading;
}

// In an Airport Hangar
static void AircraftEventHandler_InHangar(Vehicle *v, const AirportFTAClass *apc)
{
	// if we just arrived, execute EnterHangar first
	if (v->u.air.previous_pos != v->u.air.pos) {
		AircraftEventHandler_EnterHangar(v, apc);
		return;
	}

	// if we were sent to the depot, stay there
	if (v->current_order.type == OT_GOTO_DEPOT && (v->vehstatus & VS_STOPPED)) {
		v->current_order.type = OT_NOTHING;
		v->current_order.flags = 0;
		return;
	}

	if (v->current_order.type != OT_GOTO_STATION &&
			v->current_order.type != OT_GOTO_DEPOT)
		return;

	// if the block of the next position is busy, stay put
	if (AirportHasBlock(v, &apc->layout[v->u.air.pos], apc)) return;

	// We are already at the target airport, we need to find a terminal
	if (v->current_order.dest == v->u.air.targetairport) {
		// FindFreeTerminal:
		// 1. Find a free terminal, 2. Occupy it, 3. Set the vehicle's state to that terminal
		if (v->subtype != 0) {
			if (!AirportFindFreeTerminal(v, apc)) return; // airplane
		} else {
			if (!AirportFindFreeHelipad(v, apc)) return; // helicopter
		}
	} else { // Else prepare for launch.
		// airplane goto state takeoff, helicopter to helitakeoff
		v->u.air.state = (v->subtype != 0) ? TAKEOFF : HELITAKEOFF;
	}
	AircraftLeaveHangar(v);
	AirportMove(v, apc);
}

// At one of the Airport's Terminals
static void AircraftEventHandler_AtTerminal(Vehicle *v, const AirportFTAClass *apc)
{
	// if we just arrived, execute EnterTerminal first
	if (v->u.air.previous_pos != v->u.air.pos) {
		AircraftEventHandler_EnterTerminal(v, apc);
		// on an airport with helipads, a helicopter will always land there
		// and get serviced at the same time - patch setting
		if (_patches.serviceathelipad) {
			if (v->subtype == 0 && apc->helipads != NULL) {
				// an exerpt of ServiceAircraft, without the invisibility stuff
				v->date_of_last_service = _date;
				v->breakdowns_since_last_service = 0;
				v->reliability = GetEngine(v->engine_type)->reliability;
				InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
			}
		}
		return;
	}

	if (v->current_order.type == OT_NOTHING) return;

	// if the block of the next position is busy, stay put
	if (AirportHasBlock(v, &apc->layout[v->u.air.pos], apc)) return;

	// airport-road is free. We either have to go to another airport, or to the hangar
	// ---> start moving

	switch (v->current_order.type) {
		case OT_GOTO_STATION: // ready to fly to another airport
			// airplane goto state takeoff, helicopter to helitakeoff
			v->u.air.state = (v->subtype != 0) ? TAKEOFF : HELITAKEOFF;
			break;
		case OT_GOTO_DEPOT:   // visit hangar for serivicing, sale, etc.
			if (v->current_order.dest == v->u.air.targetairport) {
				v->u.air.state = HANGAR;
			} else {
				v->u.air.state = (v->subtype != 0) ? TAKEOFF : HELITAKEOFF;
			}
			break;
		default:  // orders have been deleted (no orders), goto depot and don't bother us
			v->current_order.type = OT_NOTHING;
			v->current_order.flags = 0;
			v->u.air.state = HANGAR;
	}
	AirportMove(v, apc);
}

static void AircraftEventHandler_General(Vehicle *v, const AirportFTAClass *apc)
{
	assert("OK, you shouldn't be here, check your Airport Scheme!" && 0);
}

static void AircraftEventHandler_TakeOff(Vehicle *v, const AirportFTAClass *apc) {
	PlayAircraftSound(v); // play takeoffsound for airplanes
	v->u.air.state = STARTTAKEOFF;
}

static void AircraftEventHandler_StartTakeOff(Vehicle *v, const AirportFTAClass *apc)
{
	v->sprite_width = v->sprite_height = 24; // ??? no idea what this is
	v->u.air.state = ENDTAKEOFF;
}

static void AircraftEventHandler_EndTakeOff(Vehicle *v, const AirportFTAClass *apc)
{
	v->u.air.state = FLYING;
	// get the next position to go to, differs per airport
	AircraftNextAirportPos_and_Order(v);
}

static void AircraftEventHandler_HeliTakeOff(Vehicle *v, const AirportFTAClass *apc)
{
	const Player* p = GetPlayer(v->owner);
	v->sprite_width = v->sprite_height = 24; // ??? no idea what this is
	v->u.air.state = FLYING;
	// get the next position to go to, differs per airport
	AircraftNextAirportPos_and_Order(v);

	// check if the aircraft needs to be replaced or renewed and send it to a hangar if needed
	// unless it is due for renewal but the engine is no longer available
	if (v->owner == _local_player && (
				EngineHasReplacementForPlayer(p, v->engine_type) ||
				((p->engine_renew && v->age - v->max_age > p->engine_renew_months * 30) &&
				HASBIT(GetEngine(v->engine_type)->player_avail, _local_player))
			)) {
		_current_player = _local_player;
		DoCommandP(v->tile, v->index, DEPOT_SERVICE | DEPOT_LOCATE_HANGAR, NULL, CMD_SEND_AIRCRAFT_TO_HANGAR | CMD_SHOW_NO_ERROR);
		_current_player = OWNER_NONE;
	}
}

static void AircraftEventHandler_Flying(Vehicle *v, const AirportFTAClass *apc)
{
	Station *st;
	byte landingtype;
	AirportFTA *current;
	uint16 tcur_speed, tsubspeed;

	st = GetStation(v->u.air.targetairport);
	// flying device is accepted at this station
	// small airport --> no helicopters (AIRCRAFT_ONLY)
	// all other airports --> all types of flying devices (ALL)
	// heliport/oilrig, etc --> no airplanes (HELICOPTERS_ONLY)
	// runway busy or not allowed to use this airstation, circle
	if (v->subtype != apc->acc_planes &&
			st->airport_tile != 0 &&
			(st->owner == OWNER_NONE || st->owner == v->owner)) {
		// {32,FLYING,NOTHING_block,37}, {32,LANDING,N,33}, {32,HELILANDING,N,41},
		// if it is an airplane, look for LANDING, for helicopter HELILANDING
		// it is possible to choose from multiple landing runways, so loop until a free one is found
		landingtype = (v->subtype != 0) ? LANDING : HELILANDING;
		current = apc->layout[v->u.air.pos].next;
		while (current != NULL) {
			if (current->heading == landingtype) {
				// save speed before, since if AirportHasBlock is false, it resets them to 0
				// we don't want that for plane in air
				// hack for speed thingie
				tcur_speed = v->cur_speed;
				tsubspeed = v->subspeed;
				if (!AirportHasBlock(v, current, apc)) {
					v->u.air.state = landingtype; // LANDING / HELILANDING
					// it's a bit dirty, but I need to set position to next position, otherwise
					// if there are multiple runways, plane won't know which one it took (because
					// they all have heading LANDING). And also occupy that block!
					v->u.air.pos = current->next_position;
					SETBITS(st->airport_flags, apc->layout[v->u.air.pos].block);
					return;
				}
				v->cur_speed = tcur_speed;
				v->subspeed = tsubspeed;
			}
			current = current->next;
		}
	}
	v->u.air.state = FLYING;
	v->u.air.pos = apc->layout[v->u.air.pos].next_position;
}

static void AircraftEventHandler_Landing(Vehicle *v, const AirportFTAClass *apc)
{
	const Player* p = GetPlayer(v->owner);
	AircraftLandAirplane(v);  // maybe crash airplane
	v->u.air.state = ENDLANDING;
	// check if the aircraft needs to be replaced or renewed and send it to a hangar if needed
	if (v->current_order.type != OT_GOTO_DEPOT && v->owner == _local_player) {
		// only the vehicle owner needs to calculate the rest (locally)
		if (EngineHasReplacementForPlayer(p, v->engine_type) ||
			(p->engine_renew && v->age - v->max_age > (p->engine_renew_months * 30))) {
			// send the aircraft to the hangar at next airport
			_current_player = _local_player;
			DoCommandP(v->tile, v->index, DEPOT_SERVICE, NULL, CMD_SEND_AIRCRAFT_TO_HANGAR | CMD_SHOW_NO_ERROR);
			_current_player = OWNER_NONE;
		}
	}
}

static void AircraftEventHandler_HeliLanding(Vehicle *v, const AirportFTAClass *apc)
{
	AircraftLand(v); // helicopters don't crash
	v->u.air.state = HELIENDLANDING;
}

static void AircraftEventHandler_EndLanding(Vehicle *v, const AirportFTAClass *apc)
{
	// next block busy, don't do a thing, just wait
	if (AirportHasBlock(v, &apc->layout[v->u.air.pos], apc)) return;

	// if going to terminal (OT_GOTO_STATION) choose one
	// 1. in case all terminals are busy AirportFindFreeTerminal() returns false or
	// 2. not going for terminal (but depot, no order),
	// --> get out of the way to the hangar.
	if (v->current_order.type == OT_GOTO_STATION) {
		if (AirportFindFreeTerminal(v, apc)) return;
	}
	v->u.air.state = HANGAR;

}

static void AircraftEventHandler_HeliEndLanding(Vehicle *v, const AirportFTAClass *apc)
{
	// next block busy, don't do a thing, just wait
	if (AirportHasBlock(v, &apc->layout[v->u.air.pos], apc)) return;

	// if going to helipad (OT_GOTO_STATION) choose one. If airport doesn't have helipads, choose terminal
	// 1. in case all terminals/helipads are busy (AirportFindFreeHelipad() returns false) or
	// 2. not going for terminal (but depot, no order),
	// --> get out of the way to the hangar IF there are terminals on the airport.
	// --> else TAKEOFF
	// the reason behind this is that if an airport has a terminal, it also has a hangar. Airplanes
	// must go to a hangar.
	if (v->current_order.type == OT_GOTO_STATION) {
		if (AirportFindFreeHelipad(v, apc)) return;
	}
	v->u.air.state = (apc->nof_depots != 0) ? HANGAR : HELITAKEOFF;
}

typedef void AircraftStateHandler(Vehicle *v, const AirportFTAClass *apc);
static AircraftStateHandler * const _aircraft_state_handlers[] = {
	AircraftEventHandler_General,        // TO_ALL         =  0
	AircraftEventHandler_InHangar,       // HANGAR         =  1
	AircraftEventHandler_AtTerminal,     // TERM1          =  2
	AircraftEventHandler_AtTerminal,     // TERM2          =  3
	AircraftEventHandler_AtTerminal,     // TERM3          =  4
	AircraftEventHandler_AtTerminal,     // TERM4          =  5
	AircraftEventHandler_AtTerminal,     // TERM5          =  6
	AircraftEventHandler_AtTerminal,     // TERM6          =  7
	AircraftEventHandler_AtTerminal,     // HELIPAD1       =  8
	AircraftEventHandler_AtTerminal,     // HELIPAD2       =  9
	AircraftEventHandler_TakeOff,        // TAKEOFF        = 10
	AircraftEventHandler_StartTakeOff,   // STARTTAKEOFF   = 11
	AircraftEventHandler_EndTakeOff,     // ENDTAKEOFF     = 12
	AircraftEventHandler_HeliTakeOff,    // HELITAKEOFF    = 13
	AircraftEventHandler_Flying,         // FLYING         = 14
	AircraftEventHandler_Landing,        // LANDING        = 15
	AircraftEventHandler_EndLanding,     // ENDLANDING     = 16
	AircraftEventHandler_HeliLanding,    // HELILANDING    = 17
	AircraftEventHandler_HeliEndLanding, // HELIENDLANDING = 18
	AircraftEventHandler_AtTerminal,     // TERM7          = 19
	AircraftEventHandler_AtTerminal,     // TERM8          = 20
	AircraftEventHandler_AtTerminal,     // HELIPAD3       = 21
	AircraftEventHandler_AtTerminal,     // HELIPAD4       = 22
};

static void AirportClearBlock(const Vehicle *v, const AirportFTAClass *apc)
{
	// we have left the previous block, and entered the new one. Free the previous block
	if (apc->layout[v->u.air.previous_pos].block != apc->layout[v->u.air.pos].block) {
		Station *st = GetStation(v->u.air.targetairport);

		CLRBITS(st->airport_flags, apc->layout[v->u.air.previous_pos].block);
	}
}

static void AirportGoToNextPosition(Vehicle *v, const AirportFTAClass *apc)
{
	// if aircraft is not in position, wait until it is
	if (!AircraftController(v)) return;

	AirportClearBlock(v, apc);
	AirportMove(v, apc); // move aircraft to next position
}

// gets pos from vehicle and next orders
static bool AirportMove(Vehicle *v, const AirportFTAClass *apc)
{
	AirportFTA *current;
	byte prev_pos;

	// error handling
	if (v->u.air.pos >= apc->nofelements) {
		DEBUG(misc, 0, "[Ap] position %d is not valid for current airport. Max position is %d", v->u.air.pos, apc->nofelements-1);
		assert(v->u.air.pos < apc->nofelements);
	}

	current = &apc->layout[v->u.air.pos];
	// we have arrived in an important state (eg terminal, hangar, etc.)
	if (current->heading == v->u.air.state) {
		prev_pos = v->u.air.pos; // location could be changed in state, so save it before-hand
		_aircraft_state_handlers[v->u.air.state](v, apc);
		if (v->u.air.state != FLYING) v->u.air.previous_pos = prev_pos;
		return true;
	}

	v->u.air.previous_pos = v->u.air.pos; // save previous location

	// there is only one choice to move to
	if (current->next == NULL) {
		if (AirportSetBlocks(v, current, apc)) {
			v->u.air.pos = current->next_position;
		} // move to next position
		return false;
	}

	// there are more choices to choose from, choose the one that
	// matches our heading
	do {
		if (v->u.air.state == current->heading || current->heading == TO_ALL) {
			if (AirportSetBlocks(v, current, apc)) {
				v->u.air.pos = current->next_position;
			} // move to next position
			return false;
		}
		current = current->next;
	} while (current != NULL);

	DEBUG(misc, 0, "[Ap] cannot move further on Airport! (pos %d state %d)", v->u.air.pos, v->u.air.state);
	DEBUG(misc, 0, "[Ap] airport entry point: %d, Vehicle: %d", apc->entry_point, v->index);
	assert(0);
	return false;
}

// returns true if the road ahead is busy, eg. you must wait before proceeding
static bool AirportHasBlock(Vehicle *v, const AirportFTA *current_pos, const AirportFTAClass *apc)
{
	const AirportFTA *reference = &apc->layout[v->u.air.pos];
	const AirportFTA *next = &apc->layout[current_pos->next_position];

	// same block, then of course we can move
	if (apc->layout[current_pos->position].block != next->block) {
		const Station *st = GetStation(v->u.air.targetairport);
		uint32 airport_flags = next->block;

		// check additional possible extra blocks
		if (current_pos != reference && current_pos->block != NOTHING_block) {
			airport_flags |= current_pos->block;
		}

		if (HASBITS(st->airport_flags, airport_flags)) {
			v->cur_speed = 0;
			v->subspeed = 0;
			return true;
		}
	}
	return false;
}

// returns true on success. Eg, next block was free and we have occupied it
static bool AirportSetBlocks(Vehicle *v, AirportFTA *current_pos, const AirportFTAClass *apc)
{
	AirportFTA *next = &apc->layout[current_pos->next_position];
	AirportFTA *reference = &apc->layout[v->u.air.pos];

	// if the next position is in another block, check it and wait until it is free
	if ((apc->layout[current_pos->position].block & next->block) != next->block) {
		uint32 airport_flags = next->block;
		Station* st = GetStation(v->u.air.targetairport);
		//search for all all elements in the list with the same state, and blocks != N
		// this means more blocks should be checked/set
			AirportFTA *current = current_pos;
		if (current == reference) current = current->next;
		while (current != NULL) {
			if (current->heading == current_pos->heading && current->block != 0) {
				airport_flags |= current->block;
				break;
			}
			current = current->next;
		};

		// if the block to be checked is in the next position, then exclude that from
		// checking, because it has been set by the airplane before
		if (current_pos->block == next->block) airport_flags ^= next->block;

		if (HASBITS(st->airport_flags, airport_flags)) {
			v->cur_speed = 0;
			v->subspeed = 0;
			return false;
		}

		if (next->block != NOTHING_block) {
			SETBITS(st->airport_flags, airport_flags); // occupy next block
		}
	}
	return true;
}

static bool FreeTerminal(Vehicle *v, byte i, byte last_terminal)
{
	Station *st = GetStation(v->u.air.targetairport);
	for (; i < last_terminal; i++) {
		if (!HASBIT(st->airport_flags, _airport_terminal_flag[i])) {
			// TERMINAL# HELIPAD#
			v->u.air.state = _airport_terminal_state[i]; // start moving to that terminal/helipad
			SETBIT(st->airport_flags, _airport_terminal_flag[i]); // occupy terminal/helipad
			return true;
		}
	}
	return false;
}

static uint GetNumTerminals(const AirportFTAClass *apc)
{
	uint num = 0;
	uint i;

	for (i = apc->terminals[0]; i > 0; i--) num += apc->terminals[i];

	return num;
}

static bool AirportFindFreeTerminal(Vehicle *v, const AirportFTAClass *apc)
{
	/* example of more terminalgroups
	 * {0,HANGAR,NOTHING_block,1}, {0,255,TERM_GROUP1_block,0}, {0,255,TERM_GROUP2_ENTER_block,1}, {0,0,N,1},
	 * Heading 255 denotes a group. We see 2 groups here:
	 * 1. group 0 -- TERM_GROUP1_block (check block)
	 * 2. group 1 -- TERM_GROUP2_ENTER_block (check block)
	 * First in line is checked first, group 0. If the block (TERM_GROUP1_block) is free, it
	 * looks at the corresponding terminals of that group. If no free ones are found, other
	 * possible groups are checked (in this case group 1, since that is after group 0). If that
	 * fails, then attempt fails and plane waits
	 */
	if (apc->terminals[0] > 1) {
		Station *st = GetStation(v->u.air.targetairport);
		AirportFTA *temp = apc->layout[v->u.air.pos].next;

		while (temp != NULL) {
			if (temp->heading == 255) {
				if (!HASBITS(st->airport_flags, temp->block)) {
					int target_group;
					int i;
					int group_start = 0;
					int group_end;

					//read which group do we want to go to?
					//(the first free group)
					target_group = temp->next_position + 1;

					//at what terminal does the group start?
					//that means, sum up all terminals of
					//groups with lower number
					for (i = 1; i < target_group; i++)
						group_start += apc->terminals[i];

					group_end = group_start + apc->terminals[target_group];
					if (FreeTerminal(v, group_start, group_end)) return true;
				}
			} else {
				/* once the heading isn't 255, we've exhausted the possible blocks.
				 * So we cannot move */
				return false;
			}
			temp = temp->next;
		}
	}

	// if there is only 1 terminalgroup, all terminals are checked (starting from 0 to max)
	return FreeTerminal(v, 0, GetNumTerminals(apc));
}

static uint GetNumHelipads(const AirportFTAClass *apc)
{
	uint num = 0;
	uint i;

	for (i = apc->helipads[0]; i > 0; i--) num += apc->helipads[i];

	return num;
}


static bool AirportFindFreeHelipad(Vehicle *v, const AirportFTAClass *apc)
{
	// if an airport doesn't have helipads, use terminals
	if (apc->helipads == NULL) return AirportFindFreeTerminal(v, apc);

	// if there are more helicoptergroups, pick one, just as in AirportFindFreeTerminal()
	if (apc->helipads[0] > 1) {
		const Station* st = GetStation(v->u.air.targetairport);
		const AirportFTA* temp = apc->layout[v->u.air.pos].next;

		while (temp != NULL) {
			if (temp->heading == 255) {
				if (!HASBITS(st->airport_flags, temp->block)) {
					int target_group;
					int i;
					int group_start = 0;
					int group_end;

					//read which group do we want to go to?
					//(the first free group)
					target_group = temp->next_position + 1;

					//at what terminal does the group start?
					//that means, sum up all terminals of
					//groups with lower number
					for (i = 1; i < target_group; i++)
						group_start += apc->helipads[i];

					group_end = group_start + apc->helipads[target_group];
					if (FreeTerminal(v, group_start, group_end)) return true;
				}
			} else {
				/* once the heading isn't 255, we've exhausted the possible blocks.
				 * So we cannot move */
				return false;
			}
			temp = temp->next;
		}
	} else {
		// only 1 helicoptergroup, check all helipads
		// The blocks for helipads start after the last terminal (MAX_TERMINALS)
		return FreeTerminal(v, MAX_TERMINALS, GetNumHelipads(apc) + MAX_TERMINALS);
	}
	return false; // it shouldn't get here anytime, but just to be sure
}

static void AircraftEventHandler(Vehicle *v, int loop)
{
	v->tick_counter++;

	if (v->vehstatus & VS_CRASHED) {
		HandleCrashedAircraft(v);
		return;
	}

	if (v->vehstatus & VS_STOPPED) return;

	/* aircraft is broken down? */
	if (v->breakdown_ctr != 0) {
		if (v->breakdown_ctr <= 2) {
			HandleBrokenAircraft(v);
		} else {
			v->breakdown_ctr--;
		}
	}

	HandleAircraftSmoke(v);
	ProcessAircraftOrder(v);
	HandleAircraftLoading(v, loop);

	if (v->current_order.type >= OT_LOADING) return;

	// pass the right airport structure to the functions
	// DEREF_STATION gets target airport (Station *st), its type is passed to GetAirport
	// that returns the correct layout depending on type
	AirportGoToNextPosition(v, GetAirport(GetStation(v->u.air.targetairport)->airport_type));
}

void Aircraft_Tick(Vehicle *v)
{
	int i;

	if (v->subtype > 2) return;

	if (v->subtype == 0) HelicopterTickHandler(v);

	AgeAircraftCargo(v);

	for (i = 0; i != 6; i++) {
		AircraftEventHandler(v, i);
		if (v->type != VEH_Aircraft) // In case it was deleted
			break;
	}
}

void UpdateOilRig(void)
{
	Station* st;

	FOR_ALL_STATIONS(st) {
		if (st->airport_type == 5) st->airport_type = AT_OILRIG;
	}
}

// need to be called to load aircraft from old version
void UpdateOldAircraft(void)
{
	Station *st;
	Vehicle *v_oldstyle;
	GetNewVehiclePosResult gp;

	// set airport_flags to 0 for all airports just to be sure
	FOR_ALL_STATIONS(st) {
		st->airport_flags = 0; // reset airport
		// type of oilrig has been moved, update it (3-5)
		if (st->airport_type == 3) st->airport_type = AT_OILRIG;
	}

	FOR_ALL_VEHICLES(v_oldstyle) {
	// airplane has another vehicle with subtype 4 (shadow), helicopter also has 3 (rotor)
	// skip those
		if (v_oldstyle->type == VEH_Aircraft && v_oldstyle->subtype <= 2) {
			// airplane in terminal stopped doesn't hurt anyone, so goto next
			if (v_oldstyle->vehstatus & VS_STOPPED && v_oldstyle->u.air.state == 0) {
				v_oldstyle->u.air.state = HANGAR;
				continue;
			}

			AircraftLeaveHangar(v_oldstyle); // make airplane visible if it was in a depot for example
			v_oldstyle->vehstatus &= ~VS_STOPPED; // make airplane moving
			v_oldstyle->u.air.state = FLYING;
			AircraftNextAirportPos_and_Order(v_oldstyle); // move it to the entry point of the airport
			GetNewVehiclePos(v_oldstyle, &gp); // get the position of the plane (to be used for setting)
			v_oldstyle->tile = 0; // aircraft in air is tile=0

			// correct speed of helicopter-rotors
			if (v_oldstyle->subtype == 0) v_oldstyle->next->next->cur_speed = 32;

			// set new position x,y,z
			SetAircraftPosition(v_oldstyle, gp.x, gp.y, GetAircraftFlyingAltitude(v_oldstyle));
		}
	}
}

void UpdateAirplanesOnNewStation(Station *st)
{
	GetNewVehiclePosResult gp;
	Vehicle *v;
	byte takeofftype;
	uint16 cnt;
	// only 1 station is updated per function call, so it is enough to get entry_point once
	const AirportFTAClass *ap = GetAirport(st->airport_type);
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Aircraft && v->subtype <= 2) {
			if (v->u.air.targetairport == st->index) { // if heading to this airport
				/* update position of airplane. If plane is not flying, landing, or taking off
				 *you cannot delete airport, so it doesn't matter
				 */
				if (v->u.air.state >= FLYING) { // circle around
					v->u.air.pos = v->u.air.previous_pos = ap->entry_point;
					v->u.air.state = FLYING;
					// landing plane needs to be reset to flying height (only if in pause mode upgrade,
					// in normal mode, plane is reset in AircraftController. It doesn't hurt for FLYING
					GetNewVehiclePos(v, &gp);
					// set new position x,y,z
					SetAircraftPosition(v, gp.x, gp.y, GetAircraftFlyingAltitude(v));
				} else {
					assert(v->u.air.state == ENDTAKEOFF || v->u.air.state == HELITAKEOFF);
					takeofftype = (v->subtype == 0) ? HELITAKEOFF : ENDTAKEOFF;
					// search in airportdata for that heading
					// easiest to do, since this doesn't happen a lot
					for (cnt = 0; cnt < ap->nofelements; cnt++) {
						if (ap->layout[cnt].heading == takeofftype) {
							v->u.air.pos = ap->layout[cnt].position;
							break;
						}
					}
				}
			}
		}
	}
}
