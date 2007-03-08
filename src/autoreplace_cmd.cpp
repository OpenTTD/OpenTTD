/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "roadveh.h"
#include "ship.h"
#include "table/strings.h"
#include "functions.h"
#include "news.h"
#include "command.h"
#include "player.h"
#include "engine.h"
#include "debug.h"
#include "vehicle_gui.h"
#include "depot.h"
#include "train.h"
#include "aircraft.h"
#include "cargotype.h"


/*
 * move the cargo from one engine to another if possible
 */
static void MoveVehicleCargo(Vehicle *dest, Vehicle *source)
{
	Vehicle *v = dest;
	int units_moved;

	do {
		do {
			if (source->cargo_type != dest->cargo_type)
				continue; // cargo not compatible

			if (dest->cargo_count == dest->cargo_cap)
				continue; // the destination vehicle is already full

			units_moved = min(source->cargo_count, dest->cargo_cap - dest->cargo_count);
			source->cargo_count -= units_moved;
			dest->cargo_count   += units_moved;
			dest->cargo_source   = source->cargo_source;

			// copy the age of the cargo
			dest->cargo_days   = source->cargo_days;
			dest->day_counter  = source->day_counter;
			dest->tick_counter = source->tick_counter;

		} while (source->cargo_count > 0 && (dest = dest->next) != NULL);
		dest = v;
	} while ((source = source->next) != NULL);

	/*
	 * The of the train will be incorrect at this moment. This is due
	 * to the fact that removing the old wagon updates the weight of
	 * the complete train, which is without the weight of cargo we just
	 * moved back into some (of the) new wagon(s).
	 */
	if (dest->type == VEH_TRAIN) TrainConsistChanged(dest->first);
}

static bool VerifyAutoreplaceRefitForOrders(const Vehicle *v, const EngineID engine_type)
{
	const Order *o;
	const Vehicle *u;

	if (v->type == VEH_TRAIN) {
		u = GetFirstVehicleInChain(v);
	} else {
		u = v;
	}

	FOR_VEHICLE_ORDERS(u, o) {
		if (!(o->refit_cargo < NUM_CARGO)) continue;
		if (!CanRefitTo(v->engine_type, o->refit_cargo)) continue;
		if (!CanRefitTo(engine_type, o->refit_cargo)) return false;
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
	bool new_cargo_capacity = true;
	CargoID new_cargo_type = CT_INVALID;

	switch (v->type) {
		case VEH_TRAIN:
			new_cargo_capacity = (RailVehInfo(engine_type)->capacity > 0);
			new_cargo_type     = RailVehInfo(engine_type)->cargo_type;
			break;

		case VEH_ROAD:
			new_cargo_capacity = (RoadVehInfo(engine_type)->capacity > 0);
			new_cargo_type     = RoadVehInfo(engine_type)->cargo_type;
			break;
		case VEH_SHIP:
			new_cargo_capacity = (ShipVehInfo(engine_type)->capacity > 0);
			new_cargo_type     = ShipVehInfo(engine_type)->cargo_type;
			break;

		case VEH_AIRCRAFT:
			/* all aircraft starts as passenger planes with cargo capacity
			 * new_cargo_capacity is always true for aircraft, which is the init value. No need to set it here */
			new_cargo_type     = CT_PASSENGERS;
			break;

		default: NOT_REACHED(); break;
	}

	if (!new_cargo_capacity) return CT_NO_REFIT; // Don't try to refit an engine with no cargo capacity

	if (v->cargo_type == new_cargo_type || CanRefitTo(engine_type, v->cargo_type)) {
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
	v = GetFirstVehicleInChain(v);
	do {
		if (v->cargo_cap == 0) continue;
		/* Now we found a cargo type being carried on the train and we will see if it is possible to carry to this one */
		if (v->cargo_type == new_cargo_type) return CT_NO_REFIT;
		if (CanRefitTo(engine_type, v->cargo_type)) return v->cargo_type;
	} while ((v=v->next) != NULL);
	return CT_NO_REFIT; // We failed to find a cargo type on the old vehicle and we will not refit the new one
}

/* Replaces a vehicle (used to be called autorenew)
 * This function is only called from MaybeReplaceVehicle()
 * Must be called with _current_player set to the owner of the vehicle
 * @param w Vehicle to replace
 * @param flags is the flags to use when calling DoCommand(). Mainly DC_EXEC counts
 * @return value is cost of the replacement or CMD_ERROR
 */
static int32 ReplaceVehicle(Vehicle **w, byte flags, int32 total_cost)
{
	int32 cost;
	int32 sell_value;
	Vehicle *old_v = *w;
	const Player *p = GetPlayer(old_v->owner);
	EngineID new_engine_type;
	const UnitID cached_unitnumber = old_v->unitnumber;
	bool new_front = false;
	Vehicle *new_v = NULL;
	char vehicle_name[32];
	CargoID replacement_cargo_type;

	new_engine_type = EngineReplacementForPlayer(p, old_v->engine_type);
	if (new_engine_type == INVALID_ENGINE) new_engine_type = old_v->engine_type;

	replacement_cargo_type = GetNewCargoTypeForReplace(old_v, new_engine_type);

	/* check if we can't refit to the needed type, so no replace takes place to prevent the vehicle from altering cargo type */
	if (replacement_cargo_type == CT_INVALID) return 0;

	sell_value = DoCommand(0, old_v->index, 0, DC_QUERY_COST, GetCmdSellVeh(old_v));

	/* We give the player a loan of the same amount as the sell value.
	 * This is needed in case he needs the income from the sale to build the new vehicle.
	 * We take it back if building fails or when we really sell the old engine */
	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
	SubtractMoneyFromPlayer(sell_value);

	cost = DoCommand(old_v->tile, new_engine_type, 3, flags, GetCmdBuildVeh(old_v));
	if (CmdFailed(cost)) {
		SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
		SubtractMoneyFromPlayer(-sell_value); // Take back the money we just gave the player
		return cost;
	}

	if (replacement_cargo_type != CT_NO_REFIT) cost += GetRefitCost(new_engine_type); // add refit cost

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

		if (new_v->type == VEH_TRAIN && HASBIT(old_v->u.rail.flags, VRF_REVERSE_DIRECTION) && !IsMultiheaded(new_v) && !(new_v->next != NULL && IsArticulatedPart(new_v->next))) {
			// we are autorenewing to a single engine, so we will turn it as the old one was turned as well
			SETBIT(new_v->u.rail.flags, VRF_REVERSE_DIRECTION);
		}

		if (old_v->type == VEH_TRAIN && !IsFrontEngine(old_v)) {
			/* this is a railcar. We need to move the car into the train
			 * We add the new engine after the old one instead of replacing it. It will give the same result anyway when we
			 * sell the old engine in a moment
			 */
			DoCommand(0, (GetPrevVehicleInChain(old_v)->index << 16) | new_v->index, 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
			/* Now we move the old one out of the train */
			DoCommand(0, (INVALID_VEHICLE << 16) | old_v->index, 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
		} else {
			// copy/clone the orders
			DoCommand(0, (old_v->index << 16) | new_v->index, IsOrderListShared(old_v) ? CO_SHARE : CO_COPY, DC_EXEC, CMD_CLONE_ORDER);
			new_v->cur_order_index = old_v->cur_order_index;
			ChangeVehicleViewWindow(old_v, new_v);
			new_v->profit_this_year = old_v->profit_this_year;
			new_v->profit_last_year = old_v->profit_last_year;
			new_v->service_interval = old_v->service_interval;
			new_front = true;
			new_v->unitnumber = old_v->unitnumber; // use the same unit number

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
		MoveVehicleCargo(new_v->type == VEH_TRAIN ? GetFirstVehicleInChain(new_v) : new_v, old_v);

		// Get the name of the old vehicle if it has a custom name.
		if (!IsCustomName(old_v->string_id)) {
			vehicle_name[0] = '\0';
		} else {
			GetName(vehicle_name, old_v->string_id & 0x7FF, lastof(vehicle_name));
		}
	} else { // flags & DC_EXEC not set
		/* Ensure that the player will not end up having negative money while autoreplacing
		 * This is needed because the only other check is done after the income from selling the old vehicle is substracted from the cost */
		if (p->money64 < (cost + total_cost)) {
			SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
			SubtractMoneyFromPlayer(-sell_value); // Pay back the loan
			return CMD_ERROR;
		}
	}

	/* Take back the money we just gave the player just before building the vehicle
	 * The player will get the same amount now that the sale actually takes place */
	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
	SubtractMoneyFromPlayer(-sell_value);

	/* sell the engine/ find out how much you get for the old engine (income is returned as negative cost) */
	cost += DoCommand(0, old_v->index, 0, flags, GetCmdSellVeh(old_v));

	if (new_front) {
		/* now we assign the old unitnumber to the new vehicle */
		new_v->unitnumber = cached_unitnumber;
	}

	/* Transfer the name of the old vehicle */
	if ((flags & DC_EXEC) && vehicle_name[0] != '\0') {
		_cmd_text = vehicle_name;
		DoCommand(0, new_v->index, 0, DC_EXEC, CMD_NAME_VEHICLE);
	}

	return cost;
}

/** replaces a vehicle if it's set for autoreplace or is too old
 * (used to be called autorenew)
 * @param v The vehicle to replace
 * if the vehicle is a train, v needs to be the front engine
 * @param check Checks if the replace is valid. No action is done at all
 * @param display_costs If set, a cost animation is shown (only if check is false)
 * @return CMD_ERROR if something went wrong. Otherwise the price of the replace
 */
int32 MaybeReplaceVehicle(Vehicle *v, bool check, bool display_costs)
{
	Vehicle *w;
	const Player *p = GetPlayer(v->owner);
	byte flags = 0;
	int32 cost, temp_cost = 0;
	bool stopped;

	/* Remember the length in case we need to trim train later on
	 * If it's not a train, the value is unused
	 * round up to the length of the tiles used for the train instead of the train length instead
	 * Useful when newGRF uses custom length */
	uint16 old_total_length = (v->type == VEH_TRAIN ?
		(v->u.rail.cached_total_length + TILE_SIZE - 1) / TILE_SIZE * TILE_SIZE :
		-1
	);


	_current_player = v->owner;

	assert(IsPlayerBuildableVehicleType(v));

	assert(v->vehstatus & VS_STOPPED); // the vehicle should have been stopped in VehicleEnteredDepotThisTick() if needed

	/* Remember the flag v->leave_depot_instantly because if we replace the vehicle, the vehicle holding this flag will be sold
	 * If it is set, then we only stopped the vehicle to replace it (if needed) and we will need to start it again.
	 * We also need to reset the flag since it should remain false except from when the vehicle enters a depot until autoreplace is handled in the same tick */
	stopped = v->leave_depot_instantly;
	v->leave_depot_instantly = false;

	for (;;) {
		cost = 0;
		w = v;
		do {
			if (w->type == VEH_TRAIN && IsMultiheaded(w) && !IsTrainEngine(w)) {
				/* we build the rear ends of multiheaded trains with the front ones */
				continue;
			}

			// check if the vehicle should be replaced
			if (!p->engine_renew ||
					w->age - w->max_age < (p->engine_renew_months * 30) || // replace if engine is too old
					w->max_age == 0) { // rail cars got a max age of 0
				if (!EngineHasReplacementForPlayer(p, w->engine_type)) // updates to a new model
					continue;
			}

			/* Now replace the vehicle */
			temp_cost = ReplaceVehicle(&w, flags, cost);

			if (flags & DC_EXEC &&
					(w->type != VEH_TRAIN || w->u.rail.first_engine == INVALID_ENGINE)) {
				/* now we bought a new engine and sold the old one. We need to fix the
				 * pointers in order to avoid pointing to the old one for trains: these
				 * pointers should point to the front engine and not the cars
				 */
				v = w;
			}

			if (!CmdFailed(temp_cost)) {
				cost += temp_cost;
			}
		} while (w->type == VEH_TRAIN && (w = GetNextVehicle(w)) != NULL);

		if (!(flags & DC_EXEC) && (p->money64 < (int32)(cost + p->engine_renew_money) || cost == 0)) {
			if (!check && p->money64 < (int32)(cost + p->engine_renew_money) && ( _local_player == v->owner ) && cost != 0) {
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

				AddNewsItem(message, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
			}
			if (stopped) v->vehstatus &= ~VS_STOPPED;
			if (display_costs) _current_player = OWNER_NONE;
			return CMD_ERROR;
		}

		if (flags & DC_EXEC) {
			break; // we are done replacing since the loop ran once with DC_EXEC
		} else if (check) {
			/* It's a test only and we know that we can do this
			 * NOTE: payment for wagon removal is NOT included in this price */
			return cost;
		}
		// now we redo the loop, but this time we actually do stuff since we know that we can do it
		flags |= DC_EXEC;
	}

	/* If setting is on to try not to exceed the old length of the train with the replacement */
	if (v->type == VEH_TRAIN && p->renew_keep_length) {
		Vehicle *temp;
		w = v;

		while (v->u.rail.cached_total_length > old_total_length) {
			// the train is too long. We will remove cars one by one from the start of the train until it's short enough
			while (w != NULL && RailVehInfo(w->engine_type)->railveh_type != RAILVEH_WAGON) {
				w = GetNextVehicle(w);
			}
			if (w == NULL) {
				// we failed to make the train short enough
				SetDParam(0, v->unitnumber);
				AddNewsItem(STR_TRAIN_TOO_LONG_AFTER_REPLACEMENT, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
				break;
			}
			temp = w;
			w = GetNextVehicle(w);
			DoCommand(0, (INVALID_VEHICLE << 16) | temp->index, 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
			MoveVehicleCargo(v, temp);
			cost += DoCommand(0, temp->index, 0, DC_EXEC, CMD_SELL_RAIL_WAGON);
		}
	}

	if (stopped) v->vehstatus &= ~VS_STOPPED;
	if (display_costs) {
		if (IsLocalPlayer()) ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, cost);
		_current_player = OWNER_NONE;
	}
	return cost;
}
