/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehicle_cmd.cpp Commands for vehicles. */

#include "stdafx.h"
#include "roadveh.h"
#include "news_func.h"
#include "airport.h"
#include "command_func.h"
#include "company_func.h"
#include "train.h"
#include "aircraft.h"
#include "newgrf_text.h"
#include "vehicle_func.h"
#include "string_func.h"
#include "depot_map.h"
#include "vehiclelist.h"
#include "engine_func.h"
#include "articulated_vehicles.h"
#include "autoreplace_gui.h"
#include "group.h"
#include "order_backup.h"
#include "ship.h"
#include "newgrf.h"
#include "company_base.h"
#include "core/random_func.hpp"
#include "vehicle_cmd.h"
#include "aircraft_cmd.h"
#include "autoreplace_cmd.h"
#include "group_cmd.h"
#include "order_cmd.h"
#include "roadveh_cmd.h"
#include "train_cmd.h"
#include "ship_cmd.h"
#include <sstream>
#include <iomanip>

#include "table/strings.h"

#include "safeguards.h"

/* Tables used in vehicle_func.h to find the right error message for a certain vehicle type */
const StringID _veh_build_msg_table[] = {
	STR_ERROR_CAN_T_BUY_TRAIN,
	STR_ERROR_CAN_T_BUY_ROAD_VEHICLE,
	STR_ERROR_CAN_T_BUY_SHIP,
	STR_ERROR_CAN_T_BUY_AIRCRAFT,
};

const StringID _veh_sell_msg_table[] = {
	STR_ERROR_CAN_T_SELL_TRAIN,
	STR_ERROR_CAN_T_SELL_ROAD_VEHICLE,
	STR_ERROR_CAN_T_SELL_SHIP,
	STR_ERROR_CAN_T_SELL_AIRCRAFT,
};

const StringID _veh_refit_msg_table[] = {
	STR_ERROR_CAN_T_REFIT_TRAIN,
	STR_ERROR_CAN_T_REFIT_ROAD_VEHICLE,
	STR_ERROR_CAN_T_REFIT_SHIP,
	STR_ERROR_CAN_T_REFIT_AIRCRAFT,
};

const StringID _send_to_depot_msg_table[] = {
	STR_ERROR_CAN_T_SEND_TRAIN_TO_DEPOT,
	STR_ERROR_CAN_T_SEND_ROAD_VEHICLE_TO_DEPOT,
	STR_ERROR_CAN_T_SEND_SHIP_TO_DEPOT,
	STR_ERROR_CAN_T_SEND_AIRCRAFT_TO_HANGAR,
};


/**
 * Build a vehicle.
 * @param flags for command
 * @param tile tile of depot where the vehicle is built
 * @param eid vehicle type being built.
 * @param use_free_vehicles use free vehicles when building the vehicle.
 * @param cargo refit cargo type.
 * @param client_id User
 * @return the cost of this operation + the new vehicle ID + the refitted capacity + the refitted mail capacity (aircraft) or an error
 */
std::tuple<CommandCost, VehicleID, uint, uint16_t, CargoArray> CmdBuildVehicle(DoCommandFlag flags, TileIndex tile, EngineID eid, bool use_free_vehicles, CargoID cargo, ClientID client_id)
{
	/* Elementary check for valid location. */
	if (!IsDepotTile(tile) || !IsTileOwner(tile, _current_company)) return { CMD_ERROR, INVALID_VEHICLE, 0, 0, {} };

	VehicleType type = GetDepotVehicleType(tile);

	/* Validate the engine type. */
	if (!IsEngineBuildable(eid, type, _current_company)) return { CommandCost(STR_ERROR_RAIL_VEHICLE_NOT_AVAILABLE + type), INVALID_VEHICLE, 0, 0, {} };

	/* Validate the cargo type. */
	if (cargo >= NUM_CARGO && IsValidCargoID(cargo)) return { CMD_ERROR, INVALID_VEHICLE, 0, 0, {} };

	const Engine *e = Engine::Get(eid);
	CommandCost value(EXPENSES_NEW_VEHICLES, e->GetCost());

	/* Engines without valid cargo should not be available */
	CargoID default_cargo = e->GetDefaultCargoType();
	if (!IsValidCargoID(default_cargo)) return { CMD_ERROR, INVALID_VEHICLE, 0, 0, {} };

	bool refitting = IsValidCargoID(cargo) && cargo != default_cargo;

	/* Check whether the number of vehicles we need to build can be built according to pool space. */
	uint num_vehicles;
	switch (type) {
		case VEH_TRAIN:    num_vehicles = (e->u.rail.railveh_type == RAILVEH_MULTIHEAD ? 2 : 1) + CountArticulatedParts(eid, false); break;
		case VEH_ROAD:     num_vehicles = 1 + CountArticulatedParts(eid, false); break;
		case VEH_SHIP:     num_vehicles = 1; break;
		case VEH_AIRCRAFT: num_vehicles = e->u.air.subtype & AIR_CTOL ? 2 : 3; break;
		default: NOT_REACHED(); // Safe due to IsDepotTile()
	}
	if (!Vehicle::CanAllocateItem(num_vehicles)) return { CommandCost(STR_ERROR_TOO_MANY_VEHICLES_IN_GAME), INVALID_VEHICLE, 0, 0, {} };

	/* Check whether we can allocate a unit number. Autoreplace does not allocate
	 * an unit number as it will (always) reuse the one of the replaced vehicle
	 * and (train) wagons don't have an unit number in any scenario. */
	UnitID unit_num = (flags & DC_QUERY_COST || flags & DC_AUTOREPLACE || (type == VEH_TRAIN && e->u.rail.railveh_type == RAILVEH_WAGON)) ? 0 : GetFreeUnitNumber(type);
	if (unit_num == UINT16_MAX) return { CommandCost(STR_ERROR_TOO_MANY_VEHICLES_IN_GAME), INVALID_VEHICLE, 0, 0, {} };

	/* If we are refitting we need to temporarily purchase the vehicle to be able to
	 * test it. */
	DoCommandFlag subflags = flags;
	if (refitting && !(flags & DC_EXEC)) subflags |= DC_EXEC | DC_AUTOREPLACE;

	/* Vehicle construction needs random bits, so we have to save the random
	 * seeds to prevent desyncs. */
	SavedRandomSeeds saved_seeds;
	SaveRandomSeeds(&saved_seeds);

	Vehicle *v = nullptr;
	switch (type) {
		case VEH_TRAIN:    value.AddCost(CmdBuildRailVehicle(subflags, tile, e, &v)); break;
		case VEH_ROAD:     value.AddCost(CmdBuildRoadVehicle(subflags, tile, e, &v)); break;
		case VEH_SHIP:     value.AddCost(CmdBuildShip       (subflags, tile, e, &v)); break;
		case VEH_AIRCRAFT: value.AddCost(CmdBuildAircraft   (subflags, tile, e, &v)); break;
		default: NOT_REACHED(); // Safe due to IsDepotTile()
	}

	VehicleID veh_id = INVALID_VEHICLE;
	uint refitted_capacity = 0;
	uint16_t refitted_mail_capacity = 0;
	CargoArray cargo_capacities{};
	if (value.Succeeded()) {
		if (subflags & DC_EXEC) {
			v->unitnumber = unit_num;
			v->value      = value.GetCost();
			veh_id        = v->index;
		}

		if (refitting) {
			/* Refit only one vehicle. If we purchased an engine, it may have gained free wagons. */
			CommandCost cc;
			std::tie(cc, refitted_capacity, refitted_mail_capacity, cargo_capacities) = CmdRefitVehicle(flags, v->index, cargo, 0, false, false, 1);
			value.AddCost(cc);
		} else {
			/* Fill in non-refitted capacities */
			if (e->type == VEH_TRAIN || e->type == VEH_ROAD) {
				cargo_capacities = GetCapacityOfArticulatedParts(eid);
				refitted_capacity = cargo_capacities[default_cargo];
				refitted_mail_capacity = 0;
			} else {
				refitted_capacity = e->GetDisplayDefaultCapacity(&refitted_mail_capacity);
				cargo_capacities[default_cargo] = refitted_capacity;
				cargo_capacities[CT_MAIL] = refitted_mail_capacity;
			}
		}

		if (flags & DC_EXEC) {
			if (type == VEH_TRAIN && use_free_vehicles && !(flags & DC_AUTOREPLACE) && Train::From(v)->IsEngine()) {
				/* Move any free wagons to the new vehicle. */
				NormalizeTrainVehInDepot(Train::From(v));
			}

			InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
			InvalidateWindowClassesData(GetWindowClassForVehicleType(type), 0);
			SetWindowDirty(WC_COMPANY, _current_company);
			if (IsLocalCompany()) {
				InvalidateAutoreplaceWindow(v->engine_type, v->group_id); // updates the auto replace window (must be called before incrementing num_engines)
			}
		}

		if (subflags & DC_EXEC) {
			GroupStatistics::CountEngine(v, 1);
			GroupStatistics::UpdateAutoreplace(_current_company);

			if (v->IsPrimaryVehicle()) {
				GroupStatistics::CountVehicle(v, 1);
				if (!(subflags & DC_AUTOREPLACE)) OrderBackup::Restore(v, client_id);
			}
		}


		/* If we are not in DC_EXEC undo everything */
		if (flags != subflags) {
			Command<CMD_SELL_VEHICLE>::Do(DC_EXEC, v->index, false, false, INVALID_CLIENT_ID);
		}
	}

	/* Only restore if we actually did some refitting */
	if (flags != subflags) RestoreRandomSeeds(saved_seeds);

	return { value, veh_id, refitted_capacity, refitted_mail_capacity, cargo_capacities };
}

/**
 * Sell a vehicle.
 * @param flags for command.
 * @param v_id vehicle ID being sold.
 * @param sell_chain sell the vehicle and all vehicles following it in the chain.
 * @param backup_order make a backup of the vehicle's order (if an engine).
 * @param client_id User.
 * @return the cost of this operation or an error.
 */
CommandCost CmdSellVehicle(DoCommandFlag flags, VehicleID v_id, bool sell_chain, bool backup_order, ClientID client_id)
{
	Vehicle *v = Vehicle::GetIfValid(v_id);
	if (v == nullptr) return CMD_ERROR;

	Vehicle *front = v->First();

	CommandCost ret = CheckOwnership(front->owner);
	if (ret.Failed()) return ret;

	if (front->vehstatus & VS_CRASHED) return_cmd_error(STR_ERROR_VEHICLE_IS_DESTROYED);

	if (!front->IsStoppedInDepot()) return_cmd_error(STR_ERROR_TRAIN_MUST_BE_STOPPED_INSIDE_DEPOT + front->type);

	/* Can we actually make the order backup, i.e. are there enough orders? */
	if (backup_order &&
			front->orders != nullptr &&
			!front->orders->IsShared() &&
			!Order::CanAllocateItem(front->orders->GetNumOrders())) {
		/* Only happens in exceptional cases when there aren't enough orders anyhow.
		 * Thus it should be safe to just drop the orders in that case. */
		backup_order = false;
	}

	if (v->type == VEH_TRAIN) {
		ret = CmdSellRailWagon(flags, v, sell_chain, backup_order, client_id);
	} else {
		ret = CommandCost(EXPENSES_NEW_VEHICLES, -front->value);

		if (flags & DC_EXEC) {
			if (front->IsPrimaryVehicle() && backup_order) OrderBackup::Backup(front, client_id);
			delete front;
		}
	}

	return ret;
}

/**
 * Helper to run the refit cost callback.
 * @param v The vehicle we are refitting, can be nullptr.
 * @param engine_type Which engine to refit
 * @param new_cid Cargo type we are refitting to.
 * @param new_subtype New cargo subtype.
 * @param[out] auto_refit_allowed The refit is allowed as an auto-refit.
 * @return Price for refitting
 */
static int GetRefitCostFactor(const Vehicle *v, EngineID engine_type, CargoID new_cid, byte new_subtype, bool *auto_refit_allowed)
{
	/* Prepare callback param with info about the new cargo type. */
	const Engine *e = Engine::Get(engine_type);

	/* Is this vehicle a NewGRF vehicle? */
	if (e->GetGRF() != nullptr) {
		const CargoSpec *cs = CargoSpec::Get(new_cid);
		uint32_t param1 = (cs->classes << 16) | (new_subtype << 8) | e->GetGRF()->cargo_map[new_cid];

		uint16_t cb_res = GetVehicleCallback(CBID_VEHICLE_REFIT_COST, param1, 0, engine_type, v);
		if (cb_res != CALLBACK_FAILED) {
			*auto_refit_allowed = HasBit(cb_res, 14);
			int factor = GB(cb_res, 0, 14);
			if (factor >= 0x2000) factor -= 0x4000; // Treat as signed integer.
			return factor;
		}
	}

	*auto_refit_allowed = e->info.refit_cost == 0;
	return (v == nullptr || v->cargo_type != new_cid) ? e->info.refit_cost : 0;
}

/**
 * Learn the price of refitting a certain engine
 * @param v The vehicle we are refitting, can be nullptr.
 * @param engine_type Which engine to refit
 * @param new_cid Cargo type we are refitting to.
 * @param new_subtype New cargo subtype.
 * @param[out] auto_refit_allowed The refit is allowed as an auto-refit.
 * @return Price for refitting
 */
static CommandCost GetRefitCost(const Vehicle *v, EngineID engine_type, CargoID new_cid, byte new_subtype, bool *auto_refit_allowed)
{
	ExpensesType expense_type;
	const Engine *e = Engine::Get(engine_type);
	Price base_price;
	int cost_factor = GetRefitCostFactor(v, engine_type, new_cid, new_subtype, auto_refit_allowed);
	switch (e->type) {
		case VEH_SHIP:
			base_price = PR_BUILD_VEHICLE_SHIP;
			expense_type = EXPENSES_SHIP_RUN;
			break;

		case VEH_ROAD:
			base_price = PR_BUILD_VEHICLE_ROAD;
			expense_type = EXPENSES_ROADVEH_RUN;
			break;

		case VEH_AIRCRAFT:
			base_price = PR_BUILD_VEHICLE_AIRCRAFT;
			expense_type = EXPENSES_AIRCRAFT_RUN;
			break;

		case VEH_TRAIN:
			base_price = (e->u.rail.railveh_type == RAILVEH_WAGON) ? PR_BUILD_VEHICLE_WAGON : PR_BUILD_VEHICLE_TRAIN;
			cost_factor <<= 1;
			expense_type = EXPENSES_TRAIN_RUN;
			break;

		default: NOT_REACHED();
	}
	if (cost_factor < 0) {
		return CommandCost(expense_type, -GetPrice(base_price, -cost_factor, e->GetGRF(), -10));
	} else {
		return CommandCost(expense_type, GetPrice(base_price, cost_factor, e->GetGRF(), -10));
	}
}

/** Helper structure for RefitVehicle() */
struct RefitResult {
	Vehicle *v;         ///< Vehicle to refit
	uint capacity;      ///< New capacity of vehicle
	uint mail_capacity; ///< New mail capacity of aircraft
	byte subtype;       ///< cargo subtype to refit to
};

/**
 * Refits a vehicle (chain).
 * This is the vehicle-type independent part of the CmdRefitXXX functions.
 * @param v            The vehicle to refit.
 * @param only_this    Whether to only refit this vehicle, or to check the rest of them.
 * @param num_vehicles Number of vehicles to refit (not counting articulated parts). Zero means the whole chain.
 * @param new_cid      Cargotype to refit to
 * @param new_subtype  Cargo subtype to refit to. 0xFF means to try keeping the same subtype according to GetBestFittingSubType().
 * @param flags        Command flags
 * @param auto_refit   Refitting is done as automatic refitting outside a depot.
 * @return Refit cost + refittet capacity + mail capacity (aircraft).
 */
static std::tuple<CommandCost, uint, uint16_t, CargoArray> RefitVehicle(Vehicle *v, bool only_this, uint8_t num_vehicles, CargoID new_cid, byte new_subtype, DoCommandFlag flags, bool auto_refit)
{
	CommandCost cost(v->GetExpenseType(false));
	uint total_capacity = 0;
	uint total_mail_capacity = 0;
	num_vehicles = num_vehicles == 0 ? UINT8_MAX : num_vehicles;
	CargoArray cargo_capacities{};

	VehicleSet vehicles_to_refit;
	if (!only_this) {
		GetVehicleSet(vehicles_to_refit, v, num_vehicles);
		/* In this case, we need to check the whole chain. */
		v = v->First();
	}

	std::vector<RefitResult> refit_result;

	v->InvalidateNewGRFCacheOfChain();
	byte actual_subtype = new_subtype;
	for (; v != nullptr; v = (only_this ? nullptr : v->Next())) {
		/* Reset actual_subtype for every new vehicle */
		if (!v->IsArticulatedPart()) actual_subtype = new_subtype;

		if (v->type == VEH_TRAIN && std::find(vehicles_to_refit.begin(), vehicles_to_refit.end(), v->index) == vehicles_to_refit.end() && !only_this) continue;

		const Engine *e = v->GetEngine();
		if (!e->CanCarryCargo()) continue;

		/* If the vehicle is not refittable, or does not allow automatic refitting,
		 * count its capacity nevertheless if the cargo matches */
		bool refittable = HasBit(e->info.refit_mask, new_cid) && (!auto_refit || HasBit(e->info.misc_flags, EF_AUTO_REFIT));
		if (!refittable && v->cargo_type != new_cid) {
			uint amount = e->DetermineCapacity(v, nullptr);
			if (amount > 0) cargo_capacities[v->cargo_type] += amount;
			continue;
		}

		/* Determine best fitting subtype if requested */
		if (actual_subtype == 0xFF) {
			actual_subtype = GetBestFittingSubType(v, v, new_cid);
		}

		/* Back up the vehicle's cargo type */
		CargoID temp_cid = v->cargo_type;
		byte temp_subtype = v->cargo_subtype;
		if (refittable) {
			v->cargo_type = new_cid;
			v->cargo_subtype = actual_subtype;
		}

		uint16_t mail_capacity = 0;
		uint amount = e->DetermineCapacity(v, &mail_capacity);
		total_capacity += amount;
		/* mail_capacity will always be zero if the vehicle is not an aircraft. */
		total_mail_capacity += mail_capacity;

		cargo_capacities[new_cid] += amount;
		cargo_capacities[CT_MAIL] += mail_capacity;

		if (!refittable) continue;

		/* Restore the original cargo type */
		v->cargo_type = temp_cid;
		v->cargo_subtype = temp_subtype;

		bool auto_refit_allowed;
		CommandCost refit_cost = GetRefitCost(v, v->engine_type, new_cid, actual_subtype, &auto_refit_allowed);
		if (auto_refit && (flags & DC_QUERY_COST) == 0 && !auto_refit_allowed) {
			/* Sorry, auto-refitting not allowed, subtract the cargo amount again from the total.
			 * When querrying cost/capacity (for example in order refit GUI), we always assume 'allowed'.
			 * It is not predictable. */
			total_capacity -= amount;
			total_mail_capacity -= mail_capacity;

			if (v->cargo_type == new_cid) {
				/* Add the old capacity nevertheless, if the cargo matches */
				total_capacity += v->cargo_cap;
				if (v->type == VEH_AIRCRAFT) total_mail_capacity += v->Next()->cargo_cap;
			}
			continue;
		}
		cost.AddCost(refit_cost);

		/* Record the refitting.
		 * Do not execute the refitting immediately, so DetermineCapacity and GetRefitCost do the same in test and exec run.
		 * (weird NewGRFs)
		 * Note:
		 *  - If the capacity of vehicles depends on other vehicles in the chain, the actual capacity is
		 *    set after RefitVehicle() via ConsistChanged() and friends. The estimation via _returned_refit_capacity will be wrong.
		 *  - We have to call the refit cost callback with the pre-refit configuration of the chain because we want refit and
		 *    autorefit to behave the same, and we need its result for auto_refit_allowed.
		 */
		refit_result.push_back({v, amount, mail_capacity, actual_subtype});
	}

	if (flags & DC_EXEC) {
		/* Store the result */
		for (RefitResult &result : refit_result) {
			Vehicle *u = result.v;
			u->refit_cap = (u->cargo_type == new_cid) ? std::min<uint16_t>(result.capacity, u->refit_cap) : 0;
			if (u->cargo.TotalCount() > u->refit_cap) u->cargo.Truncate(u->cargo.TotalCount() - u->refit_cap);
			u->cargo_type = new_cid;
			u->cargo_cap = result.capacity;
			u->cargo_subtype = result.subtype;
			if (u->type == VEH_AIRCRAFT) {
				Vehicle *w = u->Next();
				assert(w != nullptr);
				w->refit_cap = std::min<uint16_t>(w->refit_cap, result.mail_capacity);
				w->cargo_cap = result.mail_capacity;
				if (w->cargo.TotalCount() > w->refit_cap) w->cargo.Truncate(w->cargo.TotalCount() - w->refit_cap);
			}
		}
	}

	refit_result.clear();
	return { cost, total_capacity, total_mail_capacity, cargo_capacities };
}

/**
 * Refits a vehicle to the specified cargo type.
 * @param flags type of operation
 * @param veh_id vehicle ID to refit
 * @param new_cid New cargo type to refit to.
 * @param new_subtype New cargo subtype to refit to. 0xFF means to try keeping the same subtype according to GetBestFittingSubType().
 * @param auto_refit Automatic refitting.
 * @param only_this Refit only this vehicle. Used only for cloning vehicles.
 * @param num_vehicles Number of vehicles to refit (not counting articulated parts). Zero means all vehicles.
 *                     Only used if "refit only this vehicle" is false.
 * @return the cost of this operation or an error
 */
std::tuple<CommandCost, uint, uint16_t, CargoArray> CmdRefitVehicle(DoCommandFlag flags, VehicleID veh_id, CargoID new_cid, byte new_subtype, bool auto_refit, bool only_this, uint8_t num_vehicles)
{
	Vehicle *v = Vehicle::GetIfValid(veh_id);
	if (v == nullptr) return { CMD_ERROR, 0, 0, {} };

	/* Don't allow disasters and sparks and such to be refitted.
	 * We cannot check for IsPrimaryVehicle as autoreplace also refits in free wagon chains. */
	if (!IsCompanyBuildableVehicleType(v->type)) return { CMD_ERROR, 0, 0, {} };

	Vehicle *front = v->First();

	CommandCost ret = CheckOwnership(front->owner);
	if (ret.Failed()) return { ret, 0, 0, {} };

	bool free_wagon = v->type == VEH_TRAIN && Train::From(front)->IsFreeWagon(); // used by autoreplace/renew

	/* Don't allow shadows and such to be refitted. */
	if (v != front && (v->type == VEH_SHIP || v->type == VEH_AIRCRAFT)) return { CMD_ERROR, 0, 0, {} };

	/* Allow auto-refitting only during loading and normal refitting only in a depot. */
	if ((flags & DC_QUERY_COST) == 0 && // used by the refit GUI, including the order refit GUI.
			!free_wagon && // used by autoreplace/renew
			(!auto_refit || !front->current_order.IsType(OT_LOADING)) && // refit inside stations
			!front->IsStoppedInDepot()) { // refit inside depots
		return { CommandCost(STR_ERROR_TRAIN_MUST_BE_STOPPED_INSIDE_DEPOT + front->type), 0, 0, {} };
	}

	if (front->vehstatus & VS_CRASHED) return { CommandCost(STR_ERROR_VEHICLE_IS_DESTROYED), 0, 0, {} };

	/* Check cargo */
	if (new_cid >= NUM_CARGO) return { CMD_ERROR, 0, 0, {} };

	/* For ships and aircraft there is always only one. */
	only_this |= front->type == VEH_SHIP || front->type == VEH_AIRCRAFT;

	auto [cost, refit_capacity, mail_capacity, cargo_capacities] = RefitVehicle(v, only_this, num_vehicles, new_cid, new_subtype, flags, auto_refit);

	if (flags & DC_EXEC) {
		/* Update the cached variables */
		switch (v->type) {
			case VEH_TRAIN:
				Train::From(front)->ConsistChanged(auto_refit ? CCF_AUTOREFIT : CCF_REFIT);
				break;
			case VEH_ROAD:
				RoadVehUpdateCache(RoadVehicle::From(front), auto_refit);
				if (_settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL) RoadVehicle::From(front)->CargoChanged();
				break;

			case VEH_SHIP:
				v->InvalidateNewGRFCacheOfChain();
				Ship::From(v)->UpdateCache();
				break;

			case VEH_AIRCRAFT:
				v->InvalidateNewGRFCacheOfChain();
				UpdateAircraftCache(Aircraft::From(v), true);
				break;

			default: NOT_REACHED();
		}
		front->MarkDirty();

		if (!free_wagon) {
			InvalidateWindowData(WC_VEHICLE_DETAILS, front->index);
			InvalidateWindowClassesData(GetWindowClassForVehicleType(v->type), 0);
		}
		SetWindowDirty(WC_VEHICLE_DEPOT, front->tile);
	} else {
		/* Always invalidate the cache; querycost might have filled it. */
		v->InvalidateNewGRFCacheOfChain();
	}

	return { cost, refit_capacity, mail_capacity, cargo_capacities };
}

/**
 * Start/Stop a vehicle
 * @param flags type of operation
 * @param veh_id vehicle to start/stop, don't forget to change CcStartStopVehicle if you modify this!
 * @param evaluate_startstop_cb Shall the start/stop newgrf callback be evaluated (only valid with DC_AUTOREPLACE for network safety)
 * @return the cost of this operation or an error
 */
CommandCost CmdStartStopVehicle(DoCommandFlag flags, VehicleID veh_id, bool evaluate_startstop_cb)
{
	/* Disable the effect of p2 bit 0, when DC_AUTOREPLACE is not set */
	if ((flags & DC_AUTOREPLACE) == 0) evaluate_startstop_cb = true;

	Vehicle *v = Vehicle::GetIfValid(veh_id);
	if (v == nullptr || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (v->vehstatus & VS_CRASHED) return_cmd_error(STR_ERROR_VEHICLE_IS_DESTROYED);

	switch (v->type) {
		case VEH_TRAIN:
			if ((v->vehstatus & VS_STOPPED) && Train::From(v)->gcache.cached_power == 0) return_cmd_error(STR_ERROR_TRAIN_START_NO_POWER);
			break;

		case VEH_SHIP:
		case VEH_ROAD:
			break;

		case VEH_AIRCRAFT: {
			Aircraft *a = Aircraft::From(v);
			/* cannot stop airplane when in flight, or when taking off / landing */
			if (a->state >= STARTTAKEOFF && a->state < TERM7) return_cmd_error(STR_ERROR_AIRCRAFT_IS_IN_FLIGHT);
			if (HasBit(a->flags, VAF_HELI_DIRECT_DESCENT)) return_cmd_error(STR_ERROR_AIRCRAFT_IS_IN_FLIGHT);
			break;
		}

		default: return CMD_ERROR;
	}

	if (evaluate_startstop_cb) {
		/* Check if this vehicle can be started/stopped. Failure means 'allow'. */
		uint16_t callback = GetVehicleCallback(CBID_VEHICLE_START_STOP_CHECK, 0, 0, v->engine_type, v);
		StringID error = STR_NULL;
		if (callback != CALLBACK_FAILED) {
			if (v->GetGRF()->grf_version < 8) {
				/* 8 bit result 0xFF means 'allow' */
				if (callback < 0x400 && GB(callback, 0, 8) != 0xFF) error = GetGRFStringID(v->GetGRFID(), 0xD000 + callback);
			} else {
				if (callback < 0x400) {
					error = GetGRFStringID(v->GetGRFID(), 0xD000 + callback);
				} else {
					switch (callback) {
						case 0x400: // allow
							break;

						default: // unknown reason -> disallow
							error = STR_ERROR_INCOMPATIBLE_RAIL_TYPES;
							break;
					}
				}
			}
		}
		if (error != STR_NULL) return_cmd_error(error);
	}

	if (flags & DC_EXEC) {
		if (v->IsStoppedInDepot() && (flags & DC_AUTOREPLACE) == 0) DeleteVehicleNews(veh_id, STR_NEWS_TRAIN_IS_WAITING + v->type);

		v->vehstatus ^= VS_STOPPED;
		if (v->type != VEH_TRAIN) v->cur_speed = 0; // trains can stop 'slowly'
		v->MarkDirty();
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, WID_VV_START_STOP);
		SetWindowDirty(WC_VEHICLE_DEPOT, v->tile);
		SetWindowClassesDirty(GetWindowClassForVehicleType(v->type));
		InvalidateWindowData(WC_VEHICLE_VIEW, v->index);
	}
	return CommandCost();
}

/**
 * Starts or stops a lot of vehicles
 * @param flags type of operation
 * @param tile Tile of the depot where the vehicles are started/stopped (only used for depots)
 * @param do_start set = start vehicles, unset = stop vehicles
 * @param vehicle_list_window if set, then it's a vehicle list window, not a depot and Tile is ignored in this case
 * @param vli VehicleListIdentifier
 * @return the cost of this operation or an error
 */
CommandCost CmdMassStartStopVehicle(DoCommandFlag flags, TileIndex tile, bool do_start, bool vehicle_list_window, const VehicleListIdentifier &vli)
{
	VehicleList list;

	if (!vli.Valid()) return CMD_ERROR;
	if (!IsCompanyBuildableVehicleType(vli.vtype)) return CMD_ERROR;

	if (vehicle_list_window) {
		if (!GenerateVehicleSortList(&list, vli)) return CMD_ERROR;
	} else {
		if (!IsDepotTile(tile) || !IsTileOwner(tile, _current_company)) return CMD_ERROR;
		/* Get the list of vehicles in the depot */
		BuildDepotVehicleList(vli.vtype, tile, &list, nullptr);
	}

	for (uint i = 0; i < list.size(); i++) {
		const Vehicle *v = list[i];

		if (!!(v->vehstatus & VS_STOPPED) != do_start) continue;

		if (!vehicle_list_window && !v->IsChainInDepot()) continue;

		/* Just try and don't care if some vehicle's can't be stopped. */
		Command<CMD_START_STOP_VEHICLE>::Do(flags, v->index, false);
	}

	return CommandCost();
}

/**
 * Sells all vehicles in a depot
 * @param flags type of operation
 * @param tile Tile of the depot where the depot is
 * @param vehicle_type Vehicle type
 * @return the cost of this operation or an error
 */
CommandCost CmdDepotSellAllVehicles(DoCommandFlag flags, TileIndex tile, VehicleType vehicle_type)
{
	VehicleList list;

	CommandCost cost(EXPENSES_NEW_VEHICLES);

	if (!IsCompanyBuildableVehicleType(vehicle_type)) return CMD_ERROR;
	if (!IsDepotTile(tile) || !IsTileOwner(tile, _current_company)) return CMD_ERROR;

	/* Get the list of vehicles in the depot */
	BuildDepotVehicleList(vehicle_type, tile, &list, &list);

	CommandCost last_error = CMD_ERROR;
	bool had_success = false;
	for (uint i = 0; i < list.size(); i++) {
		CommandCost ret = Command<CMD_SELL_VEHICLE>::Do(flags, list[i]->index, true, false, INVALID_CLIENT_ID);
		if (ret.Succeeded()) {
			cost.AddCost(ret);
			had_success = true;
		} else {
			last_error = ret;
		}
	}

	return had_success ? cost : last_error;
}

/**
 * Autoreplace all vehicles in the depot
 * @param flags type of operation
 * @param tile Tile of the depot where the vehicles are
 * @param vehicle_type Type of vehicle
 * @return the cost of this operation or an error
 */
CommandCost CmdDepotMassAutoReplace(DoCommandFlag flags, TileIndex tile, VehicleType vehicle_type)
{
	VehicleList list;
	CommandCost cost = CommandCost(EXPENSES_NEW_VEHICLES);

	if (!IsCompanyBuildableVehicleType(vehicle_type)) return CMD_ERROR;
	if (!IsDepotTile(tile) || !IsTileOwner(tile, _current_company)) return CMD_ERROR;

	/* Get the list of vehicles in the depot */
	BuildDepotVehicleList(vehicle_type, tile, &list, &list, true);

	for (uint i = 0; i < list.size(); i++) {
		const Vehicle *v = list[i];

		/* Ensure that the vehicle completely in the depot */
		if (!v->IsChainInDepot()) continue;

		CommandCost ret = Command<CMD_AUTOREPLACE_VEHICLE>::Do(flags, v->index);

		if (ret.Succeeded()) cost.AddCost(ret);
	}
	return cost;
}

/**
 * Test if a name is unique among vehicle names.
 * @param name Name to test.
 * @return True ifffffff the name is unique.
 */
bool IsUniqueVehicleName(const std::string &name)
{
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (!v->name.empty() && v->name == name) return false;
	}

	return true;
}

/**
 * Clone the custom name of a vehicle, adding or incrementing a number.
 * @param src Source vehicle, with a custom name.
 * @param dst Destination vehicle.
 */
static void CloneVehicleName(const Vehicle *src, Vehicle *dst)
{
	std::string buf;

	/* Find the position of the first digit in the last group of digits. */
	size_t number_position;
	for (number_position = src->name.length(); number_position > 0; number_position--) {
		/* The design of UTF-8 lets this work simply without having to check
		 * for UTF-8 sequences. */
		if (src->name[number_position - 1] < '0' || src->name[number_position - 1] > '9') break;
	}

	/* Format buffer and determine starting number. */
	long num;
	byte padding = 0;
	if (number_position == src->name.length()) {
		/* No digit at the end, so start at number 2. */
		buf = src->name;
		buf += " ";
		number_position = buf.length();
		num = 2;
	} else {
		/* Found digits, parse them and start at the next number. */
		buf = src->name.substr(0, number_position);

		auto num_str = src->name.substr(number_position);
		padding = (byte)num_str.length();

		std::istringstream iss(num_str);
		iss >> num;
		num++;
	}

	/* Check if this name is already taken. */
	for (int max_iterations = 1000; max_iterations > 0; max_iterations--, num++) {
		std::ostringstream oss;

		/* Attach the number to the temporary name. */
		oss << buf << std::setw(padding) << std::setfill('0') << std::internal << num;

		/* Check the name is unique. */
		auto new_name = oss.str();
		if (IsUniqueVehicleName(new_name)) {
			dst->name = new_name;
			break;
		}
	}

	/* All done. If we didn't find a name, it'll just use its default. */
}

/**
 * Clone a vehicle. If it is a train, it will clone all the cars too
 * @param flags type of operation
 * @param tile tile of the depot where the cloned vehicle is build
 * @param veh_id the original vehicle's index
 * @param share_orders shared orders, else copied orders
 * @return the cost of this operation + the new vehicle ID or an error
 */
std::tuple<CommandCost, VehicleID> CmdCloneVehicle(DoCommandFlag flags, TileIndex tile, VehicleID veh_id, bool share_orders)
{
	CommandCost total_cost(EXPENSES_NEW_VEHICLES);

	Vehicle *v = Vehicle::GetIfValid(veh_id);
	if (v == nullptr || !v->IsPrimaryVehicle()) return { CMD_ERROR, INVALID_VEHICLE };
	Vehicle *v_front = v;
	Vehicle *w = nullptr;
	Vehicle *w_front = nullptr;
	Vehicle *w_rear = nullptr;

	/*
	 * v_front is the front engine in the original vehicle
	 * v is the car/vehicle of the original vehicle that is currently being copied
	 * w_front is the front engine of the cloned vehicle
	 * w is the car/vehicle currently being cloned
	 * w_rear is the rear end of the cloned train. It's used to add more cars and is only used by trains
	 */

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return { ret, INVALID_VEHICLE };

	if (v->type == VEH_TRAIN && (!v->IsFrontEngine() || Train::From(v)->crash_anim_pos >= 4400)) return { CMD_ERROR, INVALID_VEHICLE };

	/* check that we can allocate enough vehicles */
	if (!(flags & DC_EXEC)) {
		int veh_counter = 0;
		do {
			veh_counter++;
		} while ((v = v->Next()) != nullptr);

		if (!Vehicle::CanAllocateItem(veh_counter)) {
			return { CommandCost(STR_ERROR_TOO_MANY_VEHICLES_IN_GAME), INVALID_VEHICLE };
		}
	}

	v = v_front;

	VehicleID new_veh_id = INVALID_VEHICLE;
	do {
		if (v->type == VEH_TRAIN && Train::From(v)->IsRearDualheaded()) {
			/* we build the rear ends of multiheaded trains with the front ones */
			continue;
		}

		/* In case we're building a multi headed vehicle and the maximum number of
		 * vehicles is almost reached (e.g. max trains - 1) not all vehicles would
		 * be cloned. When the non-primary engines were build they were seen as
		 * 'new' vehicles whereas they would immediately be joined with a primary
		 * engine. This caused the vehicle to be not build as 'the limit' had been
		 * reached, resulting in partially build vehicles and such. */
		DoCommandFlag build_flags = flags;
		if ((flags & DC_EXEC) && !v->IsPrimaryVehicle()) build_flags |= DC_AUTOREPLACE;

		CommandCost cost;
		std::tie(cost, new_veh_id, std::ignore, std::ignore, std::ignore) = Command<CMD_BUILD_VEHICLE>::Do(build_flags, tile, v->engine_type, false, CT_INVALID, INVALID_CLIENT_ID);

		if (cost.Failed()) {
			/* Can't build a part, then sell the stuff we already made; clear up the mess */
			if (w_front != nullptr) Command<CMD_SELL_VEHICLE>::Do(flags, w_front->index, true, false, INVALID_CLIENT_ID);
			return { cost, INVALID_VEHICLE };
		}

		total_cost.AddCost(cost);

		if (flags & DC_EXEC) {
			w = Vehicle::Get(new_veh_id);

			if (v->type == VEH_TRAIN && HasBit(Train::From(v)->flags, VRF_REVERSE_DIRECTION)) {
				SetBit(Train::From(w)->flags, VRF_REVERSE_DIRECTION);
			}

			if (v->type == VEH_TRAIN && !v->IsFrontEngine()) {
				/* this s a train car
				 * add this unit to the end of the train */
				CommandCost result = Command<CMD_MOVE_RAIL_VEHICLE>::Do(flags, w->index, w_rear->index, true);
				if (result.Failed()) {
					/* The train can't be joined to make the same consist as the original.
					 * Sell what we already made (clean up) and return an error.           */
					Command<CMD_SELL_VEHICLE>::Do(flags, w_front->index, true, false, INVALID_CLIENT_ID);
					Command<CMD_SELL_VEHICLE>::Do(flags, w->index,       true, false, INVALID_CLIENT_ID);
					return { result, INVALID_VEHICLE }; // return error and the message returned from CMD_MOVE_RAIL_VEHICLE
				}
			} else {
				/* this is a front engine or not a train. */
				w_front = w;
				w->service_interval = v->service_interval;
				w->SetServiceIntervalIsCustom(v->ServiceIntervalIsCustom());
				w->SetServiceIntervalIsPercent(v->ServiceIntervalIsPercent());
			}
			w_rear = w; // trains needs to know the last car in the train, so they can add more in next loop
		}
	} while (v->type == VEH_TRAIN && (v = v->GetNextVehicle()) != nullptr);

	if ((flags & DC_EXEC) && v_front->type == VEH_TRAIN) {
		/* for trains this needs to be the front engine due to the callback function */
		new_veh_id = w_front->index;
	}

	if (flags & DC_EXEC) {
		/* Cloned vehicles belong to the same group */
		Command<CMD_ADD_VEHICLE_GROUP>::Do(flags, v_front->group_id, w_front->index, false, VehicleListIdentifier{});
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
				assert(w != nullptr);

				/* Find out what's the best sub type */
				byte subtype = GetBestFittingSubType(v, w, v->cargo_type);
				if (w->cargo_type != v->cargo_type || w->cargo_subtype != subtype) {
					CommandCost cost = std::get<0>(Command<CMD_REFIT_VEHICLE>::Do(flags, w->index, v->cargo_type, subtype, false, true, 0));
					if (cost.Succeeded()) total_cost.AddCost(cost);
				}

				if (w->IsGroundVehicle() && w->HasArticulatedPart()) {
					w = w->GetNextArticulatedPart();
				} else {
					break;
				}
			} else {
				const Engine *e = v->GetEngine();
				CargoID initial_cargo = (e->CanCarryCargo() ? e->GetDefaultCargoType() : (CargoID)CT_INVALID);

				if (v->cargo_type != initial_cargo && IsValidCargoID(initial_cargo)) {
					bool dummy;
					total_cost.AddCost(GetRefitCost(nullptr, v->engine_type, v->cargo_type, v->cargo_subtype, &dummy));
				}
			}

			if (v->IsGroundVehicle() && v->HasArticulatedPart()) {
				v = v->GetNextArticulatedPart();
			} else {
				break;
			}
		} while (v != nullptr);

		if ((flags & DC_EXEC) && v->type == VEH_TRAIN) w = w->GetNextVehicle();
	} while (v->type == VEH_TRAIN && (v = v->GetNextVehicle()) != nullptr);

	if (flags & DC_EXEC) {
		/*
		 * Set the orders of the vehicle. Cannot do it earlier as we need
		 * the vehicle refitted before doing this, otherwise the moved
		 * cargo types might not match (passenger vs non-passenger)
		 */
		CommandCost result = Command<CMD_CLONE_ORDER>::Do(flags, (share_orders ? CO_SHARE : CO_COPY), w_front->index, v_front->index);
		if (result.Failed()) {
			/* The vehicle has already been bought, so now it must be sold again. */
			Command<CMD_SELL_VEHICLE>::Do(flags, w_front->index, true, false, INVALID_CLIENT_ID);
			return { result, INVALID_VEHICLE };
		}

		/* Now clone the vehicle's name, if it has one. */
		if (!v_front->name.empty()) CloneVehicleName(v_front, w_front);

		/* Since we can't estimate the cost of cloning a vehicle accurately we must
		 * check whether the company has enough money manually. */
		if (!CheckCompanyHasMoney(total_cost)) {
			/* The vehicle has already been bought, so now it must be sold again. */
			Command<CMD_SELL_VEHICLE>::Do(flags, w_front->index, true, false, INVALID_CLIENT_ID);
			return { total_cost, INVALID_VEHICLE };
		}
	}

	return { total_cost, new_veh_id };
}

/**
 * Send all vehicles of type to depots
 * @param flags   the flags used for DoCommand()
 * @param service should the vehicles only get service in the depots
 * @param vli     identifier of the vehicle list
 * @return 0 for success and CMD_ERROR if no vehicle is able to go to depot
 */
static CommandCost SendAllVehiclesToDepot(DoCommandFlag flags, bool service, const VehicleListIdentifier &vli)
{
	VehicleList list;

	if (!GenerateVehicleSortList(&list, vli)) return CMD_ERROR;

	/* Send all the vehicles to a depot */
	bool had_success = false;
	for (uint i = 0; i < list.size(); i++) {
		const Vehicle *v = list[i];
		CommandCost ret = Command<CMD_SEND_VEHICLE_TO_DEPOT>::Do(flags, v->index, (service ? DepotCommand::Service : DepotCommand::None) | DepotCommand::DontCancel, {});

		if (ret.Succeeded()) {
			had_success = true;

			/* Return 0 if DC_EXEC is not set this is a valid goto depot command)
			 * In this case we know that at least one vehicle can be sent to a depot
			 * and we will issue the command. We can now safely quit the loop, knowing
			 * it will succeed at least once. With DC_EXEC we really need to send them to the depot */
			if (!(flags & DC_EXEC)) break;
		}
	}

	return had_success ? CommandCost() : CMD_ERROR;
}

/**
 * Send a vehicle to the depot.
 * @param flags for command type
 * @param veh_id vehicle ID to send to the depot
 * @param depot_cmd DEPOT_ flags (see vehicle_type.h)
 * @param vli VehicleListIdentifier.
 * @return the cost of this operation or an error
 */
CommandCost CmdSendVehicleToDepot(DoCommandFlag flags, VehicleID veh_id, DepotCommand depot_cmd, const VehicleListIdentifier &vli)
{
	if ((depot_cmd & DepotCommand::MassSend) != DepotCommand::None) {
		/* Mass goto depot requested */
		if (!vli.Valid()) return CMD_ERROR;
		return SendAllVehiclesToDepot(flags, (depot_cmd & DepotCommand::Service) != DepotCommand::None, vli);
	}

	Vehicle *v = Vehicle::GetIfValid(veh_id);
	if (v == nullptr) return CMD_ERROR;
	if (!v->IsPrimaryVehicle()) return CMD_ERROR;

	return v->SendToDepot(flags, depot_cmd);
}

/**
 * Give a custom name to your vehicle
 * @param flags type of operation
 * @param veh_id vehicle ID to name
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameVehicle(DoCommandFlag flags, VehicleID veh_id, const std::string &text)
{
	Vehicle *v = Vehicle::GetIfValid(veh_id);
	if (v == nullptr || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	bool reset = text.empty();

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_VEHICLE_NAME_CHARS) return CMD_ERROR;
		if (!(flags & DC_AUTOREPLACE) && !IsUniqueVehicleName(text)) return_cmd_error(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		if (reset) {
			v->name.clear();
		} else {
			v->name = text;
		}
		InvalidateWindowClassesData(GetWindowClassForVehicleType(v->type), 1);
		MarkWholeScreenDirty();
	}

	return CommandCost();
}


/**
 * Change the service interval of a vehicle
 * @param flags type of operation
 * @param veh_id vehicle ID that is being service-interval-changed
 * @param serv_int new service interval
 * @param is_custom service interval is custom flag
 * @param is_percent service interval is percentage flag
 * @return the cost of this operation or an error
 */
CommandCost CmdChangeServiceInt(DoCommandFlag flags, VehicleID veh_id, uint16_t serv_int, bool is_custom, bool is_percent)
{
	Vehicle *v = Vehicle::GetIfValid(veh_id);
	if (v == nullptr || !v->IsPrimaryVehicle()) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	const Company *company = Company::Get(v->owner);
	is_percent = is_custom ? is_percent : company->settings.vehicle.servint_ispercent;

	if (is_custom) {
		if (serv_int != GetServiceIntervalClamped(serv_int, is_percent)) return CMD_ERROR;
	} else {
		serv_int = CompanyServiceInterval(company, v->type);
	}

	if (flags & DC_EXEC) {
		v->SetServiceInterval(serv_int);
		v->SetServiceIntervalIsCustom(is_custom);
		v->SetServiceIntervalIsPercent(is_percent);
		SetWindowDirty(WC_VEHICLE_DETAILS, v->index);
	}

	return CommandCost();
}
