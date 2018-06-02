/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file articulated_vehicles.cpp Implementation of articulated vehicles. */

#include "stdafx.h"
#include "train.h"
#include "roadveh.h"
#include "vehicle_func.h"
#include "engine_func.h"
#include "company_func.h"
#include "newgrf.h"

#include "table/strings.h"

#include "safeguards.h"

static const uint MAX_ARTICULATED_PARTS = 100; ///< Maximum of articulated parts per vehicle, i.e. when to abort calling the articulated vehicle callback.

/**
 * Determines the next articulated part to attach
 * @param index Position in chain
 * @param front_type Front engine type
 * @param front Front engine
 * @param mirrored Returns whether the part shall be flipped.
 * @return engine to add or INVALID_ENGINE
 */
static EngineID GetNextArticulatedPart(uint index, EngineID front_type, Vehicle *front = NULL, bool *mirrored = NULL)
{
	assert(front == NULL || front->engine_type == front_type);

	const Engine *front_engine = Engine::Get(front_type);

	uint16 callback = GetVehicleCallback(CBID_VEHICLE_ARTIC_ENGINE, index, 0, front_type, front);
	if (callback == CALLBACK_FAILED) return INVALID_ENGINE;

	if (front_engine->GetGRF()->grf_version < 8) {
		/* 8 bits, bit 7 for mirroring */
		callback = GB(callback, 0, 8);
		if (callback == 0xFF) return INVALID_ENGINE;
		if (mirrored != NULL) *mirrored = HasBit(callback, 7);
		callback = GB(callback, 0, 7);
	} else {
		/* 15 bits, bit 14 for mirroring */
		if (callback == 0x7FFF) return INVALID_ENGINE;
		if (mirrored != NULL) *mirrored = HasBit(callback, 14);
		callback = GB(callback, 0, 14);
	}

	return GetNewEngineID(front_engine->GetGRF(), front_engine->type, callback);
}

/**
 * Does a NewGRF report that this should be an articulated vehicle?
 * @param engine_type The engine to check.
 * @return True iff the articulated engine callback flag is set.
 */
bool IsArticulatedEngine(EngineID engine_type)
{
	return HasBit(EngInfo(engine_type)->callback_mask, CBM_VEHICLE_ARTIC_ENGINE);
}

/**
 * Count the number of articulated parts of an engine.
 * @param engine_type The engine to get the number of parts of.
 * @param purchase_window Whether we are in the scope of the purchase window or not, i.e. whether we cannot allocate vehicles.
 * @return The number of parts.
 */
uint CountArticulatedParts(EngineID engine_type, bool purchase_window)
{
	if (!HasBit(EngInfo(engine_type)->callback_mask, CBM_VEHICLE_ARTIC_ENGINE)) return 0;

	/* If we can't allocate a vehicle now, we can't allocate it in the command
	 * either, so it doesn't matter how many articulated parts there are. */
	if (!Vehicle::CanAllocateItem()) return 0;

	Vehicle *v = NULL;
	if (!purchase_window) {
		v = new Vehicle();
		v->engine_type = engine_type;
		v->owner = _current_company;
	}

	uint i;
	for (i = 1; i < MAX_ARTICULATED_PARTS; i++) {
		if (GetNextArticulatedPart(i, engine_type, v) == INVALID_ENGINE) break;
	}

	delete v;

	return i - 1;
}


/**
 * Returns the default (non-refitted) capacity of a specific EngineID.
 * @param engine the EngineID of interest
 * @param cargo_type returns the default cargo type, if needed
 * @return capacity
 */
static inline uint16 GetVehicleDefaultCapacity(EngineID engine, CargoID *cargo_type)
{
	const Engine *e = Engine::Get(engine);
	CargoID cargo = (e->CanCarryCargo() ? e->GetDefaultCargoType() : (CargoID)CT_INVALID);
	if (cargo_type != NULL) *cargo_type = cargo;
	if (cargo == CT_INVALID) return 0;
	return e->GetDisplayDefaultCapacity();
}

/**
 * Returns all cargoes a vehicle can carry.
 * @param engine the EngineID of interest
 * @param include_initial_cargo_type if true the default cargo type of the vehicle is included; if false only the refit_mask
 * @return bit set of CargoIDs
 */
static inline CargoTypes GetAvailableVehicleCargoTypes(EngineID engine, bool include_initial_cargo_type)
{
	const Engine *e = Engine::Get(engine);
	if (!e->CanCarryCargo()) return 0;

	CargoTypes cargoes = e->info.refit_mask;

	if (include_initial_cargo_type) {
		SetBit(cargoes, e->GetDefaultCargoType());
	}

	return cargoes;
}

/**
 * Get the capacity of the parts of a given engine.
 * @param engine The engine to get the capacities from.
 * @return The cargo capacities.
 */
CargoArray GetCapacityOfArticulatedParts(EngineID engine)
{
	CargoArray capacity;
	const Engine *e = Engine::Get(engine);

	CargoID cargo_type;
	uint16 cargo_capacity = GetVehicleDefaultCapacity(engine, &cargo_type);
	if (cargo_type < NUM_CARGO) capacity[cargo_type] = cargo_capacity;

	if (!e->IsGroundVehicle()) return capacity;

	if (!HasBit(e->info.callback_mask, CBM_VEHICLE_ARTIC_ENGINE)) return capacity;

	for (uint i = 1; i < MAX_ARTICULATED_PARTS; i++) {
		EngineID artic_engine = GetNextArticulatedPart(i, engine);
		if (artic_engine == INVALID_ENGINE) break;

		cargo_capacity = GetVehicleDefaultCapacity(artic_engine, &cargo_type);
		if (cargo_type < NUM_CARGO) capacity[cargo_type] += cargo_capacity;
	}

	return capacity;
}

/**
 * Get the default cargoes and refits of an articulated vehicle.
 * The refits are linked to a cargo rather than an articulated part to prevent a long list of parts.
 * @param engine Model to investigate.
 * @param[out] cargoes Total amount of units that can be transported, summed by cargo.
 * @param[out] refits Whether a (possibly partial) refit for each cargo is possible.
 */
void GetArticulatedVehicleCargoesAndRefits(EngineID engine, CargoArray *cargoes, CargoTypes *refits)
{
	cargoes->Clear();
	*refits = 0;

	const Engine *e = Engine::Get(engine);

	CargoID cargo_type;
	uint16 cargo_capacity = GetVehicleDefaultCapacity(engine, &cargo_type);
	if (cargo_type < NUM_CARGO && cargo_capacity > 0) {
		(*cargoes)[cargo_type] += cargo_capacity;
		if (IsEngineRefittable(engine)) SetBit(*refits, cargo_type);
	}

	if (!e->IsGroundVehicle() || !HasBit(e->info.callback_mask, CBM_VEHICLE_ARTIC_ENGINE)) return;

	for (uint i = 1; i < MAX_ARTICULATED_PARTS; i++) {
		EngineID artic_engine = GetNextArticulatedPart(i, engine);
		if (artic_engine == INVALID_ENGINE) break;

		cargo_capacity = GetVehicleDefaultCapacity(artic_engine, &cargo_type);
		if (cargo_type < NUM_CARGO && cargo_capacity > 0) {
			(*cargoes)[cargo_type] += cargo_capacity;
			if (IsEngineRefittable(artic_engine)) SetBit(*refits, cargo_type);
		}
	}
}

/**
 * Checks whether any of the articulated parts is refittable
 * @param engine the first part
 * @return true if refittable
 */
bool IsArticulatedVehicleRefittable(EngineID engine)
{
	if (IsEngineRefittable(engine)) return true;

	const Engine *e = Engine::Get(engine);
	if (!e->IsGroundVehicle()) return false;

	if (!HasBit(e->info.callback_mask, CBM_VEHICLE_ARTIC_ENGINE)) return false;

	for (uint i = 1; i < MAX_ARTICULATED_PARTS; i++) {
		EngineID artic_engine = GetNextArticulatedPart(i, engine);
		if (artic_engine == INVALID_ENGINE) break;

		if (IsEngineRefittable(artic_engine)) return true;
	}

	return false;
}

/**
 * Merges the refit_masks of all articulated parts.
 * @param engine the first part
 * @param include_initial_cargo_type if true the default cargo type of the vehicle is included; if false only the refit_mask
 * @param union_mask returns bit mask of CargoIDs which are a refit option for at least one articulated part
 * @param intersection_mask returns bit mask of CargoIDs which are a refit option for every articulated part (with default capacity > 0)
 */
void GetArticulatedRefitMasks(EngineID engine, bool include_initial_cargo_type, CargoTypes *union_mask, CargoTypes *intersection_mask)
{
	const Engine *e = Engine::Get(engine);
	CargoTypes veh_cargoes = GetAvailableVehicleCargoTypes(engine, include_initial_cargo_type);
	*union_mask = veh_cargoes;
	*intersection_mask = (veh_cargoes != 0) ? veh_cargoes : ALL_CARGOTYPES;

	if (!e->IsGroundVehicle()) return;
	if (!HasBit(e->info.callback_mask, CBM_VEHICLE_ARTIC_ENGINE)) return;

	for (uint i = 1; i < MAX_ARTICULATED_PARTS; i++) {
		EngineID artic_engine = GetNextArticulatedPart(i, engine);
		if (artic_engine == INVALID_ENGINE) break;

		veh_cargoes = GetAvailableVehicleCargoTypes(artic_engine, include_initial_cargo_type);
		*union_mask |= veh_cargoes;
		if (veh_cargoes != 0) *intersection_mask &= veh_cargoes;
	}
}

/**
 * Ors the refit_masks of all articulated parts.
 * @param engine the first part
 * @param include_initial_cargo_type if true the default cargo type of the vehicle is included; if false only the refit_mask
 * @return bit mask of CargoIDs which are a refit option for at least one articulated part
 */
CargoTypes GetUnionOfArticulatedRefitMasks(EngineID engine, bool include_initial_cargo_type)
{
	CargoTypes union_mask, intersection_mask;
	GetArticulatedRefitMasks(engine, include_initial_cargo_type, &union_mask, &intersection_mask);
	return union_mask;
}

/**
 * Ands the refit_masks of all articulated parts.
 * @param engine the first part
 * @param include_initial_cargo_type if true the default cargo type of the vehicle is included; if false only the refit_mask
 * @return bit mask of CargoIDs which are a refit option for every articulated part (with default capacity > 0)
 */
CargoTypes GetIntersectionOfArticulatedRefitMasks(EngineID engine, bool include_initial_cargo_type)
{
	CargoTypes union_mask, intersection_mask;
	GetArticulatedRefitMasks(engine, include_initial_cargo_type, &union_mask, &intersection_mask);
	return intersection_mask;
}


/**
 * Tests if all parts of an articulated vehicle are refitted to the same cargo.
 * Note: Vehicles not carrying anything are ignored
 * @param v the first vehicle in the chain
 * @param cargo_type returns the common CargoID if needed. (CT_INVALID if no part is carrying something or they are carrying different things)
 * @return true if some parts are carrying different cargoes, false if all parts are carrying the same (nothing is also the same)
 */
bool IsArticulatedVehicleCarryingDifferentCargoes(const Vehicle *v, CargoID *cargo_type)
{
	CargoID first_cargo = CT_INVALID;

	do {
		if (v->cargo_type != CT_INVALID && v->GetEngine()->CanCarryCargo()) {
			if (first_cargo == CT_INVALID) first_cargo = v->cargo_type;
			if (first_cargo != v->cargo_type) {
				if (cargo_type != NULL) *cargo_type = CT_INVALID;
				return true;
			}
		}

		v = v->HasArticulatedPart() ? v->GetNextArticulatedPart() : NULL;
	} while (v != NULL);

	if (cargo_type != NULL) *cargo_type = first_cargo;
	return false;
}

/**
 * Checks whether the specs of freshly build articulated vehicles are consistent with the information specified in the purchase list.
 * Only essential information is checked to leave room for magic tricks/workarounds to grfcoders.
 * It checks:
 *   For autoreplace/-renew:
 *    - Default cargo type (without capacity)
 *    - intersection and union of refit masks.
 */
void CheckConsistencyOfArticulatedVehicle(const Vehicle *v)
{
	const Engine *engine = v->GetEngine();

	CargoTypes purchase_refit_union, purchase_refit_intersection;
	GetArticulatedRefitMasks(v->engine_type, true, &purchase_refit_union, &purchase_refit_intersection);
	CargoArray purchase_default_capacity = GetCapacityOfArticulatedParts(v->engine_type);

	CargoTypes real_refit_union = 0;
	CargoTypes real_refit_intersection = ALL_CARGOTYPES;
	CargoArray real_default_capacity;

	do {
		CargoTypes refit_mask = GetAvailableVehicleCargoTypes(v->engine_type, true);
		real_refit_union |= refit_mask;
		if (refit_mask != 0) real_refit_intersection &= refit_mask;

		assert(v->cargo_type < NUM_CARGO);
		real_default_capacity[v->cargo_type] += v->cargo_cap;

		v = v->HasArticulatedPart() ? v->GetNextArticulatedPart() : NULL;
	} while (v != NULL);

	/* Check whether the vehicle carries more cargoes than expected */
	bool carries_more = false;
	for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
		if (real_default_capacity[cid] != 0 && purchase_default_capacity[cid] == 0) {
			carries_more = true;
			break;
		}
	}

	/* show a warning once for each GRF after each game load */
	if (real_refit_union != purchase_refit_union || real_refit_intersection != purchase_refit_intersection || carries_more) {
		ShowNewGrfVehicleError(engine->index, STR_NEWGRF_BUGGY, STR_NEWGRF_BUGGY_ARTICULATED_CARGO, GBUG_VEH_REFIT, false);
	}
}

/**
 * Add the remaining articulated parts to the given vehicle.
 * @param first The head of the articulated bit.
 */
void AddArticulatedParts(Vehicle *first)
{
	VehicleType type = first->type;
	if (!HasBit(EngInfo(first->engine_type)->callback_mask, CBM_VEHICLE_ARTIC_ENGINE)) return;

	Vehicle *v = first;
	for (uint i = 1; i < MAX_ARTICULATED_PARTS; i++) {
		bool flip_image;
		EngineID engine_type = GetNextArticulatedPart(i, first->engine_type, first, &flip_image);
		if (engine_type == INVALID_ENGINE) return;

		/* In the (very rare) case the GRF reported wrong number of articulated parts
		 * and we run out of available vehicles, bail out. */
		if (!Vehicle::CanAllocateItem()) return;

		GroundVehicleCache *gcache = v->GetGroundVehicleCache();
		gcache->first_engine = v->engine_type; // Needs to be set before first callback

		const Engine *e_artic = Engine::Get(engine_type);
		switch (type) {
			default: NOT_REACHED();

			case VEH_TRAIN: {
				Train *front = Train::From(first);
				Train *t = new Train();
				v->SetNext(t);
				v = t;

				t->subtype = 0;
				t->track = front->track;
				t->railtype = front->railtype;

				t->spritenum = e_artic->u.rail.image_index;
				if (e_artic->CanCarryCargo()) {
					t->cargo_type = e_artic->GetDefaultCargoType();
					t->cargo_cap = e_artic->u.rail.capacity;  // Callback 36 is called when the consist is finished
				} else {
					t->cargo_type = front->cargo_type; // Needed for livery selection
					t->cargo_cap = 0;
				}
				t->refit_cap = 0;

				t->SetArticulatedPart();
				break;
			}

			case VEH_ROAD: {
				RoadVehicle *front = RoadVehicle::From(first);
				RoadVehicle *rv = new RoadVehicle();
				v->SetNext(rv);
				v = rv;

				rv->subtype = 0;
				gcache->cached_veh_length = VEHICLE_LENGTH; // Callback is called when the consist is finished
				rv->state = RVSB_IN_DEPOT;

				rv->rtid = front->rtid;
				rv->compatible_subtypes = front->compatible_subtypes;

				rv->spritenum = e_artic->u.road.image_index;
				if (e_artic->CanCarryCargo()) {
					rv->cargo_type = e_artic->GetDefaultCargoType();
					rv->cargo_cap = e_artic->u.road.capacity;  // Callback 36 is called when the consist is finished
				} else {
					rv->cargo_type = front->cargo_type; // Needed for livery selection
					rv->cargo_cap = 0;
				}
				rv->refit_cap = 0;

				rv->SetArticulatedPart();
				break;
			}
		}

		/* get common values from first engine */
		v->direction = first->direction;
		v->owner = first->owner;
		v->tile = first->tile;
		v->x_pos = first->x_pos;
		v->y_pos = first->y_pos;
		v->z_pos = first->z_pos;
		v->date_of_last_service = first->date_of_last_service;
		v->build_year = first->build_year;
		v->vehstatus = first->vehstatus & ~VS_STOPPED;

		v->cargo_subtype = 0;
		v->max_age = 0;
		v->engine_type = engine_type;
		v->value = 0;
		v->sprite_seq.Set(SPR_IMG_QUERY);
		v->random_bits = VehicleRandomBits();

		if (flip_image) v->spritenum++;

		v->UpdatePosition();
	}
}
