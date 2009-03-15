/* $Id$ */

/** @file vehicle_cmd.cpp Commands for vehicles. */

#include "stdafx.h"
#include "roadveh.h"
#include "gfx_func.h"
#include "news_func.h"
#include "command_func.h"
#include "company_func.h"
#include "vehicle_gui.h"
#include "train.h"
#include "newgrf_engine.h"
#include "newgrf_text.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "string_func.h"
#include "depot_map.h"
#include "vehiclelist.h"

#include "table/strings.h"

/* Tables used in vehicle.h to find the right command for a certain vehicle type */
const uint32 _veh_build_proc_table[] = {
	CMD_BUILD_RAIL_VEHICLE,
	CMD_BUILD_ROAD_VEH,
	CMD_BUILD_SHIP,
	CMD_BUILD_AIRCRAFT,
};
const uint32 _veh_sell_proc_table[] = {
	CMD_SELL_RAIL_WAGON,
	CMD_SELL_ROAD_VEH,
	CMD_SELL_SHIP,
	CMD_SELL_AIRCRAFT,
};

const uint32 _veh_refit_proc_table[] = {
	CMD_REFIT_RAIL_VEHICLE,
	CMD_REFIT_ROAD_VEH,
	CMD_REFIT_SHIP,
	CMD_REFIT_AIRCRAFT,
};

const uint32 _send_to_depot_proc_table[] = {
	CMD_SEND_TRAIN_TO_DEPOT,
	CMD_SEND_ROADVEH_TO_DEPOT,
	CMD_SEND_SHIP_TO_DEPOT,
	CMD_SEND_AIRCRAFT_TO_HANGAR,
};

/** Start/Stop a vehicle
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle to start/stop
 * @param p2 bit 0: Shall the start/stop newgrf callback be evaluated (only valid with DC_AUTOREPLACE for network safety)
 * @return result of operation.  Nothing if everything went well
 */
CommandCost CmdStartStopVehicle(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	/* Disable the effect of p2 bit 0, when DC_AUTOREPLACE is not set */
	if ((flags & DC_AUTOREPLACE) == 0) SetBit(p2, 0);

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;
	if (!v->IsPrimaryVehicle()) return CMD_ERROR;

	switch (v->type) {
		case VEH_TRAIN:
			if (v->vehstatus & VS_STOPPED && v->u.rail.cached_power == 0) return_cmd_error(STR_TRAIN_START_NO_CATENARY);
			break;

		case VEH_SHIP:
		case VEH_ROAD:
			break;

		case VEH_AIRCRAFT:
			/* cannot stop airplane when in flight, or when taking off / landing */
			if (v->u.air.state >= STARTTAKEOFF && v->u.air.state < TERM7) return_cmd_error(STR_A017_AIRCRAFT_IS_IN_FLIGHT);
			break;

		default: return CMD_ERROR;
	}

	/* Check if this vehicle can be started/stopped. The callback will fail or
	 * return 0xFF if it can. */
	uint16 callback = GetVehicleCallback(CBID_VEHICLE_START_STOP_CHECK, 0, 0, v->engine_type, v);
	if (callback != CALLBACK_FAILED && GB(callback, 0, 8) != 0xFF && HasBit(p2, 0)) {
		StringID error = GetGRFStringID(GetEngineGRFID(v->engine_type), 0xD000 + callback);
		return_cmd_error(error);
	}

	if (flags & DC_EXEC) {
		static const StringID vehicle_waiting_in_depot[] = {
			STR_8814_TRAIN_IS_WAITING_IN_DEPOT,
			STR_9016_ROAD_VEHICLE_IS_WAITING,
			STR_981C_SHIP_IS_WAITING_IN_DEPOT,
			STR_A014_AIRCRAFT_IS_WAITING_IN,
		};

		static const WindowClass vehicle_list[] = {
			WC_TRAINS_LIST,
			WC_ROADVEH_LIST,
			WC_SHIPS_LIST,
			WC_AIRCRAFT_LIST,
		};

		if (v->IsStoppedInDepot() && (flags & DC_AUTOREPLACE) == 0) DeleteVehicleNews(p1, vehicle_waiting_in_depot[v->type]);

		v->vehstatus ^= VS_STOPPED;
		if (v->type != VEH_TRAIN) v->cur_speed = 0; // trains can stop 'slowly'
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		InvalidateWindowClasses(vehicle_list[v->type]);
	}
	return CommandCost();
}

/** Starts or stops a lot of vehicles
 * @param tile Tile of the depot where the vehicles are started/stopped (only used for depots)
 * @param flags type of operation
 * @param p1 Station/Order/Depot ID (only used for vehicle list windows)
 * @param p2 bitmask
 *   - bit 0-4 Vehicle type
 *   - bit 5 false = start vehicles, true = stop vehicles
 *   - bit 6 if set, then it's a vehicle list window, not a depot and Tile is ignored in this case
 *   - bit 8-11 Vehicle List Window type (ignored unless bit 1 is set)
 */
CommandCost CmdMassStartStopVehicle(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleList list;
	CommandCost return_value = CMD_ERROR;
	VehicleType vehicle_type = (VehicleType)GB(p2, 0, 5);
	bool start_stop = HasBit(p2, 5);
	bool vehicle_list_window = HasBit(p2, 6);

	if (vehicle_list_window) {
		uint32 id = p1;
		uint16 window_type = p2 & VLW_MASK;

		GenerateVehicleSortList(&list, vehicle_type, _current_company, id, window_type);
	} else {
		/* Get the list of vehicles in the depot */
		BuildDepotVehicleList(vehicle_type, tile, &list, NULL);
	}

	for (uint i = 0; i < list.Length(); i++) {
		const Vehicle *v = list[i];

		if (!!(v->vehstatus & VS_STOPPED) != start_stop) continue;

		if (!vehicle_list_window) {
			if (vehicle_type == VEH_TRAIN) {
				if (CheckTrainInDepot(v, false) == -1) continue;
			} else {
				if (!(v->vehstatus & VS_HIDDEN)) continue;
			}
		}

		CommandCost ret = DoCommand(tile, v->index, 0, flags, CMD_START_STOP_VEHICLE);

		if (CmdSucceeded(ret)) {
			return_value = CommandCost();
			/* We know that the command is valid for at least one vehicle.
			 * If we haven't set DC_EXEC, then there is no point in continueing because it will be valid */
			if (!(flags & DC_EXEC)) break;
		}
	}

	return return_value;
}

/** Sells all vehicles in a depot
 * @param tile Tile of the depot where the depot is
 * @param flags type of operation
 * @param p1 Vehicle type
 * @param p2 unused
 */
CommandCost CmdDepotSellAllVehicles(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleList list;

	CommandCost cost(EXPENSES_NEW_VEHICLES);
	uint sell_command;
	VehicleType vehicle_type = (VehicleType)GB(p1, 0, 8);

	switch (vehicle_type) {
		case VEH_TRAIN:    sell_command = CMD_SELL_RAIL_WAGON; break;
		case VEH_ROAD:     sell_command = CMD_SELL_ROAD_VEH;   break;
		case VEH_SHIP:     sell_command = CMD_SELL_SHIP;       break;
		case VEH_AIRCRAFT: sell_command = CMD_SELL_AIRCRAFT;   break;
		default: return CMD_ERROR;
	}

	/* Get the list of vehicles in the depot */
	BuildDepotVehicleList(vehicle_type, tile, &list, &list);

	for (uint i = 0; i < list.Length(); i++) {
		CommandCost ret = DoCommand(tile, list[i]->index, 1, flags, sell_command);
		if (CmdSucceeded(ret)) cost.AddCost(ret);
	}

	if (cost.GetCost() == 0) return CMD_ERROR; // no vehicles to sell
	return cost;
}

/** Autoreplace all vehicles in the depot
 * Note: this command can make incorrect cost estimations
 * Luckily the final price can only drop, not increase. This is due to the fact that
 * estimation can't predict wagon removal so it presumes worst case which is no income from selling wagons.
 * @param tile Tile of the depot where the vehicles are
 * @param flags type of operation
 * @param p1 Type of vehicle
 * @param p2 If bit 0 is set, then either replace all or nothing (instead of replacing until money runs out)
 */
CommandCost CmdDepotMassAutoReplace(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleList list;
	CommandCost cost = CommandCost(EXPENSES_NEW_VEHICLES);
	VehicleType vehicle_type = (VehicleType)GB(p1, 0, 8);
	bool all_or_nothing = HasBit(p2, 0);

	if (!IsDepotTile(tile) || !IsTileOwner(tile, _current_company)) return CMD_ERROR;

	/* Get the list of vehicles in the depot */
	BuildDepotVehicleList(vehicle_type, tile, &list, &list, true);

	bool did_something = false;

	for (uint i = 0; i < list.Length(); i++) {
		Vehicle *v = (Vehicle*)list[i];

		/* Ensure that the vehicle completely in the depot */
		if (!v->IsInDepot()) continue;

		CommandCost ret = DoCommand(0, v->index, 0, flags, CMD_AUTOREPLACE_VEHICLE);

		if (CmdSucceeded(ret)) {
			did_something = true;
			cost.AddCost(ret);
		} else {
			if (ret.GetErrorMessage() != STR_AUTOREPLACE_NOTHING_TO_DO && all_or_nothing) {
				/* We failed to replace a vehicle even though we set all or nothing.
				 * We should never reach this if DC_EXEC is set since then it should
				 * have failed the estimation guess. */
				assert(!(flags & DC_EXEC));
				/* Now we will have to return an error. */
				return CMD_ERROR;
			}
		}
	}

	if (!did_something) {
		/* Either we didn't replace anything or something went wrong.
		 * Either way we want to return an error and not execute this command. */
		cost = CMD_ERROR;
	}

	return cost;
}

/** Test if a name is unique among vehicle names.
 * @param name Name to test.
 * @return True ifffffff the name is unique.
 */
static bool IsUniqueVehicleName(const char *name)
{
	const Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->name != NULL && strcmp(v->name, name) == 0) return false;
	}

	return true;
}

/** Clone the custom name of a vehicle, adding or incrementing a number.
 * @param src Source vehicle, with a custom name.
 * @param dst Destination vehicle.
 */
static void CloneVehicleName(const Vehicle *src, Vehicle *dst)
{
	char buf[256];

	/* Find the position of the first digit in the last group of digits. */
	size_t number_position;
	for (number_position = strlen(src->name); number_position > 0; number_position--) {
		/* The design of UTF-8 lets this work simply without having to check
		 * for UTF-8 sequences. */
		if (src->name[number_position - 1] < '0' || src->name[number_position - 1] > '9') break;
	}

	/* Format buffer and determine starting number. */
	int num;
	if (number_position == strlen(src->name)) {
		/* No digit at the end, so start at number 2. */
		strecpy(buf, src->name, lastof(buf));
		strecat(buf, " ", lastof(buf));
		number_position = strlen(buf);
		num = 2;
	} else {
		/* Found digits, parse them and start at the next number. */
		strecpy(buf, src->name, lastof(buf));
		buf[number_position] = '\0';
		num = strtol(&src->name[number_position], NULL, 10) + 1;
	}

	/* Check if this name is already taken. */
	for (int max_iterations = 1000; max_iterations > 0; max_iterations--, num++) {
		/* Attach the number to the temporary name. */
		seprintf(&buf[number_position], lastof(buf), "%d", num);

		/* Check the name is unique. */
		if (IsUniqueVehicleName(buf)) {
			dst->name = strdup(buf);
			break;
		}
	}

	/* All done. If we didn't find a name, it'll just use its default. */
}

/** Clone a vehicle. If it is a train, it will clone all the cars too
 * @param tile tile of the depot where the cloned vehicle is build
 * @param flags type of operation
 * @param p1 the original vehicle's index
 * @param p2 1 = shared orders, else copied orders
 */
CommandCost CmdCloneVehicle(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CommandCost total_cost(EXPENSES_NEW_VEHICLES);
	uint32 build_argument = 2;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);
	Vehicle *v_front = v;
	Vehicle *w = NULL;
	Vehicle *w_front = NULL;
	Vehicle *w_rear = NULL;

	/*
	 * v_front is the front engine in the original vehicle
	 * v is the car/vehicle of the original vehicle, that is currently being copied
	 * w_front is the front engine of the cloned vehicle
	 * w is the car/vehicle currently being cloned
	 * w_rear is the rear end of the cloned train. It's used to add more cars and is only used by trains
	 */

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->type == VEH_TRAIN && (!IsFrontEngine(v) || v->u.rail.crash_anim_pos >= 4400)) return CMD_ERROR;

	/* check that we can allocate enough vehicles */
	if (!(flags & DC_EXEC)) {
		int veh_counter = 0;
		do {
			veh_counter++;
		} while ((v = v->Next()) != NULL);

		if (!Vehicle::AllocateList(NULL, veh_counter)) {
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);
		}
	}

	v = v_front;

	do {
		if (v->type == VEH_TRAIN && IsRearDualheaded(v)) {
			/* we build the rear ends of multiheaded trains with the front ones */
			continue;
		}

		CommandCost cost = DoCommand(tile, v->engine_type, build_argument, flags, GetCmdBuildVeh(v));
		build_argument = 3; // ensure that we only assign a number to the first engine

		if (CmdFailed(cost)) return cost;

		total_cost.AddCost(cost);

		if (flags & DC_EXEC) {
			w = GetVehicle(_new_vehicle_id);

			if (v->type == VEH_TRAIN && HasBit(v->u.rail.flags, VRF_REVERSE_DIRECTION)) {
				SetBit(w->u.rail.flags, VRF_REVERSE_DIRECTION);
			}

			if (v->type == VEH_TRAIN && !IsFrontEngine(v)) {
				/* this s a train car
				 * add this unit to the end of the train */
				CommandCost result = DoCommand(0, (w_rear->index << 16) | w->index, 1, flags, CMD_MOVE_RAIL_VEHICLE);
				if (CmdFailed(result)) {
					/* The train can't be joined to make the same consist as the original.
					 * Sell what we already made (clean up) and return an error.           */
					DoCommand(w_front->tile, w_front->index, 1, flags, GetCmdSellVeh(w_front));
					DoCommand(w_front->tile, w->index,       1, flags, GetCmdSellVeh(w));
					return result; // return error and the message returned from CMD_MOVE_RAIL_VEHICLE
				}
			} else {
				/* this is a front engine or not a train. */
				w_front = w;
				w->service_interval = v->service_interval;
			}
			w_rear = w; // trains needs to know the last car in the train, so they can add more in next loop
		}
	} while (v->type == VEH_TRAIN && (v = GetNextVehicle(v)) != NULL);

	if (flags & DC_EXEC && v_front->type == VEH_TRAIN) {
		/* for trains this needs to be the front engine due to the callback function */
		_new_vehicle_id = w_front->index;
	}

	if (flags & DC_EXEC) {
		/* Cloned vehicles belong to the same group */
		DoCommand(0, v_front->group_id, w_front->index, flags, CMD_ADD_VEHICLE_GROUP);
	}


	/* Take care of refitting. */
	w = w_front;
	v = v_front;

	/* Both building and refitting are influenced by newgrf callbacks, which
	 * makes it impossible to accurately estimate the cloning costs. In
	 * particular, it is possible for engines of the same type to be built with
	 * different numbers of articulated parts, so when refitting we have to
	 * loop over real vehicles first, and then the articulated parts of those
	 * vehicles in a different loop. */
	do {
		do {
			if (flags & DC_EXEC) {
				assert(w != NULL);

				if (w->cargo_type != v->cargo_type || w->cargo_subtype != v->cargo_subtype) {
					CommandCost cost = DoCommand(0, w->index, v->cargo_type | (v->cargo_subtype << 8) | 1U << 16 , flags, GetCmdRefitVeh(v));
					if (CmdSucceeded(cost)) total_cost.AddCost(cost);
				}

				if (w->type == VEH_TRAIN && EngineHasArticPart(w)) {
					w = GetNextArticPart(w);
				} else if (w->type == VEH_ROAD && RoadVehHasArticPart(w)) {
					w = w->Next();
				} else {
					break;
				}
			} else {
				const Engine *e = GetEngine(v->engine_type);
				CargoID initial_cargo = (e->CanCarryCargo() ? e->GetDefaultCargoType() : (CargoID)CT_INVALID);

				if (v->cargo_type != initial_cargo && initial_cargo != CT_INVALID) {
					total_cost.AddCost(GetRefitCost(v->engine_type));
				}
			}

			if (v->type == VEH_TRAIN && EngineHasArticPart(v)) {
				v = GetNextArticPart(v);
			} else if (v->type == VEH_ROAD && RoadVehHasArticPart(v)) {
				v = v->Next();
			} else {
				break;
			}
		} while (v != NULL);

		if ((flags & DC_EXEC) && v->type == VEH_TRAIN) w = GetNextVehicle(w);
	} while (v->type == VEH_TRAIN && (v = GetNextVehicle(v)) != NULL);

	if (flags & DC_EXEC) {
		/*
		 * Set the orders of the vehicle. Cannot do it earlier as we need
		 * the vehicle refitted before doing this, otherwise the moved
		 * cargo types might not match (passenger vs non-passenger)
		 */
		DoCommand(0, (v_front->index << 16) | w_front->index, p2 & 1 ? CO_SHARE : CO_COPY, flags, CMD_CLONE_ORDER);

		/* Now clone the vehicle's name, if it has one. */
		if (v_front->name != NULL) CloneVehicleName(v_front, w_front);
	}

	/* Since we can't estimate the cost of cloning a vehicle accurately we must
	 * check whether the company has enough money manually. */
	if (!CheckCompanyHasMoney(total_cost)) {
		if (flags & DC_EXEC) {
			/* The vehicle has already been bought, so now it must be sold again. */
			DoCommand(w_front->tile, w_front->index, 1, flags, GetCmdSellVeh(w_front));
		}
		return CMD_ERROR;
	}

	return total_cost;
}

/**
 * Send all vehicles of type to depots
 * @param type type of vehicle
 * @param flags the flags used for DoCommand()
 * @param service should the vehicles only get service in the depots
 * @param owner owner of the vehicles to send
 * @param vlw_flag tells what kind of list requested the goto depot
 * @return 0 for success and CMD_ERROR if no vehicle is able to go to depot
 */
CommandCost SendAllVehiclesToDepot(VehicleType type, DoCommandFlag flags, bool service, Owner owner, uint16 vlw_flag, uint32 id)
{
	VehicleList list;

	GenerateVehicleSortList(&list, type, owner, id, vlw_flag);

	/* Send all the vehicles to a depot */
	for (uint i = 0; i < list.Length(); i++) {
		const Vehicle *v = list[i];
		CommandCost ret = DoCommand(v->tile, v->index, (service ? 1 : 0) | DEPOT_DONT_CANCEL, flags, GetCmdSendToDepot(type));

		/* Return 0 if DC_EXEC is not set this is a valid goto depot command)
		 * In this case we know that at least one vehicle can be sent to a depot
		 * and we will issue the command. We can now safely quit the loop, knowing
		 * it will succeed at least once. With DC_EXEC we really need to send them to the depot */
		if (CmdSucceeded(ret) && !(flags & DC_EXEC)) {
			return CommandCost();
		}
	}

	return (flags & DC_EXEC) ? CommandCost() : CMD_ERROR;
}

/** Give a custom name to your vehicle
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID to name
 * @param p2 unused
 */
CommandCost CmdRenameVehicle(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);
	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	bool reset = StrEmpty(text);

	if (!reset) {
		if (strlen(text) >= MAX_LENGTH_VEHICLE_NAME_BYTES) return CMD_ERROR;
		if (!(flags & DC_AUTOREPLACE) && !IsUniqueVehicleName(text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		free(v->name);
		v->name = reset ? NULL : strdup(text);
		InvalidateWindowClassesData(WC_TRAINS_LIST, 1);
		MarkWholeScreenDirty();
	}

	return CommandCost();
}


/** Change the service interval of a vehicle
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID that is being service-interval-changed
 * @param p2 new service interval
 */
CommandCost CmdChangeServiceInt(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	uint16 serv_int = GetServiceIntervalClamped(p2); // Double check the service interval from the user-input

	if (serv_int != p2 || !IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->service_interval = serv_int;
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	}

	return CommandCost();
}
