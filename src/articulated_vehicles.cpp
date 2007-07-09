/* $Id$ */

/** @file articulated_vehicles.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "functions.h"
#include "command.h"
#include "vehicle.h"
#include "articulated_vehicles.h"
#include "engine.h"
#include "train.h"
#include "roadveh.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"


uint CountArticulatedParts(EngineID engine_type)
{
	if (!HASBIT(EngInfo(engine_type)->callbackmask, CBM_ARTIC_ENGINE)) return 0;

	uint i;
	for (i = 1; i < 10; i++) {
		uint16 callback = GetVehicleCallback(CBID_VEHICLE_ARTIC_ENGINE, i, 0, engine_type, NULL);
		if (callback == CALLBACK_FAILED || callback == 0xFF) break;
	}

	return i - 1;
}

void AddArticulatedParts(Vehicle **vl, VehicleType type)
{
	const Vehicle *v = vl[0];
	Vehicle *u = vl[0];

	if (!HASBIT(EngInfo(v->engine_type)->callbackmask, CBM_ARTIC_ENGINE)) return;

	for (uint i = 1; i < 10; i++) {
		uint16 callback = GetVehicleCallback(CBID_VEHICLE_ARTIC_ENGINE, i, 0, v->engine_type, v);
		if (callback == CALLBACK_FAILED || callback == 0xFF) return;

		/* Attempt to use pre-allocated vehicles until they run out. This can happen
		 * if the callback returns different values depending on the cargo type. */
		u->next = vl[i];
		if (u->next == NULL) u->next = AllocateVehicle();
		if (u->next == NULL) return;

		u = u->next;

		EngineID engine_type = GetFirstEngineOfType(type) + GB(callback, 0, 7);
		bool flip_image = HASBIT(callback, 7);

		/* get common values from first engine */
		u->direction = v->direction;
		u->owner = v->owner;
		u->tile = v->tile;
		u->x_pos = v->x_pos;
		u->y_pos = v->y_pos;
		u->z_pos = v->z_pos;
		u->build_year = v->build_year;
		u->vehstatus = v->vehstatus & ~VS_STOPPED;

		u->cargo_subtype = 0;
		u->max_speed = 0;
		u->max_age = 0;
		u->engine_type = engine_type;
		u->value = 0;
		u->subtype = 0;
		u->cur_image = 0xAC2;
		u->random_bits = VehicleRandomBits();

		switch (type) {
			default: NOT_REACHED();

			case VEH_TRAIN: {
				const RailVehicleInfo *rvi_artic = RailVehInfo(engine_type);

				u = new (u) Train();
				u->u.rail.track = v->u.rail.track;
				u->u.rail.railtype = v->u.rail.railtype;
				u->u.rail.first_engine = v->engine_type;

				u->spritenum = rvi_artic->image_index;
				u->cargo_type = rvi_artic->cargo_type;
				u->cargo_cap = rvi_artic->capacity;

				SetArticulatedPart(u);
			} break;

			case VEH_ROAD: {
				const RoadVehicleInfo *rvi_artic = RoadVehInfo(engine_type);

				u = new (u) RoadVehicle();
				u->u.road.first_engine = v->engine_type;
				u->u.road.cached_veh_length = GetRoadVehLength(u);
				u->u.road.state = RVSB_IN_DEPOT;

				u->u.road.roadtype = v->u.road.roadtype;
				u->u.road.compatible_roadtypes = v->u.road.compatible_roadtypes;

				u->spritenum = rvi_artic->image_index;
				u->cargo_type = rvi_artic->cargo_type;
				u->cargo_cap = rvi_artic->capacity;

				SetRoadVehArticPart(u);
			} break;
		}

		if (flip_image) u->spritenum++;

		VehiclePositionChanged(u);
	}
}
