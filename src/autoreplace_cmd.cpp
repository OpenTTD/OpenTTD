/* $Id$ */

/** @file autoreplace_cmd.cpp Deals with autoreplace execution but not the setup */

#include "stdafx.h"
#include "openttd.h"
#include "roadveh.h"
#include "ship.h"
#include "news_func.h"
#include "player_func.h"
#include "debug.h"
#include "vehicle_gui.h"
#include "train.h"
#include "aircraft.h"
#include "cargotype.h"
#include "group.h"
#include "strings_func.h"
#include "command_func.h"
#include "vehicle_func.h"
#include "functions.h"
#include "variables.h"
#include "autoreplace_func.h"
#include "articulated_vehicles.h"

#include "table/strings.h"

/** Figure out if two engines got at least one type of cargo in common (refitting if needed)
 * @param engine_a one of the EngineIDs
 * @param engine_b the other EngineID
 * @param type the type of the engines
 * @return true if they can both carry the same type of cargo (or at least one of them got no capacity at all)
 */
static bool EnginesGotCargoInCommon(EngineID engine_a, EngineID engine_b, VehicleType type)
{
	uint32 available_cargos_a = GetUnionOfArticulatedRefitMasks(engine_a, type, true);
	uint32 available_cargos_b = GetUnionOfArticulatedRefitMasks(engine_b, type, true);
	return (available_cargos_a == 0 || available_cargos_b == 0 || (available_cargos_a & available_cargos_b) != 0);
}

/**
 * Checks some basic properties whether autoreplace is allowed
 * @param from Origin engine
 * @param to Destination engine
 * @param player Player to check for
 * @return true if autoreplace is allowed
 */
bool CheckAutoreplaceValidity(EngineID from, EngineID to, PlayerID player)
{
	/* First we make sure that it's a valid type the user requested
	 * check that it's an engine that is in the engine array */
	if (!IsEngineIndex(from) || !IsEngineIndex(to)) return false;

	/* we can't replace an engine into itself (that would be autorenew) */
	if (from == to) return false;

	VehicleType type = GetEngine(from)->type;

	/* check that the new vehicle type is available to the player and its type is the same as the original one */
	if (!IsEngineBuildable(to, type, player)) return false;

	switch (type) {
		case VEH_TRAIN: {
			const RailVehicleInfo *rvi_from = RailVehInfo(from);
			const RailVehicleInfo *rvi_to   = RailVehInfo(to);

			/* make sure the railtypes are compatible */
			if ((GetRailTypeInfo(rvi_from->railtype)->compatible_railtypes & GetRailTypeInfo(rvi_to->railtype)->compatible_railtypes) == 0) return false;

			/* make sure we do not replace wagons with engines or vise versa */
			if ((rvi_from->railveh_type == RAILVEH_WAGON) != (rvi_to->railveh_type == RAILVEH_WAGON)) return false;
			break;
		}

		case VEH_ROAD:
			/* make sure that we do not replace a tram with a normal road vehicles or vise versa */
			if (HasBit(EngInfo(from)->misc_flags, EF_ROAD_TRAM) != HasBit(EngInfo(to)->misc_flags, EF_ROAD_TRAM)) return false;
			break;

		case VEH_AIRCRAFT:
			/* make sure that we do not replace a plane with a helicopter or vise versa */
			if ((AircraftVehInfo(from)->subtype & AIR_CTOL) != (AircraftVehInfo(to)->subtype & AIR_CTOL)) return false;
			break;

		default: break;
	}

	/* the engines needs to be able to carry the same cargo */
	return EnginesGotCargoInCommon(from, to, type);
}

/*
 * move the cargo from one engine to another if possible
 */
static void MoveVehicleCargo(Vehicle *dest, Vehicle *source)
{
	Vehicle *v = dest;

	do {
		do {
			if (source->cargo_type != dest->cargo_type)
				continue; // cargo not compatible

			if (dest->cargo.Count() == dest->cargo_cap)
				continue; // the destination vehicle is already full

			uint units_moved = min(source->cargo.Count(), dest->cargo_cap - dest->cargo.Count());
			source->cargo.MoveTo(&dest->cargo, units_moved);

			// copy the age of the cargo
			dest->day_counter  = source->day_counter;
			dest->tick_counter = source->tick_counter;

		} while (source->cargo.Count() > 0 && (dest = dest->Next()) != NULL);
		dest = v;
	} while ((source = source->Next()) != NULL);

	/*
	 * The of the train will be incorrect at this moment. This is due
	 * to the fact that removing the old wagon updates the weight of
	 * the complete train, which is without the weight of cargo we just
	 * moved back into some (of the) new wagon(s).
	 */
	if (dest->type == VEH_TRAIN) TrainConsistChanged(dest->First(), true);
}


/** Transfer cargo from a single (articulated )old vehicle to the new vehicle chain
 * @param old_veh Old vehicle that will be sold
 * @param new_head Head of the completely constructed new vehicle chain
 */
static void TransferCargo(Vehicle *old_veh, Vehicle *new_head)
{
	/* Loop through source parts */
	for (Vehicle *src = old_veh; src != NULL; src = src->Next()) {
		if (src->cargo_type >= NUM_CARGO || src->cargo.Count() == 0) continue;

		/* Find free space in the new chain */
		for (Vehicle *dest = new_head; dest != NULL && src->cargo.Count() > 0; dest = dest->Next()) {
			if (dest->cargo_type != src->cargo_type) continue;

			uint amount = min(src->cargo.Count(), dest->cargo_cap - dest->cargo.Count());
			if (amount <= 0) continue;

			src->cargo.MoveTo(&dest->cargo, amount);
		}
	}

	/* Update train weight etc., the old vehicle will be sold anyway */
	if (new_head->type == VEH_TRAIN) TrainConsistChanged(new_head, true);
}

/**
 * Tests whether refit orders that applied to v will also apply to the new vehicle type
 * @param v The vehicle to be replaced
 * @param engine_type The type we want to replace with
 * @return true iff all refit orders stay valid
 */
static bool VerifyAutoreplaceRefitForOrders(const Vehicle *v, EngineID engine_type)
{
	const Order *o;
	const Vehicle *u;

	uint32 union_refit_mask_a = GetUnionOfArticulatedRefitMasks(v->engine_type, v->type, false);
	uint32 union_refit_mask_b = GetUnionOfArticulatedRefitMasks(engine_type, v->type, false);

	if (v->type == VEH_TRAIN) {
		u = v->First();
	} else {
		u = v;
	}

	FOR_VEHICLE_ORDERS(u, o) {
		if (!o->IsRefit()) continue;
		CargoID cargo_type = o->GetRefitCargo();

		if (!HasBit(union_refit_mask_a, cargo_type)) continue;
		if (!HasBit(union_refit_mask_b, cargo_type)) return false;
	}

	return true;
}

/**
 * Function to find what type of cargo to refit to when autoreplacing
 * @param *v Original vehicle, that is being replaced
 * @param engine_type The EngineID of the vehicle that is being replaced to
 * @return The cargo type to replace to
 *    CT_NO_REFIT is returned if no refit is needed
 *    CT_INVALID is returned when both old and new vehicle got cargo capacity and refitting the new one to the old one's cargo type isn't possible
 */
static CargoID GetNewCargoTypeForReplace(Vehicle *v, EngineID engine_type)
{
	CargoID cargo_type;

	if (GetUnionOfArticulatedRefitMasks(engine_type, v->type, true) == 0) return CT_NO_REFIT; // Don't try to refit an engine with no cargo capacity

	if (IsArticulatedVehicleCarryingDifferentCargos(v, &cargo_type)) return CT_INVALID; // We cannot refit to mixed cargos in an automated way

	uint32 available_cargo_types = GetIntersectionOfArticulatedRefitMasks(engine_type, v->type, true);

	if (cargo_type == CT_INVALID) {
		if (v->type != VEH_TRAIN) return CT_NO_REFIT; // If the vehicle does not carry anything at all, every replacement is fine.

		/* the old engine didn't have cargo capacity, but the new one does
		 * now we will figure out what cargo the train is carrying and refit to fit this */

		for (v = v->First(); v != NULL; v = v->Next()) {
			if (v->cargo_cap == 0) continue;
			/* Now we found a cargo type being carried on the train and we will see if it is possible to carry to this one */
			if (HasBit(available_cargo_types, v->cargo_type)) {
				/* Do we have to refit the vehicle, or is it already carrying the right cargo? */
				uint16 *default_capacity = GetCapacityOfArticulatedParts(engine_type, v->type);
				for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
					if (cid != v->cargo_type && default_capacity[cid] > 0) return v->cargo_type;
				}

				return CT_NO_REFIT;
			}
		}

		return CT_NO_REFIT; // We failed to find a cargo type on the old vehicle and we will not refit the new one
	} else {
		if (!HasBit(available_cargo_types, cargo_type)) return CT_INVALID; // We can't refit the vehicle to carry the cargo we want

		if (!VerifyAutoreplaceRefitForOrders(v, engine_type)) return CT_INVALID; // Some refit orders loose their effect

		/* Do we have to refit the vehicle, or is it already carrying the right cargo? */
		uint16 *default_capacity = GetCapacityOfArticulatedParts(engine_type, v->type);
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			if (cid != cargo_type && default_capacity[cid] > 0) return cargo_type;
		}

		return CT_NO_REFIT;
	}
}

/** Replaces a vehicle (used to be called autorenew)
 * This function is only called from MaybeReplaceVehicle()
 * Must be called with _current_player set to the owner of the vehicle
 * @param w Vehicle to replace
 * @param flags is the flags to use when calling DoCommand(). Mainly DC_EXEC counts
 * @param p The vehicle owner (faster than refinding the pointer)
 * @param new_engine_type The EngineID to replace to
 * @return value is cost of the replacement or CMD_ERROR
 */
static CommandCost ReplaceVehicle(Vehicle **w, uint32 flags, Money total_cost, const Player *p, EngineID new_engine_type)
{
	CommandCost cost;
	CommandCost sell_value;
	Vehicle *old_v = *w;
	const UnitID cached_unitnumber = old_v->unitnumber;
	bool new_front = false;
	Vehicle *new_v = NULL;
	char *vehicle_name = NULL;
	CargoID replacement_cargo_type;

	replacement_cargo_type = GetNewCargoTypeForReplace(old_v, new_engine_type);

	/* check if we can't refit to the needed type, so no replace takes place to prevent the vehicle from altering cargo type */
	if (replacement_cargo_type == CT_INVALID) return CommandCost();

	sell_value = DoCommand(0, old_v->index, 0, DC_QUERY_COST, GetCmdSellVeh(old_v));

	/* We give the player a loan of the same amount as the sell value.
	 * This is needed in case he needs the income from the sale to build the new vehicle.
	 * We take it back if building fails or when we really sell the old engine */
	SubtractMoneyFromPlayer(sell_value);

	cost = DoCommand(old_v->tile, new_engine_type, 0, flags | DC_AUTOREPLACE, GetCmdBuildVeh(old_v));
	if (CmdFailed(cost)) {
		/* Take back the money we just gave the player */
		sell_value.MultiplyCost(-1);
		SubtractMoneyFromPlayer(sell_value);
		return cost;
	}

	if (replacement_cargo_type != CT_NO_REFIT) {
		/* add refit cost */
		CommandCost refit_cost = GetRefitCost(new_engine_type);
		if (old_v->type == VEH_TRAIN && RailVehInfo(new_engine_type)->railveh_type == RAILVEH_MULTIHEAD) {
			/* Since it's a dualheaded engine we have to pay once more because the rear end is being refitted too. */
			refit_cost.AddCost(refit_cost);
		}
		cost.AddCost(refit_cost);
	}

	if (flags & DC_EXEC) {
		new_v = GetVehicle(_new_vehicle_id);
		*w = new_v; //we changed the vehicle, so MaybeReplaceVehicle needs to work on the new one. Now we tell it what the new one is

		/* refit if needed */
		if (replacement_cargo_type != CT_NO_REFIT) {
			if (CmdFailed(DoCommand(0, new_v->index, replacement_cargo_type, DC_EXEC, GetCmdRefitVeh(new_v)))) {
				/* Being here shows a failure, which most likely is in GetNewCargoTypeForReplace() or incorrect estimation costs */
				error("Autoreplace failed to refit. Replace engine %d to %d and refit to cargo %d", old_v->engine_type, new_v->engine_type, replacement_cargo_type);
			}
		}

		if (new_v->type == VEH_TRAIN && HasBit(old_v->u.rail.flags, VRF_REVERSE_DIRECTION) && !IsMultiheaded(new_v) && !(new_v->Next() != NULL && IsArticulatedPart(new_v->Next()))) {
			// we are autorenewing to a single engine, so we will turn it as the old one was turned as well
			SetBit(new_v->u.rail.flags, VRF_REVERSE_DIRECTION);
		}

		if (old_v->type == VEH_TRAIN && !IsFrontEngine(old_v)) {
			/* this is a railcar. We need to move the car into the train
			 * We add the new engine after the old one instead of replacing it. It will give the same result anyway when we
			 * sell the old engine in a moment
			 */
			/* Get the vehicle in front of the one we move out */
			Vehicle *front = old_v->Previous();
			if (front == NULL) {
				/* It would appear that we have the front wagon of a row of wagons without engines */
				Vehicle *next = old_v->Next();
				if (next != NULL) {
					/* Move the chain to the new front wagon */
					DoCommand(0, (new_v->index << 16) | next->index, 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
				}
			} else {
				/* If the vehicle in front is the rear end of a dualheaded engine, then we need to use the one in front of that one */
				if (IsRearDualheaded(front)) front = front->Previous();
				/* Now we move the old one out of the train */
				DoCommand(0, (INVALID_VEHICLE << 16) | old_v->index, 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
				/* Add the new vehicle */
				CommandCost tmp_move = DoCommand(0, (front->index << 16) | new_v->index, 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
				if (CmdFailed(tmp_move)) {
					cost.AddCost(tmp_move);
					DoCommand(0, new_v->index, 1, DC_EXEC, GetCmdSellVeh(VEH_TRAIN));
				}
			}
		} else {
			// copy/clone the orders
			DoCommand(0, (old_v->index << 16) | new_v->index, old_v->IsOrderListShared() ? CO_SHARE : CO_COPY, DC_EXEC, CMD_CLONE_ORDER);
			new_v->cur_order_index = old_v->cur_order_index;
			ChangeVehicleViewWindow(old_v->index, new_v->index);
			new_v->profit_this_year = old_v->profit_this_year;
			new_v->profit_last_year = old_v->profit_last_year;
			new_v->service_interval = old_v->service_interval;
			DoCommand(0, old_v->group_id, new_v->index, flags, CMD_ADD_VEHICLE_GROUP);
			new_front = true;
			new_v->unitnumber = old_v->unitnumber; // use the same unit number
			new_v->dest_tile  = old_v->dest_tile;

			new_v->current_order = old_v->current_order;
			if (old_v->type == VEH_TRAIN && GetNextVehicle(old_v) != NULL){
				Vehicle *temp_v = GetNextVehicle(old_v);

				// move the entire train to the new engine, excluding the old engine
				if (IsMultiheaded(old_v) && temp_v == old_v->u.rail.other_multiheaded_part) {
					// we got front and rear of a multiheaded engine right after each other. We should work with the next in line instead
					temp_v = GetNextVehicle(temp_v);
				}

				if (temp_v != NULL) {
					CommandCost tmp_move = DoCommand(0, (new_v->index << 16) | temp_v->index, 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
					if (CmdFailed(tmp_move)) {
						cost.AddCost(tmp_move);
						DoCommand(0, temp_v->index, 1, DC_EXEC, GetCmdSellVeh(VEH_TRAIN));
					}
				}
			}
		}
		if (CmdSucceeded(cost)) {
			/* We are done setting up the new vehicle. Now we move the cargo from the old one to the new one */
			MoveVehicleCargo(new_v->type == VEH_TRAIN ? new_v->First() : new_v, old_v);

			/* Get the name of the old vehicle if it has a custom name. */
			if (old_v->name != NULL) vehicle_name = strdup(old_v->name);
		}
	} else { // flags & DC_EXEC not set
		CommandCost tmp_move;

		if (old_v->type == VEH_TRAIN && IsFrontEngine(old_v)) {
			Vehicle *next_veh = GetNextUnit(old_v); // don't try to move the rear multiheaded engine or articulated parts
			if (next_veh != NULL) {
				/* Verify that the wagons can be placed on the engine in question.
				 * This is done by building an engine, test if the wagons can be added and then sell the test engine. */
				DoCommand(old_v->tile, new_engine_type, 0, DC_EXEC | DC_AUTOREPLACE, GetCmdBuildVeh(old_v));
				Vehicle *temp = GetVehicle(_new_vehicle_id);
				tmp_move = DoCommand(0, (temp->index << 16) | next_veh->index, 1, 0, CMD_MOVE_RAIL_VEHICLE);
				DoCommand(0, temp->index, 0, DC_EXEC, GetCmdSellVeh(old_v));
			}
		}

		/* Ensure that the player will not end up having negative money while autoreplacing
		 * This is needed because the only other check is done after the income from selling the old vehicle is substracted from the cost */
		if (CmdFailed(tmp_move) || p->player_money < (cost.GetCost() + total_cost)) {
			/* Pay back the loan */
			sell_value.MultiplyCost(-1);
			SubtractMoneyFromPlayer(sell_value);
			return CMD_ERROR;
		}
	}

	/* Take back the money we just gave the player just before building the vehicle
	 * The player will get the same amount now that the sale actually takes place */
	sell_value.MultiplyCost(-1);
	SubtractMoneyFromPlayer(sell_value);

	/* sell the engine/ find out how much you get for the old engine (income is returned as negative cost) */
	cost.AddCost(DoCommand(0, old_v->index, 0, flags, GetCmdSellVeh(old_v)));

	if (CmdFailed(cost)) return cost;

	if (new_front) {
		/* now we assign the old unitnumber to the new vehicle */
		new_v->unitnumber = cached_unitnumber;
	}

	/* Transfer the name of the old vehicle */
	if ((flags & DC_EXEC) && vehicle_name != NULL) {
		_cmd_text = vehicle_name;
		DoCommand(0, new_v->index, 0, DC_EXEC, CMD_NAME_VEHICLE);
		free(vehicle_name);
	}

	return cost;
}

/** Removes wagons from a train until it get a certain length
 * @param v The vehicle
 * @param old_total_length The wanted max length
 * @return The profit from selling the wagons
 */
static CommandCost WagonRemoval(Vehicle *v, uint16 old_total_length)
{
	if (v->type != VEH_TRAIN) return CommandCost();
	Vehicle *front = v;

	CommandCost cost = CommandCost();

	while (front->u.rail.cached_total_length > old_total_length) {
		/* the train is too long. We will remove cars one by one from the start of the train until it's short enough */
		while (v != NULL && RailVehInfo(v->engine_type)->railveh_type != RAILVEH_WAGON) {
			/* We move backwards in the train until we find a wagon */
			v = GetNextVehicle(v);
		}

		if (v == NULL) {
			/* We sold all the wagons and the train is still not short enough */
			SetDParam(0, front->unitnumber);
			AddNewsItem(STR_TRAIN_TOO_LONG_AFTER_REPLACEMENT, NS_ADVICE, front->index, 0);
			return cost;
		}

		/* We found a wagon we can sell */
		Vehicle *temp = v;
		v = GetNextVehicle(v);
		DoCommand(0, (INVALID_VEHICLE << 16) | temp->index, 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE); // remove the wagon from the train
		MoveVehicleCargo(front, temp); // move the cargo back on the train
		cost.AddCost(DoCommand(0, temp->index, 0, DC_EXEC, CMD_SELL_RAIL_WAGON)); // sell the wagon
	}
	return cost;
}

/** Get the EngineID of the replacement for a vehicle
 * @param v The vehicle to find a replacement for
 * @param p The vehicle's owner (it's faster to forward the pointer than refinding it)
 * @return the EngineID of the replacement. INVALID_ENGINE if no buildable replacement is found
 */
static EngineID GetNewEngineType(const Vehicle *v, const Player *p)
{
	assert(v->type != VEH_TRAIN || !IsArticulatedPart(v));

	if (v->type == VEH_TRAIN && IsRearDualheaded(v)) {
		/* we build the rear ends of multiheaded trains with the front ones */
		return INVALID_ENGINE;
	}

	EngineID e = EngineReplacementForPlayer(p, v->engine_type, v->group_id);

	if (e != INVALID_ENGINE && IsEngineBuildable(e, v->type, _current_player)) {
		return e;
	}

	if (v->NeedsAutorenewing(p) && // replace if engine is too old
	    IsEngineBuildable(v->engine_type, v->type, _current_player)) { // engine can still be build
		return v->engine_type;
	}

	return INVALID_ENGINE;
}

/** replaces a vehicle if it's set for autoreplace or is too old
 * (used to be called autorenew)
 * @param v The vehicle to replace
 * if the vehicle is a train, v needs to be the front engine
 * @param flags
 * @param display_costs If set, a cost animation is shown (only if DC_EXEC is set)
 *        This bool also takes autorenew money into consideration
 * @return the costs, the success bool and sometimes an error message
 */
CommandCost MaybeReplaceVehicle(Vehicle *v, uint32 flags, bool display_costs)
{
	Vehicle *w;
	Player *p = GetPlayer(v->owner);
	CommandCost cost;
	bool stopped = false;
	BackuppedVehicle backup(true);

	/* We only want "real" vehicle types. */
	assert(IsPlayerBuildableVehicleType(v));

	/* Ensure that this bool is cleared. */
	assert(!v->leave_depot_instantly);

	/* We can't sell if the current player don't own the vehicle. */
	assert(v->owner == _current_player);

	if (!v->IsInDepot()) {
		/* The vehicle should be inside the depot */
		switch (v->type) {
			default: NOT_REACHED();
			case VEH_TRAIN:    return_cmd_error(STR_881A_TRAINS_CAN_ONLY_BE_ALTERED); break;
			case VEH_ROAD:     return_cmd_error(STR_9013_MUST_BE_STOPPED_INSIDE);     break;
			case VEH_SHIP:     return_cmd_error(STR_980B_SHIP_MUST_BE_STOPPED_IN);    break;
			case VEH_AIRCRAFT: return_cmd_error(STR_A01B_AIRCRAFT_MUST_BE_STOPPED);   break;
		}
	}

	/* Remember the length in case we need to trim train later on
	 * If it's not a train, the value is unused
	 * round up to the length of the tiles used for the train instead of the train length instead
	 * Useful when newGRF uses custom length */
	uint16 old_total_length = (v->type == VEH_TRAIN ?
		(v->u.rail.cached_total_length + TILE_SIZE - 1) / TILE_SIZE * TILE_SIZE :
		-1
	);

	if (!(v->vehstatus & VS_STOPPED)) {
		/* The vehicle is moving so we better stop it before we might alter consist or sell it */
		v->vehstatus |= VS_STOPPED;
		/* Remember that we stopped the vehicle */
		stopped = true;
	}

	{
		cost = CommandCost(EXPENSES_NEW_VEHICLES);
		w = v;
		do {
			EngineID new_engine = GetNewEngineType(w, p);
			if (new_engine == INVALID_ENGINE) continue;

			if (!backup.ContainsBackup()) {
				/* We are going to try to replace a vehicle but we don't have any backup so we will make one. */
				backup.Backup(v, p);
			}
			/* Now replace the vehicle.
			 * First we need to cache if it's the front vehicle as we need to update the v pointer if it is.
			 * If the replacement fails then we can't trust the data in the vehicle hence the reason to cache the result. */
			bool IsFront = w->type != VEH_TRAIN || w->u.rail.first_engine == INVALID_ENGINE;

			cost.AddCost(ReplaceVehicle(&w, DC_EXEC, cost.GetCost(), p, new_engine));

			if (IsFront) {
				/* now we bought a new engine and sold the old one. We need to fix the
				 * pointers in order to avoid pointing to the old one for trains: these
				 * pointers should point to the front engine and not the cars
				 */
				v = w;
			}
		} while (CmdSucceeded(cost) && w->type == VEH_TRAIN && (w = GetNextVehicle(w)) != NULL);

		if (CmdSucceeded(cost) && v->type == VEH_TRAIN && p->renew_keep_length) {
			/* Remove wagons until the wanted length is reached */
			cost.AddCost(WagonRemoval(v, old_total_length));
		}

		if (flags & DC_QUERY_COST || cost.GetCost() == 0) {
			/* We didn't do anything during the replace so we will just exit here */
			v = backup.Restore(v, p);
			if (stopped) v->vehstatus &= ~VS_STOPPED;
			return cost;
		}

		if (display_costs) {
			/* We want to ensure that we will not get below p->engine_renew_money.
			 * We will not actually pay this amount. It's for display and checks only. */
			CommandCost tmp = cost;
			tmp.AddCost((Money)p->engine_renew_money);
			if (CmdSucceeded(tmp) && GetAvailableMoneyForCommand() < tmp.GetCost()) {
				/* We don't have enough money so we will set cost to failed */
				cost.AddCost((Money)p->engine_renew_money);
				cost.AddCost(CMD_ERROR);
			}
		}

		if (display_costs && CmdFailed(cost)) {
			if (GetAvailableMoneyForCommand() < cost.GetCost() && IsLocalPlayer()) {
				StringID message;
				SetDParam(0, v->unitnumber);
				switch (v->type) {
					case VEH_TRAIN:    message = STR_TRAIN_AUTORENEW_FAILED;       break;
					case VEH_ROAD:     message = STR_ROADVEHICLE_AUTORENEW_FAILED; break;
					case VEH_SHIP:     message = STR_SHIP_AUTORENEW_FAILED;        break;
					case VEH_AIRCRAFT: message = STR_AIRCRAFT_AUTORENEW_FAILED;    break;
						// This should never happen
					default: NOT_REACHED(); message = 0; break;
				}

				AddNewsItem(message, NS_ADVICE, v->index, 0);
			}
		}
	}

	if (display_costs && IsLocalPlayer() && (flags & DC_EXEC) && CmdSucceeded(cost)) {
		ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, cost.GetCost());
	}

	if (!(flags & DC_EXEC) || CmdFailed(cost)) {
		v = backup.Restore(v, p);
	}

	/* Start the vehicle if we stopped it earlier */
	if (stopped) v->vehstatus &= ~VS_STOPPED;

	return cost;
}

/** Builds and refits a replacement vehicle
 * Important: The old vehicle is still in the original vehicle chain (used for determining the cargo when the old vehicle did not carry anything, but the new one does)
 * @param old_veh A single (articulated/multiheaded) vehicle that shall be replaced.
 * @param new_vehicle Returns the newly build and refittet vehicle
 * @return cost or error
 */
static CommandCost BuildReplacementVehicle(Vehicle *old_veh, Vehicle **new_vehicle)
{
	*new_vehicle = NULL;

	/* Shall the vehicle be replaced? */
	const Player *p = GetPlayer(_current_player);
	EngineID e = GetNewEngineType(old_veh, p);
	if (e == INVALID_ENGINE) return CommandCost(); // neither autoreplace is set, nor autorenew is triggered

	/* Does it need to be refitted */
	CargoID refit_cargo = GetNewCargoTypeForReplace(old_veh, e);
	if (refit_cargo == CT_INVALID) return CommandCost(); // incompatible cargos

	/* Build the new vehicle */
	CommandCost cost = DoCommand(old_veh->tile, e, 0, DC_EXEC | DC_AUTOREPLACE, GetCmdBuildVeh(old_veh));
	if (cost.Failed()) return cost;

	Vehicle *new_veh = GetVehicle(_new_vehicle_id);
	*new_vehicle = new_veh;

	/* Refit the vehicle if needed */
	if (refit_cargo != CT_NO_REFIT) {
		cost.AddCost(DoCommand(0, new_veh->index, refit_cargo, DC_EXEC, GetCmdRefitVeh(new_veh)));
		assert(cost.Succeeded()); // This should be ensured by GetNewCargoTypeForReplace()
	}

	/* Try to reverse the vehicle, but do not care if it fails as the new type might not be reversible */
	if (new_veh->type == VEH_TRAIN && HasBit(old_veh->u.rail.flags, VRF_REVERSE_DIRECTION)) {
		DoCommand(0, new_veh->index, true, DC_EXEC, CMD_REVERSE_TRAIN_DIRECTION);
	}

	return cost;
}

/** Issue a start/stop command
 * @param v a vehicle
 * @param evaluate_callback shall the start/stop callback be evaluated?
 * @return success or error
 */
static inline CommandCost StartStopVehicle(const Vehicle *v, bool evaluate_callback)
{
	return DoCommand(0, v->index, evaluate_callback ? 1 : 0, DC_EXEC | DC_AUTOREPLACE, CMD_START_STOP_VEHICLE);
}

/** Issue a train vehicle move command
 * @param v The vehicle to move
 * @param after The vehicle to insert 'v' after, or NULL to start new chain
 * @param whole_chain move all vehicles following 'v' (true), or only 'v' (false)
 * @return success or error
 */
static inline CommandCost MoveVehicle(const Vehicle *v, const Vehicle *after, uint32 flags, bool whole_chain)
{
	return DoCommand(0, v->index | (after != NULL ? after->index : INVALID_VEHICLE) << 16, whole_chain ? 1 : 0, flags, CMD_MOVE_RAIL_VEHICLE);
}

/** Copy head specific things to the new vehicle chain after it was successfully constructed
 * @param old_head The old front vehicle (no wagons attached anymore)
 * @param new_head The new head of the completely replaced vehicle chain
 * @param flags the command flags to use
 */
static CommandCost CopyHeadSpecificThings(Vehicle *old_head, Vehicle *new_head, uint32 flags)
{
	CommandCost cost = CommandCost();

	/* Share orders */
	if (cost.Succeeded() && old_head != new_head) cost.AddCost(DoCommand(0, (old_head->index << 16) | new_head->index, CO_SHARE, DC_EXEC, CMD_CLONE_ORDER));

	/* Copy group membership */
	if (cost.Succeeded() && old_head != new_head) cost.AddCost(DoCommand(0, old_head->group_id, new_head->index, DC_EXEC, CMD_ADD_VEHICLE_GROUP));

	/* Perform start/stop check whether the new vehicle suits newgrf restrictions etc. */
	if (cost.Succeeded()) {
		/* Start the vehicle, might be denied by certain things */
		assert((new_head->vehstatus & VS_STOPPED) != 0);
		cost.AddCost(StartStopVehicle(new_head, true));

		/* Stop the vehicle again, but do not care about evil newgrfs allowing starting but not stopping :p */
		if (cost.Succeeded()) cost.AddCost(StartStopVehicle(new_head, false));
	}

	/* Last do those things which do never fail (resp. we do not care about), but which are not undo-able */
	if (cost.Succeeded() && old_head != new_head && (flags & DC_EXEC) != 0) {
		/* Copy vehicle name */
		if (old_head->name != NULL) {
			_cmd_text = old_head->name;
			DoCommand(0, new_head->index, 0, DC_EXEC | DC_AUTOREPLACE, CMD_NAME_VEHICLE);
			_cmd_text = NULL;
		}

		/* Copy other things which cannot be copied by a command and which shall not stay resetted from the build vehicle command */
		new_head->CopyVehicleConfigAndStatistics(old_head);

		/* Switch vehicle windows to the new vehicle, so they are not closed when the old vehicle is sold */
		ChangeVehicleViewWindow(old_head->index, new_head->index);
	}

	return cost;
}

/** Replace a whole vehicle chain
 * @param chain vehicle chain to let autoreplace/renew operator on
 * @param flags command flags
 * @param wagon_removal remove wagons when the resulting chain occupies more tiles than the old did
 * @param nothing_to_do is set to 'false' when something was done (only valid when not failed)
 * @return cost or error
 */
static CommandCost ReplaceChain(Vehicle **chain, uint32 flags, bool wagon_removal, bool *nothing_to_do)
{
	Vehicle *old_head = *chain;

	CommandCost cost = CommandCost(EXPENSES_NEW_VEHICLES, 0);

	if (old_head->type == VEH_TRAIN) {
		/* Store the length of the old vehicle chain, rounded up to whole tiles */
		uint16 old_total_length = (old_head->u.rail.cached_total_length + TILE_SIZE - 1) / TILE_SIZE * TILE_SIZE;

		int num_units = 0; ///< Number of units in the chain
		for (Vehicle *w = old_head; w != NULL; w = GetNextUnit(w)) num_units++;

		Vehicle **old_vehs = CallocT<Vehicle *>(num_units); ///< Will store vehicles of the old chain in their order
		Vehicle **new_vehs = CallocT<Vehicle *>(num_units); ///< New vehicles corresponding to old_vehs or NULL if no replacement
		Money *new_costs = MallocT<Money>(num_units);       ///< Costs for buying and refitting the new vehicles

		/* Collect vehicles and build replacements
		 * Note: The replacement vehicles can only successfully build as long as the old vehicles are still in their chain */
		int i;
		Vehicle *w;
		for (w = old_head, i = 0; w != NULL; w = GetNextUnit(w), i++) {
			assert(i < num_units);
			old_vehs[i] = w;

			CommandCost ret = BuildReplacementVehicle(old_vehs[i], &new_vehs[i]);
			cost.AddCost(ret);
			if (cost.Failed()) break;

			new_costs[i] = ret.GetCost();
			if (new_vehs[i] != NULL) *nothing_to_do = false;
		}
		Vehicle *new_head = (new_vehs[0] != NULL ? new_vehs[0] : old_vehs[0]);

		/* Separate the head, so we can start constructing the new chain */
		if (cost.Succeeded()) {
			Vehicle *second = GetNextUnit(old_head);
			if (second != NULL) cost.AddCost(MoveVehicle(second, NULL, DC_EXEC | DC_AUTOREPLACE, true));

			assert(GetNextUnit(new_head) == NULL);
		}

		/* Append engines to the new chain
		 * We do this from back to front, so that the head of the temporary vehicle chain does not change all the time.
		 * OTOH the vehicle attach callback is more expensive this way :s */
		Vehicle *last_engine = NULL; ///< Shall store the last engine unit after this step
		if (cost.Succeeded()) {
			for (int i = num_units - 1; i > 0; i--) {
				Vehicle *append = (new_vehs[i] != NULL ? new_vehs[i] : old_vehs[i]);

				if (RailVehInfo(append->engine_type)->railveh_type == RAILVEH_WAGON) continue;

				if (last_engine == NULL) last_engine = append;
				cost.AddCost(MoveVehicle(append, new_head, DC_EXEC, false));
				if (cost.Failed()) break;
			}
			if (last_engine == NULL) last_engine = new_head;
		}

		/* When wagon removal is enabled and the new engines without any wagons are already longer than the old, we have to fail */
		if (cost.Succeeded() && wagon_removal && new_head->u.rail.cached_total_length > old_total_length) cost = CommandCost(STR_TRAIN_TOO_LONG_AFTER_REPLACEMENT);

		/* Append/insert wagons into the new vehicle chain
		 * We do this from back to front, so we can stop when wagon removal or maximum train length (i.e. from mammoth-train setting) is triggered.
		 */
		if (cost.Succeeded()) {
			for (int i = num_units - 1; i > 0; i--) {
				assert(last_engine != NULL);
				Vehicle *append = (new_vehs[i] != NULL ? new_vehs[i] : old_vehs[i]);

				if (RailVehInfo(append->engine_type)->railveh_type == RAILVEH_WAGON) {
					/* Insert wagon after 'last_engine' */
					CommandCost res = MoveVehicle(append, last_engine, DC_EXEC, false);

					if (res.Succeeded() && wagon_removal && new_head->u.rail.cached_total_length > old_total_length) {
						MoveVehicle(append, NULL, DC_EXEC | DC_AUTOREPLACE, false);
						break;
					}

					cost.AddCost(res);
					if (cost.Failed()) break;
				} else {
					/* We have reached 'last_engine', continue with the next engine towards the front */
					assert(append == last_engine);
					last_engine = GetPrevUnit(last_engine);
				}
			}
		}

		/* Sell superfluous new vehicles that could not be inserted. */
		if (cost.Succeeded() && wagon_removal) {
			for (int i = 1; i < num_units; i++) {
				Vehicle *wagon = new_vehs[i];
				if (wagon == NULL) continue;
				if (wagon->First() == new_head) break;

				assert(RailVehInfo(wagon->engine_type)->railveh_type == RAILVEH_WAGON);

				/* Sell wagon */
				CommandCost ret = DoCommand(0, wagon->index, 0, DC_EXEC, GetCmdSellVeh(wagon));
				assert(ret.Succeeded());
				new_vehs[i] = NULL;

				/* Revert the money subtraction when the vehicle was built.
				 * This value is different from the sell value, esp. because of refitting */
				cost.AddCost(-new_costs[i]);
			}
		}

		/* The new vehicle chain is constructed, now take over orders and everything... */
		if (cost.Succeeded()) cost.AddCost(CopyHeadSpecificThings(old_head, new_head, flags));

		if (cost.Succeeded()) {
			/* Success ! */
			if ((flags & DC_EXEC) != 0 && new_head != old_head) {
				*chain = new_head;
			}

			/* Transfer cargo of old vehicles and sell them*/
			for (int i = 0; i < num_units; i++) {
				Vehicle *w = old_vehs[i];
				/* Is the vehicle again part of the new chain?
				 * Note: We cannot test 'new_vehs[i] != NULL' as wagon removal might cause to remove both */
				if (w->First() == new_head) continue;

				if ((flags & DC_EXEC) != 0) TransferCargo(w, new_head);

				cost.AddCost(DoCommand(0, w->index, 0, flags, GetCmdSellVeh(w)));
				if ((flags & DC_EXEC) != 0) {
					old_vehs[i] = NULL;
					if (i == 0) old_head = NULL;
				}
			}
		}

		/* If we are not in DC_EXEC undo everything */
		if ((flags & DC_EXEC) == 0) {
			/* Separate the head, so we can reattach the old vehicles */
			Vehicle *second = GetNextUnit(old_head);
			if (second != NULL) MoveVehicle(second, NULL, DC_EXEC | DC_AUTOREPLACE, true);

			assert(GetNextUnit(old_head) == NULL);

			/* Rearrange old vehicles and sell new
			 * We do this from back to front, so that the head of the temporary vehicle chain does not change all the time.
			 * Note: The vehicle attach callback is disabled here :) */

			for (int i = num_units - 1; i >= 0; i--) {
				if (i > 0) {
					CommandCost ret = MoveVehicle(old_vehs[i], old_head, DC_EXEC | DC_AUTOREPLACE, false);
					assert(ret.Succeeded());
				}
				if (new_vehs[i] != NULL) {
					DoCommand(0, new_vehs[i]->index, 0, DC_EXEC, GetCmdSellVeh(new_vehs[i]));
					new_vehs[i] = NULL;
				}
			}
		}

		free(old_vehs);
		free(new_vehs);
		free(new_costs);
	} else {
		/* Build and refit replacement vehicle */
		Vehicle *new_head = NULL;
		cost.AddCost(BuildReplacementVehicle(old_head, &new_head));

		/* Was a new vehicle constructed? */
		if (cost.Succeeded() && new_head != NULL) {
			*nothing_to_do = false;

			/* The new vehicle is constructed, now take over orders and everything... */
			cost.AddCost(CopyHeadSpecificThings(old_head, new_head, flags));

			if (cost.Succeeded()) {
				/* The new vehicle is constructed, now take over cargo */
				if ((flags & DC_EXEC) != 0) {
					TransferCargo(old_head, new_head);
					*chain = new_head;
				}

				/* Sell the old vehicle */
				cost.AddCost(DoCommand(0, old_head->index, 0, flags, GetCmdSellVeh(old_head)));
			}

			/* If we are not in DC_EXEC undo everything */
			if ((flags & DC_EXEC) == 0) {
				DoCommand(0, new_head->index, 0, DC_EXEC, GetCmdSellVeh(new_head));
			}
		}
	}

	return cost;
}

/** Autoreplace a vehicles
 * @param tile not used
 * @param flags type of operation
 * @param p1 Index of vehicle
 * @param p2 not used
 */
CommandCost CmdAutoreplaceVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CommandCost cost = CommandCost(EXPENSES_NEW_VEHICLES, 0);
	bool nothing_to_do = true;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;
	Vehicle *v = GetVehicle(p1);
	if (!CheckOwnership(v->owner)) return CMD_ERROR;
	if (!v->IsInDepot()) return CMD_ERROR;
	if (HASBITS(v->vehstatus, VS_CRASHED)) return CMD_ERROR;

	const Player *p = GetPlayer(_current_player);
	bool wagon_removal = p->renew_keep_length;

	/* Test whether any replacement is set, before issuing a whole lot of commands that would end in nothing changed */
	Vehicle *w = v;
	bool any_replacements = false;
	while (w != NULL && !any_replacements) {
		any_replacements = (GetNewEngineType(w, p) != INVALID_ENGINE);
		w = (w->type == VEH_TRAIN ? GetNextUnit(w) : NULL);
	}

	if (any_replacements) {
		bool was_stopped = (v->vehstatus & VS_STOPPED) != 0;

		/* Stop the vehicle */
		if (!was_stopped) cost.AddCost(StartStopVehicle(v, true));
		if (cost.Failed()) return cost;

		assert(v->IsStoppedInDepot());

		/* We have to construct the new vehicle chain to test whether it is valid.
		 * Vehicle construction needs random bits, so we have to save the random seeds
		 * to prevent desyncs and to replay newgrf callbacks during DC_EXEC */
		SavedRandomSeeds saved_seeds;
		SaveRandomSeeds(&saved_seeds);
		cost.AddCost(ReplaceChain(&v, flags & ~DC_EXEC, wagon_removal, &nothing_to_do));
		RestoreRandomSeeds(saved_seeds);

		if (cost.Succeeded() && (flags & DC_EXEC) != 0) {
			CommandCost ret = ReplaceChain(&v, flags, wagon_removal, &nothing_to_do);
			assert(ret.Succeeded() && ret.GetCost() == cost.GetCost());
		}

		/* Restart the vehicle */
		if (!was_stopped) cost.AddCost(StartStopVehicle(v, false));
	}

	if (cost.Succeeded() && nothing_to_do) cost = CommandCost(STR_AUTOREPLACE_NOTHING_TO_DO);
	return cost;
}
