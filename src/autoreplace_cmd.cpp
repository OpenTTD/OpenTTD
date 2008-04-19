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
	if (dest->type == VEH_TRAIN) TrainConsistChanged(dest->First());
}

static bool VerifyAutoreplaceRefitForOrders(const Vehicle *v, const EngineID engine_type)
{
	const Order *o;
	const Vehicle *u;

	if (v->type == VEH_TRAIN) {
		u = v->First();
	} else {
		u = v;
	}

	FOR_VEHICLE_ORDERS(u, o) {
		if (!o->IsRefit()) continue;
		if (!CanRefitTo(v->engine_type, o->GetRefitCargo())) continue;
		if (!CanRefitTo(engine_type, o->GetRefitCargo())) return false;
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
	CargoID new_cargo_type = GetEngineCargoType(engine_type);

	if (new_cargo_type == CT_INVALID) return CT_NO_REFIT; // Don't try to refit an engine with no cargo capacity

	if (v->cargo_cap != 0 && (v->cargo_type == new_cargo_type || CanRefitTo(engine_type, v->cargo_type))) {
		if (VerifyAutoreplaceRefitForOrders(v, engine_type)) {
			return v->cargo_type == new_cargo_type ? (CargoID)CT_NO_REFIT : v->cargo_type;
		} else {
			return CT_INVALID;
		}
	}
	if (v->type != VEH_TRAIN) return CT_INVALID; // We can't refit the vehicle to carry the cargo we want

	/* Below this line it's safe to assume that the vehicle in question is a train */

	if (v->cargo_cap != 0) return CT_INVALID; // trying to replace a vehicle with cargo capacity into another one with incompatible cargo type

	/* the old engine didn't have cargo capacity, but the new one does
	 * now we will figure out what cargo the train is carrying and refit to fit this */
	v = v->First();
	do {
		if (v->cargo_cap == 0) continue;
		/* Now we found a cargo type being carried on the train and we will see if it is possible to carry to this one */
		if (v->cargo_type == new_cargo_type) return CT_NO_REFIT;
		if (CanRefitTo(engine_type, v->cargo_type)) return v->cargo_type;
	} while ((v = v->Next()) != NULL);
	return CT_NO_REFIT; // We failed to find a cargo type on the old vehicle and we will not refit the new one
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
static CommandCost ReplaceVehicle(Vehicle **w, byte flags, Money total_cost, const Player *p, EngineID new_engine_type)
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

	cost = DoCommand(old_v->tile, new_engine_type, 3, flags, GetCmdBuildVeh(old_v));
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
				DoCommand(0, (front->index << 16) | new_v->index, 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
			}
		} else {
			// copy/clone the orders
			DoCommand(0, (old_v->index << 16) | new_v->index, old_v->IsOrderListShared() ? CO_SHARE : CO_COPY, DC_EXEC, CMD_CLONE_ORDER);
			new_v->cur_order_index = old_v->cur_order_index;
			ChangeVehicleViewWindow(old_v, new_v);
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
					DoCommand(0, (new_v->index << 16) | temp_v->index, 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
				}
			}
		}
		/* We are done setting up the new vehicle. Now we move the cargo from the old one to the new one */
		MoveVehicleCargo(new_v->type == VEH_TRAIN ? new_v->First() : new_v, old_v);

		// Get the name of the old vehicle if it has a custom name.
		if (old_v->name != NULL) vehicle_name = strdup(old_v->name);
	} else { // flags & DC_EXEC not set
		CommandCost tmp_move;

		if (old_v->type == VEH_TRAIN && IsFrontEngine(old_v)) {
			Vehicle *next_veh = GetNextUnit(old_v); // don't try to move the rear multiheaded engine or articulated parts
			if (next_veh != NULL) {
				/* Verify that the wagons can be placed on the engine in question.
				 * This is done by building an engine, test if the wagons can be added and then sell the test engine. */
				DoCommand(old_v->tile, new_engine_type, 3, DC_EXEC, GetCmdBuildVeh(old_v));
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
			AddNewsItem(STR_TRAIN_TOO_LONG_AFTER_REPLACEMENT, NM_SMALL, NF_VIEWPORT | NF_VEHICLE, NT_ADVICE, DNC_NONE, front->index, 0);
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
	const Player *p = GetPlayer(v->owner);
	CommandCost cost;
	bool stopped = false;

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

			/* Now replace the vehicle */
			cost.AddCost(ReplaceVehicle(&w, flags, cost.GetCost(), p, new_engine));

			if (flags & DC_EXEC &&
					(w->type != VEH_TRAIN || w->u.rail.first_engine == INVALID_ENGINE)) {
				/* now we bought a new engine and sold the old one. We need to fix the
				 * pointers in order to avoid pointing to the old one for trains: these
				 * pointers should point to the front engine and not the cars
				 */
				v = w;
			}
		} while (w->type == VEH_TRAIN && (w = GetNextVehicle(w)) != NULL);

		if (flags & DC_QUERY_COST || cost.GetCost() == 0) {
			/* We didn't do anything during the replace so we will just exit here */
			if (stopped) v->vehstatus &= ~VS_STOPPED;
			return cost;
		}

		if (display_costs && !(flags & DC_EXEC)) {
			/* We want to ensure that we will not get below p->engine_renew_money.
			 * We will not actually pay this amount. It's for display and checks only. */
			cost.AddCost((Money)p->engine_renew_money);
			if (CmdSucceeded(cost) && GetAvailableMoneyForCommand() < cost.GetCost()) {
				/* We don't have enough money so we will set cost to failed */
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

				AddNewsItem(message, NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, DNC_NONE, v->index, 0);
			}
		}
	}

	if (flags & DC_EXEC && CmdSucceeded(cost)) {
		if (v->type == VEH_TRAIN && p->renew_keep_length) {
			/* Remove wagons until the wanted length is reached */
			cost.AddCost(WagonRemoval(v, old_total_length));
		}

		if (display_costs && IsLocalPlayer()) {
			ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, cost.GetCost());
		}
	}

	/* Start the vehicle if we stopped it earlier */
	if (stopped) v->vehstatus &= ~VS_STOPPED;

	return cost;
}
