/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_cmd.cpp Deals with autoreplace execution but not the setup */

#include "stdafx.h"
#include "company_func.h"
#include "train.h"
#include "command_func.h"
#include "engine_func.h"
#include "vehicle_func.h"
#include "functions.h"
#include "autoreplace_func.h"
#include "autoreplace_gui.h"
#include "group.h"
#include "articulated_vehicles.h"
#include "core/random_func.hpp"

#include "table/strings.h"

extern void ChangeVehicleViewports(VehicleID from_index, VehicleID to_index);
extern void ChangeVehicleNews(VehicleID from_index, VehicleID to_index);
extern void ChangeVehicleViewWindow(VehicleID from_index, VehicleID to_index);

/**
 * Figure out if two engines got at least one type of cargo in common (refitting if needed)
 * @param engine_a one of the EngineIDs
 * @param engine_b the other EngineID
 * @param type the type of the engines
 * @return true if they can both carry the same type of cargo (or at least one of them got no capacity at all)
 */
static bool EnginesHaveCargoInCommon(EngineID engine_a, EngineID engine_b)
{
	uint32 available_cargos_a = GetUnionOfArticulatedRefitMasks(engine_a, true);
	uint32 available_cargos_b = GetUnionOfArticulatedRefitMasks(engine_b, true);
	return (available_cargos_a == 0 || available_cargos_b == 0 || (available_cargos_a & available_cargos_b) != 0);
}

/**
 * Checks some basic properties whether autoreplace is allowed
 * @param from Origin engine
 * @param to Destination engine
 * @param company Company to check for
 * @return true if autoreplace is allowed
 */
bool CheckAutoreplaceValidity(EngineID from, EngineID to, CompanyID company)
{
	assert(Engine::IsValidID(from) && Engine::IsValidID(to));

	/* we can't replace an engine into itself (that would be autorenew) */
	if (from == to) return false;

	const Engine *e_from = Engine::Get(from);
	const Engine *e_to = Engine::Get(to);
	VehicleType type = e_from->type;

	/* check that the new vehicle type is available to the company and its type is the same as the original one */
	if (!IsEngineBuildable(to, type, company)) return false;

	switch (type) {
		case VEH_TRAIN: {
			/* make sure the railtypes are compatible */
			if ((GetRailTypeInfo(e_from->u.rail.railtype)->compatible_railtypes & GetRailTypeInfo(e_to->u.rail.railtype)->compatible_railtypes) == 0) return false;

			/* make sure we do not replace wagons with engines or vise versa */
			if ((e_from->u.rail.railveh_type == RAILVEH_WAGON) != (e_to->u.rail.railveh_type == RAILVEH_WAGON)) return false;
			break;
		}

		case VEH_ROAD:
			/* make sure that we do not replace a tram with a normal road vehicles or vise versa */
			if (HasBit(e_from->info.misc_flags, EF_ROAD_TRAM) != HasBit(e_to->info.misc_flags, EF_ROAD_TRAM)) return false;
			break;

		case VEH_AIRCRAFT:
			/* make sure that we do not replace a plane with a helicopter or vise versa */
			if ((e_from->u.air.subtype & AIR_CTOL) != (e_to->u.air.subtype & AIR_CTOL)) return false;
			break;

		default: break;
	}

	/* the engines needs to be able to carry the same cargo */
	return EnginesHaveCargoInCommon(from, to);
}

/**
 * Transfer cargo from a single (articulated )old vehicle to the new vehicle chain
 * @param old_veh Old vehicle that will be sold
 * @param new_head Head of the completely constructed new vehicle chain
 * @param part_of_chain The vehicle is part of a train
 */
static void TransferCargo(Vehicle *old_veh, Vehicle *new_head, bool part_of_chain)
{
	assert(!part_of_chain || new_head->IsPrimaryVehicle());
	/* Loop through source parts */
	for (Vehicle *src = old_veh; src != NULL; src = src->Next()) {
		if (!part_of_chain && src->type == VEH_TRAIN && src != old_veh && src != Train::From(old_veh)->other_multiheaded_part && !Train::From(src)->IsArticulatedPart()) {
			/* Skip vehicles, which do not belong to old_veh */
			src = Train::From(src)->GetLastEnginePart();
			continue;
		}
		if (src->cargo_type >= NUM_CARGO || src->cargo.Count() == 0) continue;

		/* Find free space in the new chain */
		for (Vehicle *dest = new_head; dest != NULL && src->cargo.Count() > 0; dest = dest->Next()) {
			if (!part_of_chain && dest->type == VEH_TRAIN && dest != new_head && dest != Train::From(new_head)->other_multiheaded_part && !Train::From(dest)->IsArticulatedPart()) {
				/* Skip vehicles, which do not belong to new_head */
				dest = Train::From(dest)->GetLastEnginePart();
				continue;
			}
			if (dest->cargo_type != src->cargo_type) continue;

			uint amount = min(src->cargo.Count(), dest->cargo_cap - dest->cargo.Count());
			if (amount <= 0) continue;

			src->cargo.MoveTo(&dest->cargo, amount, VehicleCargoList::MTA_UNLOAD, NULL);
		}
	}

	/* Update train weight etc., the old vehicle will be sold anyway */
	if (part_of_chain && new_head->type == VEH_TRAIN) Train::From(new_head)->ConsistChanged(true);
}

/**
 * Tests whether refit orders that applied to v will also apply to the new vehicle type
 * @param v The vehicle to be replaced
 * @param engine_type The type we want to replace with
 * @return true iff all refit orders stay valid
 */
static bool VerifyAutoreplaceRefitForOrders(const Vehicle *v, EngineID engine_type)
{

	uint32 union_refit_mask_a = GetUnionOfArticulatedRefitMasks(v->engine_type, false);
	uint32 union_refit_mask_b = GetUnionOfArticulatedRefitMasks(engine_type, false);

	const Order *o;
	const Vehicle *u = (v->type == VEH_TRAIN) ? v->First() : v;
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
 * @param *v Original vehicle that is being replaced.
 * @param engine_type The EngineID of the vehicle that is being replaced to
 * @param part_of_chain The vehicle is part of a train
 * @return The cargo type to replace to
 *    CT_NO_REFIT is returned if no refit is needed
 *    CT_INVALID is returned when both old and new vehicle got cargo capacity and refitting the new one to the old one's cargo type isn't possible
 */
static CargoID GetNewCargoTypeForReplace(Vehicle *v, EngineID engine_type, bool part_of_chain)
{
	uint32 available_cargo_types, union_mask;
	GetArticulatedRefitMasks(engine_type, true, &union_mask, &available_cargo_types);

	if (union_mask == 0) return CT_NO_REFIT; // Don't try to refit an engine with no cargo capacity

	CargoID cargo_type;
	if (IsArticulatedVehicleCarryingDifferentCargos(v, &cargo_type)) return CT_INVALID; // We cannot refit to mixed cargos in an automated way

	if (cargo_type == CT_INVALID) {
		if (v->type != VEH_TRAIN) return CT_NO_REFIT; // If the vehicle does not carry anything at all, every replacement is fine.

		if (!part_of_chain) return CT_NO_REFIT;

		/* the old engine didn't have cargo capacity, but the new one does
		 * now we will figure out what cargo the train is carrying and refit to fit this */

		for (v = v->First(); v != NULL; v = v->Next()) {
			if (v->cargo_cap == 0) continue;
			/* Now we found a cargo type being carried on the train and we will see if it is possible to carry to this one */
			if (HasBit(available_cargo_types, v->cargo_type)) {
				/* Do we have to refit the vehicle, or is it already carrying the right cargo? */
				CargoArray default_capacity = GetCapacityOfArticulatedParts(engine_type);
				for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
					if (cid != v->cargo_type && default_capacity[cid] > 0) return v->cargo_type;
				}

				return CT_NO_REFIT;
			}
		}

		return CT_NO_REFIT; // We failed to find a cargo type on the old vehicle and we will not refit the new one
	} else {
		if (!HasBit(available_cargo_types, cargo_type)) return CT_INVALID; // We can't refit the vehicle to carry the cargo we want

		if (part_of_chain && !VerifyAutoreplaceRefitForOrders(v, engine_type)) return CT_INVALID; // Some refit orders lose their effect

		/* Do we have to refit the vehicle, or is it already carrying the right cargo? */
		CargoArray default_capacity = GetCapacityOfArticulatedParts(engine_type);
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			if (cid != cargo_type && default_capacity[cid] > 0) return cargo_type;
		}

		return CT_NO_REFIT;
	}
}

/**
 * Get the EngineID of the replacement for a vehicle
 * @param v The vehicle to find a replacement for
 * @param c The vehicle's owner (it's faster to forward the pointer than refinding it)
 * @return the EngineID of the replacement. INVALID_ENGINE if no buildable replacement is found
 */
static EngineID GetNewEngineType(const Vehicle *v, const Company *c)
{
	assert(v->type != VEH_TRAIN || !Train::From(v)->IsArticulatedPart());

	if (v->type == VEH_TRAIN && Train::From(v)->IsRearDualheaded()) {
		/* we build the rear ends of multiheaded trains with the front ones */
		return INVALID_ENGINE;
	}

	EngineID e = EngineReplacementForCompany(c, v->engine_type, v->group_id);

	if (e != INVALID_ENGINE && IsEngineBuildable(e, v->type, _current_company)) {
		return e;
	}

	if (v->NeedsAutorenewing(c) && // replace if engine is too old
	    IsEngineBuildable(v->engine_type, v->type, _current_company)) { // engine can still be build
		return v->engine_type;
	}

	return INVALID_ENGINE;
}

/**
 * Builds and refits a replacement vehicle
 * Important: The old vehicle is still in the original vehicle chain (used for determining the cargo when the old vehicle did not carry anything, but the new one does)
 * @param old_veh A single (articulated/multiheaded) vehicle that shall be replaced.
 * @param new_vehicle Returns the newly build and refittet vehicle
 * @param part_of_chain The vehicle is part of a train
 * @return cost or error
 */
static CommandCost BuildReplacementVehicle(Vehicle *old_veh, Vehicle **new_vehicle, bool part_of_chain)
{
	*new_vehicle = NULL;

	/* Shall the vehicle be replaced? */
	const Company *c = Company::Get(_current_company);
	EngineID e = GetNewEngineType(old_veh, c);
	if (e == INVALID_ENGINE) return CommandCost(); // neither autoreplace is set, nor autorenew is triggered

	/* Does it need to be refitted */
	CargoID refit_cargo = GetNewCargoTypeForReplace(old_veh, e, part_of_chain);
	if (refit_cargo == CT_INVALID) return CommandCost(); // incompatible cargos

	/* Build the new vehicle */
	CommandCost cost = DoCommand(old_veh->tile, e, 0, DC_EXEC | DC_AUTOREPLACE, GetCmdBuildVeh(old_veh));
	if (cost.Failed()) return cost;

	Vehicle *new_veh = Vehicle::Get(_new_vehicle_id);
	*new_vehicle = new_veh;

	/* Refit the vehicle if needed */
	byte subtype = GetBestFittingSubType(old_veh, new_veh);
	/* If the subtype isn't zero and the refit cargo is not set,
	 * we're better off setting the refit cargo too. */
	if (subtype != 0 && refit_cargo == CT_NO_REFIT) refit_cargo = old_veh->cargo_type;

	if (refit_cargo != CT_NO_REFIT) {
		cost.AddCost(DoCommand(0, new_veh->index, refit_cargo | (subtype << 8), DC_EXEC, GetCmdRefitVeh(new_veh)));
		assert(cost.Succeeded()); // This should be ensured by GetNewCargoTypeForReplace()
	}

	/* Try to reverse the vehicle, but do not care if it fails as the new type might not be reversible */
	if (new_veh->type == VEH_TRAIN && HasBit(Train::From(old_veh)->flags, VRF_REVERSE_DIRECTION)) {
		DoCommand(0, new_veh->index, true, DC_EXEC, CMD_REVERSE_TRAIN_DIRECTION);
	}

	return cost;
}

/**
 * Issue a start/stop command
 * @param v a vehicle
 * @param evaluate_callback shall the start/stop callback be evaluated?
 * @return success or error
 */
static inline CommandCost CmdStartStopVehicle(const Vehicle *v, bool evaluate_callback)
{
	return DoCommand(0, v->index, evaluate_callback ? 1 : 0, DC_EXEC | DC_AUTOREPLACE, CMD_START_STOP_VEHICLE);
}

/**
 * Issue a train vehicle move command
 * @param v The vehicle to move
 * @param after The vehicle to insert 'v' after, or NULL to start new chain
 * @param flags the command flags to use
 * @param whole_chain move all vehicles following 'v' (true), or only 'v' (false)
 * @return success or error
 */
static inline CommandCost CmdMoveVehicle(const Vehicle *v, const Vehicle *after, DoCommandFlag flags, bool whole_chain)
{
	return DoCommand(0, v->index | (whole_chain ? 1 : 0) << 20, after != NULL ? after->index : INVALID_VEHICLE, flags, CMD_MOVE_RAIL_VEHICLE);
}

/**
 * Copy head specific things to the new vehicle chain after it was successfully constructed
 * @param old_head The old front vehicle (no wagons attached anymore)
 * @param new_head The new head of the completely replaced vehicle chain
 * @param flags the command flags to use
 */
static CommandCost CopyHeadSpecificThings(Vehicle *old_head, Vehicle *new_head, DoCommandFlag flags)
{
	CommandCost cost = CommandCost();

	/* Share orders */
	if (cost.Succeeded() && old_head != new_head) cost.AddCost(DoCommand(0, new_head->index | CO_SHARE << 30, old_head->index, DC_EXEC, CMD_CLONE_ORDER));

	/* Copy group membership */
	if (cost.Succeeded() && old_head != new_head) cost.AddCost(DoCommand(0, old_head->group_id, new_head->index, DC_EXEC, CMD_ADD_VEHICLE_GROUP));

	/* Perform start/stop check whether the new vehicle suits newgrf restrictions etc. */
	if (cost.Succeeded()) {
		/* Start the vehicle, might be denied by certain things */
		assert((new_head->vehstatus & VS_STOPPED) != 0);
		cost.AddCost(CmdStartStopVehicle(new_head, true));

		/* Stop the vehicle again, but do not care about evil newgrfs allowing starting but not stopping :p */
		if (cost.Succeeded()) cost.AddCost(CmdStartStopVehicle(new_head, false));
	}

	/* Last do those things which do never fail (resp. we do not care about), but which are not undo-able */
	if (cost.Succeeded() && old_head != new_head && (flags & DC_EXEC) != 0) {
		/* Copy vehicle name */
		if (old_head->name != NULL) {
			DoCommand(0, new_head->index, 0, DC_EXEC | DC_AUTOREPLACE, CMD_RENAME_VEHICLE, old_head->name);
		}

		/* Copy other things which cannot be copied by a command and which shall not stay resetted from the build vehicle command */
		new_head->CopyVehicleConfigAndStatistics(old_head);

		/* Switch vehicle windows/news to the new vehicle, so they are not closed/deleted when the old vehicle is sold */
		ChangeVehicleViewports(old_head->index, new_head->index);
		ChangeVehicleViewWindow(old_head->index, new_head->index);
		ChangeVehicleNews(old_head->index, new_head->index);
	}

	return cost;
}

/**
 * Replace a single unit in a free wagon chain
 * @param single_unit vehicle to let autoreplace/renew operator on
 * @param flags command flags
 * @param nothing_to_do is set to 'false' when something was done (only valid when not failed)
 * @return cost or error
 */
static CommandCost ReplaceFreeUnit(Vehicle **single_unit, DoCommandFlag flags, bool *nothing_to_do)
{
	Train *old_v = Train::From(*single_unit);
	assert(!old_v->IsArticulatedPart() && !old_v->IsRearDualheaded());

	CommandCost cost = CommandCost(EXPENSES_NEW_VEHICLES, 0);

	/* Build and refit replacement vehicle */
	Vehicle *new_v = NULL;
	cost.AddCost(BuildReplacementVehicle(old_v, &new_v, false));

	/* Was a new vehicle constructed? */
	if (cost.Succeeded() && new_v != NULL) {
		*nothing_to_do = false;

		if ((flags & DC_EXEC) != 0) {
			/* Move the new vehicle behind the old */
			CmdMoveVehicle(new_v, old_v, DC_EXEC, false);

			/* Take over cargo
			 * Note: We do only transfer cargo from the old to the new vehicle.
			 *       I.e. we do not transfer remaining cargo to other vehicles.
			 *       Else you would also need to consider moving cargo to other free chains,
			 *       or doing the same in ReplaceChain(), which would be quite troublesome.
			 */
			TransferCargo(old_v, new_v, false);

			*single_unit = new_v;
		}

		/* Sell the old vehicle */
		cost.AddCost(DoCommand(0, old_v->index, 0, flags, GetCmdSellVeh(old_v)));

		/* If we are not in DC_EXEC undo everything */
		if ((flags & DC_EXEC) == 0) {
			DoCommand(0, new_v->index, 0, DC_EXEC, GetCmdSellVeh(new_v));
		}
	}

	return cost;
}

/**
 * Replace a whole vehicle chain
 * @param chain vehicle chain to let autoreplace/renew operator on
 * @param flags command flags
 * @param wagon_removal remove wagons when the resulting chain occupies more tiles than the old did
 * @param nothing_to_do is set to 'false' when something was done (only valid when not failed)
 * @return cost or error
 */
static CommandCost ReplaceChain(Vehicle **chain, DoCommandFlag flags, bool wagon_removal, bool *nothing_to_do)
{
	Vehicle *old_head = *chain;
	assert(old_head->IsPrimaryVehicle());

	CommandCost cost = CommandCost(EXPENSES_NEW_VEHICLES, 0);

	if (old_head->type == VEH_TRAIN) {
		/* Store the length of the old vehicle chain, rounded up to whole tiles */
		uint16 old_total_length = CeilDiv(Train::From(old_head)->gcache.cached_total_length, TILE_SIZE) * TILE_SIZE;

		int num_units = 0; ///< Number of units in the chain
		for (Train *w = Train::From(old_head); w != NULL; w = w->GetNextUnit()) num_units++;

		Train **old_vehs = CallocT<Train *>(num_units); ///< Will store vehicles of the old chain in their order
		Train **new_vehs = CallocT<Train *>(num_units); ///< New vehicles corresponding to old_vehs or NULL if no replacement
		Money *new_costs = MallocT<Money>(num_units);   ///< Costs for buying and refitting the new vehicles

		/* Collect vehicles and build replacements
		 * Note: The replacement vehicles can only successfully build as long as the old vehicles are still in their chain */
		int i;
		Train *w;
		for (w = Train::From(old_head), i = 0; w != NULL; w = w->GetNextUnit(), i++) {
			assert(i < num_units);
			old_vehs[i] = w;

			CommandCost ret = BuildReplacementVehicle(old_vehs[i], (Vehicle**)&new_vehs[i], true);
			cost.AddCost(ret);
			if (cost.Failed()) break;

			new_costs[i] = ret.GetCost();
			if (new_vehs[i] != NULL) *nothing_to_do = false;
		}
		Train *new_head = (new_vehs[0] != NULL ? new_vehs[0] : old_vehs[0]);

		/* Note: When autoreplace has already failed here, old_vehs[] is not completely initialized. But it is also not needed. */
		if (cost.Succeeded()) {
			/* Separate the head, so we can start constructing the new chain */
			Train *second = Train::From(old_head)->GetNextUnit();
			if (second != NULL) cost.AddCost(CmdMoveVehicle(second, NULL, DC_EXEC | DC_AUTOREPLACE, true));

			assert(Train::From(new_head)->GetNextUnit() == NULL);

			/* Append engines to the new chain
			 * We do this from back to front, so that the head of the temporary vehicle chain does not change all the time.
			 * That way we also have less trouble when exceeding the unitnumber limit.
			 * OTOH the vehicle attach callback is more expensive this way :s */
			Train *last_engine = NULL; ///< Shall store the last engine unit after this step
			if (cost.Succeeded()) {
				for (int i = num_units - 1; i > 0; i--) {
					Train *append = (new_vehs[i] != NULL ? new_vehs[i] : old_vehs[i]);

					if (RailVehInfo(append->engine_type)->railveh_type == RAILVEH_WAGON) continue;

					if (new_vehs[i] != NULL) {
						/* Move the old engine to a separate row with DC_AUTOREPLACE. Else
						 * moving the wagon in front may fail later due to unitnumber limit.
						 * (We have to attach wagons without DC_AUTOREPLACE.) */
						CmdMoveVehicle(old_vehs[i], NULL, DC_EXEC | DC_AUTOREPLACE, false);
					}

					if (last_engine == NULL) last_engine = append;
					cost.AddCost(CmdMoveVehicle(append, new_head, DC_EXEC, false));
					if (cost.Failed()) break;
				}
				if (last_engine == NULL) last_engine = new_head;
			}

			/* When wagon removal is enabled and the new engines without any wagons are already longer than the old, we have to fail */
			if (cost.Succeeded() && wagon_removal && new_head->gcache.cached_total_length > old_total_length) cost = CommandCost(STR_ERROR_TRAIN_TOO_LONG_AFTER_REPLACEMENT);

			/* Append/insert wagons into the new vehicle chain
			 * We do this from back to front, so we can stop when wagon removal or maximum train length (i.e. from mammoth-train setting) is triggered.
			 */
			if (cost.Succeeded()) {
				for (int i = num_units - 1; i > 0; i--) {
					assert(last_engine != NULL);
					Vehicle *append = (new_vehs[i] != NULL ? new_vehs[i] : old_vehs[i]);

					if (RailVehInfo(append->engine_type)->railveh_type == RAILVEH_WAGON) {
						/* Insert wagon after 'last_engine' */
						CommandCost res = CmdMoveVehicle(append, last_engine, DC_EXEC, false);

						if (res.Succeeded() && wagon_removal && new_head->gcache.cached_total_length > old_total_length) {
							CmdMoveVehicle(append, NULL, DC_EXEC | DC_AUTOREPLACE, false);
							break;
						}

						cost.AddCost(res);
						if (cost.Failed()) break;
					} else {
						/* We have reached 'last_engine', continue with the next engine towards the front */
						assert(append == last_engine);
						last_engine = last_engine->GetPrevUnit();
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

				/* Transfer cargo of old vehicles and sell them */
				for (int i = 0; i < num_units; i++) {
					Vehicle *w = old_vehs[i];
					/* Is the vehicle again part of the new chain?
					 * Note: We cannot test 'new_vehs[i] != NULL' as wagon removal might cause to remove both */
					if (w->First() == new_head) continue;

					if ((flags & DC_EXEC) != 0) TransferCargo(w, new_head, true);

					/* Sell the vehicle.
					 * Note: This might temporarly construct new trains, so use DC_AUTOREPLACE to prevent
					 *       it from failing due to engine limits. */
					cost.AddCost(DoCommand(0, w->index, 0, flags | DC_AUTOREPLACE, GetCmdSellVeh(w)));
					if ((flags & DC_EXEC) != 0) {
						old_vehs[i] = NULL;
						if (i == 0) old_head = NULL;
					}
				}
			}

			/* If we are not in DC_EXEC undo everything, i.e. rearrange old vehicles.
			 * We do this from back to front, so that the head of the temporary vehicle chain does not change all the time.
			 * Note: The vehicle attach callback is disabled here :) */
			if ((flags & DC_EXEC) == 0) {
				/* Separate the head, so we can reattach the old vehicles */
				Train *second = Train::From(old_head)->GetNextUnit();
				if (second != NULL) CmdMoveVehicle(second, NULL, DC_EXEC | DC_AUTOREPLACE, true);

				assert(Train::From(old_head)->GetNextUnit() == NULL);

				for (int i = num_units - 1; i > 0; i--) {
					CommandCost ret = CmdMoveVehicle(old_vehs[i], old_head, DC_EXEC | DC_AUTOREPLACE, false);
					assert(ret.Succeeded());
				}
			}
		}

		/* Finally undo buying of new vehicles */
		if ((flags & DC_EXEC) == 0) {
			for (int i = num_units - 1; i >= 0; i--) {
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
		cost.AddCost(BuildReplacementVehicle(old_head, &new_head, true));

		/* Was a new vehicle constructed? */
		if (cost.Succeeded() && new_head != NULL) {
			*nothing_to_do = false;

			/* The new vehicle is constructed, now take over orders and everything... */
			cost.AddCost(CopyHeadSpecificThings(old_head, new_head, flags));

			if (cost.Succeeded()) {
				/* The new vehicle is constructed, now take over cargo */
				if ((flags & DC_EXEC) != 0) {
					TransferCargo(old_head, new_head, true);
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

/**
 * Autoreplaces a vehicle
 * Trains are replaced as a whole chain, free wagons in depot are replaced on their own
 * @param tile not used
 * @param flags type of operation
 * @param p1 Index of vehicle
 * @param p2 not used
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdAutoreplaceVehicle(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Vehicle *v = Vehicle::GetIfValid(p1);
	if (v == NULL) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (!v->IsInDepot()) return CMD_ERROR;
	if (v->vehstatus & VS_CRASHED) return CMD_ERROR;

	bool free_wagon = false;
	if (v->type == VEH_TRAIN) {
		Train *t = Train::From(v);
		if (t->IsArticulatedPart() || t->IsRearDualheaded()) return CMD_ERROR;
		free_wagon = !t->IsFrontEngine();
		if (free_wagon && t->First()->IsFrontEngine()) return CMD_ERROR;
	} else {
		if (!v->IsPrimaryVehicle()) return CMD_ERROR;
	}

	const Company *c = Company::Get(_current_company);
	bool wagon_removal = c->settings.renew_keep_length;

	/* Test whether any replacement is set, before issuing a whole lot of commands that would end in nothing changed */
	Vehicle *w = v;
	bool any_replacements = false;
	while (w != NULL && !any_replacements) {
		any_replacements = (GetNewEngineType(w, c) != INVALID_ENGINE);
		w = (!free_wagon && w->type == VEH_TRAIN ? Train::From(w)->GetNextUnit() : NULL);
	}

	CommandCost cost = CommandCost(EXPENSES_NEW_VEHICLES, 0);
	bool nothing_to_do = true;

	if (any_replacements) {
		bool was_stopped = free_wagon || ((v->vehstatus & VS_STOPPED) != 0);

		/* Stop the vehicle */
		if (!was_stopped) cost.AddCost(CmdStartStopVehicle(v, true));
		if (cost.Failed()) return cost;

		assert(v->IsStoppedInDepot());

		/* We have to construct the new vehicle chain to test whether it is valid.
		 * Vehicle construction needs random bits, so we have to save the random seeds
		 * to prevent desyncs and to replay newgrf callbacks during DC_EXEC */
		SavedRandomSeeds saved_seeds;
		SaveRandomSeeds(&saved_seeds);
		if (free_wagon) {
			cost.AddCost(ReplaceFreeUnit(&v, flags & ~DC_EXEC, &nothing_to_do));
		} else {
			cost.AddCost(ReplaceChain(&v, flags & ~DC_EXEC, wagon_removal, &nothing_to_do));
		}
		RestoreRandomSeeds(saved_seeds);

		if (cost.Succeeded() && (flags & DC_EXEC) != 0) {
			CommandCost ret;
			if (free_wagon) {
				ret = ReplaceFreeUnit(&v, flags, &nothing_to_do);
			} else {
				ret = ReplaceChain(&v, flags, wagon_removal, &nothing_to_do);
			}
			assert(ret.Succeeded() && ret.GetCost() == cost.GetCost());
		}

		/* Restart the vehicle */
		if (!was_stopped) cost.AddCost(CmdStartStopVehicle(v, false));
	}

	if (cost.Succeeded() && nothing_to_do) cost = CommandCost(STR_ERROR_AUTOREPLACE_NOTHING_TO_DO);
	return cost;
}

/**
 * Change engine renewal parameters
 * @param tile unused
 * @param flags operation to perform
 * @param p1 packed data
 *   - bits 16-31 = engine group
 * @param p2 packed data
 *   - bits  0-15 = old engine type
 *   - bits 16-31 = new engine type
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdSetAutoReplace(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Company *c = Company::GetIfValid(_current_company);
	if (c == NULL) return CMD_ERROR;

	EngineID old_engine_type = GB(p2, 0, 16);
	EngineID new_engine_type = GB(p2, 16, 16);
	GroupID id_g = GB(p1, 16, 16);
	CommandCost cost;

	if (!Group::IsValidID(id_g) && !IsAllGroupID(id_g) && !IsDefaultGroupID(id_g)) return CMD_ERROR;
	if (!Engine::IsValidID(old_engine_type)) return CMD_ERROR;

	if (new_engine_type != INVALID_ENGINE) {
		if (!Engine::IsValidID(new_engine_type)) return CMD_ERROR;
		if (!CheckAutoreplaceValidity(old_engine_type, new_engine_type, _current_company)) return CMD_ERROR;

		cost = AddEngineReplacementForCompany(c, old_engine_type, new_engine_type, id_g, flags);
	} else {
		cost = RemoveEngineReplacementForCompany(c, old_engine_type, id_g, flags);
	}

	if ((flags & DC_EXEC) && IsLocalCompany()) InvalidateAutoreplaceWindow(old_engine_type, id_g);

	return cost;
}

