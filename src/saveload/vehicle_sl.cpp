/* $Id$ */

/** @file vehicle_sl.cpp Code handling saving and loading of vehicles */

#include "../stdafx.h"
#include "../vehicle_func.h"
#include "../train.h"
#include "../roadveh.h"
#include "../ship.h"
#include "../aircraft.h"
#include "../effectvehicle_base.h"

#include "saveload.h"

#include <map>

/*
 * Link front and rear multiheaded engines to each other
 * This is done when loading a savegame
 */
void ConnectMultiheadedTrains()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN) {
			v->u.rail.other_multiheaded_part = NULL;
		}
	}

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN && (IsFrontEngine(v) || IsFreeWagon(v))) {
			/* Two ways to associate multiheaded parts to each other:
			 * sequential-matching: Trains shall be arranged to look like <..>..<..>..<..>..
			 * bracket-matching:    Free vehicle chains shall be arranged to look like ..<..<..>..<..>..>..
			 *
			 * Note: Old savegames might contain chains which do not comply with these rules, e.g.
			 *   - the front and read parts have invalid orders
			 *   - different engine types might be combined
			 *   - there might be different amounts of front and rear parts.
			 *
			 * Note: The multiheaded parts need to be matched exactly like they are matched on the server, else desyncs will occur.
			 *   This is why two matching strategies are needed.
			 */

			bool sequential_matching = IsFrontEngine(v);

			for (Vehicle *u = v; u != NULL; u = GetNextVehicle(u)) {
				if (u->u.rail.other_multiheaded_part != NULL) continue; // we already linked this one

				if (IsMultiheaded(u)) {
					if (!IsTrainEngine(u)) {
						/* we got a rear car without a front car. We will convert it to a front one */
						SetTrainEngine(u);
						u->spritenum--;
					}

					/* Find a matching back part */
					EngineID eid = u->engine_type;
					Vehicle *w;
					if (sequential_matching) {
						for (w = GetNextVehicle(u); w != NULL; w = GetNextVehicle(w)) {
							if (w->engine_type != eid || w->u.rail.other_multiheaded_part != NULL || !IsMultiheaded(w)) continue;

							/* we found a car to partner with this engine. Now we will make sure it face the right way */
							if (IsTrainEngine(w)) {
								ClearTrainEngine(w);
								w->spritenum++;
							}
							break;
						}
					} else {
						uint stack_pos = 0;
						for (w = GetNextVehicle(u); w != NULL; w = GetNextVehicle(w)) {
							if (w->engine_type != eid || w->u.rail.other_multiheaded_part != NULL || !IsMultiheaded(w)) continue;

							if (IsTrainEngine(w)) {
								stack_pos++;
							} else {
								if (stack_pos == 0) break;
								stack_pos--;
							}
						}
					}

					if (w != NULL) {
						w->u.rail.other_multiheaded_part = u;
						u->u.rail.other_multiheaded_part = w;
					} else {
						/* we got a front car and no rear cars. We will fake this one for forget that it should have been multiheaded */
						ClearMultiheaded(u);
					}
				}
			}
		}
	}
}

/**
 *  Converts all trains to the new subtype format introduced in savegame 16.2
 *  It also links multiheaded engines or make them forget they are multiheaded if no suitable partner is found
 */
void ConvertOldMultiheadToNew()
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN) {
			SetBit(v->subtype, 7); // indicates that it's the old format and needs to be converted in the next loop
		}
	}

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN) {
			if (HasBit(v->subtype, 7) && ((v->subtype & ~0x80) == 0 || (v->subtype & ~0x80) == 4)) {
				for (Vehicle *u = v; u != NULL; u = u->Next()) {
					const RailVehicleInfo *rvi = RailVehInfo(u->engine_type);

					ClrBit(u->subtype, 7);
					switch (u->subtype) {
						case 0: // TS_Front_Engine
							if (rvi->railveh_type == RAILVEH_MULTIHEAD) SetMultiheaded(u);
							SetFrontEngine(u);
							SetTrainEngine(u);
							break;

						case 1: // TS_Artic_Part
							u->subtype = 0;
							SetArticulatedPart(u);
							break;

						case 2: // TS_Not_First
							u->subtype = 0;
							if (rvi->railveh_type == RAILVEH_WAGON) {
								/* normal wagon */
								SetTrainWagon(u);
								break;
							}
							if (rvi->railveh_type == RAILVEH_MULTIHEAD && rvi->image_index == u->spritenum - 1) {
								/* rear end of a multiheaded engine */
								SetMultiheaded(u);
								break;
							}
							if (rvi->railveh_type == RAILVEH_MULTIHEAD) SetMultiheaded(u);
							SetTrainEngine(u);
							break;

						case 4: // TS_Free_Car
							u->subtype = 0;
							SetTrainWagon(u);
							SetFreeWagon(u);
							break;
						default: NOT_REACHED(); break;
					}
				}
			}
		}
	}
}


/** need to be called to load aircraft from old version */
void UpdateOldAircraft()
{
	/* set airport_flags to 0 for all airports just to be sure */
	Station *st;
	FOR_ALL_STATIONS(st) {
		st->airport_flags = 0; // reset airport
	}

	Vehicle *v_oldstyle;
	FOR_ALL_VEHICLES(v_oldstyle) {
		/* airplane has another vehicle with subtype 4 (shadow), helicopter also has 3 (rotor)
		 * skip those */
		if (v_oldstyle->type == VEH_AIRCRAFT && IsNormalAircraft(v_oldstyle)) {
			/* airplane in terminal stopped doesn't hurt anyone, so goto next */
			if (v_oldstyle->vehstatus & VS_STOPPED && v_oldstyle->u.air.state == 0) {
				v_oldstyle->u.air.state = HANGAR;
				continue;
			}

			AircraftLeaveHangar(v_oldstyle); // make airplane visible if it was in a depot for example
			v_oldstyle->vehstatus &= ~VS_STOPPED; // make airplane moving
			v_oldstyle->cur_speed = v_oldstyle->max_speed; // so aircraft don't have zero speed while in air
			if (!v_oldstyle->current_order.IsType(OT_GOTO_STATION) && !v_oldstyle->current_order.IsType(OT_GOTO_DEPOT)) {
				/* reset current order so aircraft doesn't have invalid "station-only" order */
				v_oldstyle->current_order.MakeDummy();
			}
			v_oldstyle->u.air.state = FLYING;
			AircraftNextAirportPos_and_Order(v_oldstyle); // move it to the entry point of the airport
			GetNewVehiclePosResult gp = GetNewVehiclePos(v_oldstyle);
			v_oldstyle->tile = 0; // aircraft in air is tile=0

			/* correct speed of helicopter-rotors */
			if (v_oldstyle->subtype == AIR_HELICOPTER) v_oldstyle->Next()->Next()->cur_speed = 32;

			/* set new position x,y,z */
			SetAircraftPosition(v_oldstyle, gp.x, gp.y, GetAircraftFlyingAltitude(v_oldstyle));
		}
	}
}

/**
 * Check all vehicles to ensure their engine type is valid
 * for the currently loaded NewGRFs (that includes none...)
 * This only makes a difference if NewGRFs are missing, otherwise
 * all vehicles will be valid. This does not make such a game
 * playable, it only prevents crash.
 */
static void CheckValidVehicles()
{
	uint total_engines = GetEnginePoolSize();
	EngineID first_engine[4] = { INVALID_ENGINE, INVALID_ENGINE, INVALID_ENGINE, INVALID_ENGINE };

	Engine *e;
	FOR_ALL_ENGINES_OF_TYPE(e, VEH_TRAIN) { first_engine[VEH_TRAIN] = e->index; break; }
	FOR_ALL_ENGINES_OF_TYPE(e, VEH_ROAD) { first_engine[VEH_ROAD] = e->index; break; }
	FOR_ALL_ENGINES_OF_TYPE(e, VEH_SHIP) { first_engine[VEH_SHIP] = e->index; break; }
	FOR_ALL_ENGINES_OF_TYPE(e, VEH_AIRCRAFT) { first_engine[VEH_AIRCRAFT] = e->index; break; }

	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		/* Test if engine types match */
		switch (v->type) {
			case VEH_TRAIN:
			case VEH_ROAD:
			case VEH_SHIP:
			case VEH_AIRCRAFT:
				if (v->engine_type >= total_engines || v->type != GetEngine(v->engine_type)->type) {
					v->engine_type = first_engine[v->type];
				}
				break;

			default:
				break;
		}
	}
}

/** Called after load to update coordinates */
void AfterLoadVehicles(bool part_of_load)
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		/* Reinstate the previous pointer */
		if (v->Next() != NULL) v->Next()->previous = v;
		if (v->NextShared() != NULL) v->NextShared()->previous_shared = v;

		v->UpdateDeltaXY(v->direction);

		if (part_of_load) v->fill_percent_te_id = INVALID_TE_ID;
		v->first = NULL;
		if (v->type == VEH_TRAIN) v->u.rail.first_engine = INVALID_ENGINE;
		if (v->type == VEH_ROAD)  v->u.road.first_engine = INVALID_ENGINE;

		v->cargo.InvalidateCache();
	}

	/* AfterLoadVehicles may also be called in case of NewGRF reload, in this
	 * case we may not convert orders again. */
	if (part_of_load) {
		/* Create shared vehicle chain for very old games (pre 5,2) and create
		 * OrderList from shared vehicle chains. For this to work correctly, the
		 * following conditions must be fulfilled:
		 * a) both next_shared and previous_shared are not set for pre 5,2 games
		 * b) both next_shared and previous_shared are set for later games
		 */
		std::map<Order*, OrderList*> mapping;

		FOR_ALL_VEHICLES(v) {
			if (v->orders.old != NULL) {
				if (CheckSavegameVersion(105)) { // Pre-105 didn't save an OrderList
					if (mapping[v->orders.old] == NULL) {
						/* This adds the whole shared vehicle chain for case b */
						v->orders.list = mapping[v->orders.old] = new OrderList(v->orders.old, v);
					} else {
						v->orders.list = mapping[v->orders.old];
						/* For old games (case a) we must create the shared vehicle chain */
						if (CheckSavegameVersionOldStyle(5, 2)) {
							v->AddToShared(v->orders.list->GetFirstSharedVehicle());
						}
					}
				} else { // OrderList was saved as such, only recalculate not saved values
					if (v->PreviousShared() == NULL) {
						new (v->orders.list) OrderList(v->orders.list->GetFirstOrder(), v);
					}
				}
			}
		}
	}

	FOR_ALL_VEHICLES(v) {
		/* Fill the first pointers */
		if (v->Previous() == NULL) {
			for (Vehicle *u = v; u != NULL; u = u->Next()) {
				u->first = v;
			}
		}
	}

	CheckValidVehicles();

	FOR_ALL_VEHICLES(v) {
		assert(v->first != NULL);

		if (v->type == VEH_TRAIN && (IsFrontEngine(v) || IsFreeWagon(v))) {
			if (IsFrontEngine(v)) v->u.rail.last_speed = v->cur_speed; // update displayed train speed
			TrainConsistChanged(v, false);
		} else if (v->type == VEH_ROAD && IsRoadVehFront(v)) {
			RoadVehUpdateCache(v);
		}
	}

	/* Stop non-front engines */
	if (CheckSavegameVersion(112)) {
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_TRAIN && !IsFrontEngine(v)) {
				if (IsTrainEngine(v)) v->vehstatus |= VS_STOPPED;
				/* cur_speed is now relevant for non-front parts - nonzero breaks
				 * moving-wagons-inside-depot- and autoreplace- code */
				v->cur_speed = 0;
			}
			/* trains weren't stopping gradually in old OTTD versions (and TTO/TTD)
			 * other vehicle types didn't have zero speed while stopped (even in 'recent' OTTD versions) */
			if ((v->vehstatus & VS_STOPPED) && (v->type != VEH_TRAIN || CheckSavegameVersionOldStyle(2, 1))) {
				v->cur_speed = 0;
			}
		}
	}

	FOR_ALL_VEHICLES(v) {
		switch (v->type) {
			case VEH_ROAD:
				v->u.road.roadtype = HasBit(EngInfo(v->First()->engine_type)->misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD;
				v->u.road.compatible_roadtypes = RoadTypeToRoadTypes(v->u.road.roadtype);
				/* FALL THROUGH */
			case VEH_TRAIN:
			case VEH_SHIP:
				v->cur_image = v->GetImage(v->direction);
				break;

			case VEH_AIRCRAFT:
				if (IsNormalAircraft(v)) {
					v->cur_image = v->GetImage(v->direction);

					/* The plane's shadow will have the same image as the plane */
					Vehicle *shadow = v->Next();
					shadow->cur_image = v->cur_image;

					/* In the case of a helicopter we will update the rotor sprites */
					if (v->subtype == AIR_HELICOPTER) {
						Vehicle *rotor = shadow->Next();
						rotor->cur_image = GetRotorImage(v);
					}

					UpdateAircraftCache(v);
				}
				break;
			default: break;
		}

		v->coord.left = INVALID_COORD;
		VehicleMove(v, false);
	}
}

static uint8  _cargo_days;
static uint16 _cargo_source;
static uint32 _cargo_source_xy;
static uint16 _cargo_count;
static uint16 _cargo_paid_for;
static Money  _cargo_feeder_share;
static uint32 _cargo_loaded_at_xy;

/**
 * Make it possible to make the saveload tables "friends" of other classes.
 * @param vt the vehicle type. Can be VEH_END for the common vehicle description data
 * @return the saveload description
 */
const SaveLoad *GetVehicleDescription(VehicleType vt)
{
	/** Save and load of vehicles */
	static const SaveLoad _common_veh_desc[] = {
		     SLE_VAR(Vehicle, subtype,               SLE_UINT8),

		     SLE_REF(Vehicle, next,                  REF_VEHICLE_OLD),
		 SLE_CONDVAR(Vehicle, name,                  SLE_NAME,                     0,  83),
		 SLE_CONDSTR(Vehicle, name,                  SLE_STR, 0,                  84, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, unitnumber,            SLE_FILE_U8  | SLE_VAR_U16,   0,   7),
		 SLE_CONDVAR(Vehicle, unitnumber,            SLE_UINT16,                   8, SL_MAX_VERSION),
		     SLE_VAR(Vehicle, owner,                 SLE_UINT8),
		 SLE_CONDVAR(Vehicle, tile,                  SLE_FILE_U16 | SLE_VAR_U32,   0,   5),
		 SLE_CONDVAR(Vehicle, tile,                  SLE_UINT32,                   6, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, dest_tile,             SLE_FILE_U16 | SLE_VAR_U32,   0,   5),
		 SLE_CONDVAR(Vehicle, dest_tile,             SLE_UINT32,                   6, SL_MAX_VERSION),

		 SLE_CONDVAR(Vehicle, x_pos,                 SLE_FILE_U16 | SLE_VAR_U32,   0,   5),
		 SLE_CONDVAR(Vehicle, x_pos,                 SLE_UINT32,                   6, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, y_pos,                 SLE_FILE_U16 | SLE_VAR_U32,   0,   5),
		 SLE_CONDVAR(Vehicle, y_pos,                 SLE_UINT32,                   6, SL_MAX_VERSION),
		     SLE_VAR(Vehicle, z_pos,                 SLE_UINT8),
		     SLE_VAR(Vehicle, direction,             SLE_UINT8),

		SLE_CONDNULL(2,                                                            0,  57),
		     SLE_VAR(Vehicle, spritenum,             SLE_UINT8),
		SLE_CONDNULL(5,                                                            0,  57),
		     SLE_VAR(Vehicle, engine_type,           SLE_UINT16),

		     SLE_VAR(Vehicle, max_speed,             SLE_UINT16),
		     SLE_VAR(Vehicle, cur_speed,             SLE_UINT16),
		     SLE_VAR(Vehicle, subspeed,              SLE_UINT8),
		     SLE_VAR(Vehicle, acceleration,          SLE_UINT8),
		     SLE_VAR(Vehicle, progress,              SLE_UINT8),

		     SLE_VAR(Vehicle, vehstatus,             SLE_UINT8),
		 SLE_CONDVAR(Vehicle, last_station_visited,  SLE_FILE_U8  | SLE_VAR_U16,   0,   4),
		 SLE_CONDVAR(Vehicle, last_station_visited,  SLE_UINT16,                   5, SL_MAX_VERSION),

		     SLE_VAR(Vehicle, cargo_type,            SLE_UINT8),
		 SLE_CONDVAR(Vehicle, cargo_subtype,         SLE_UINT8,                   35, SL_MAX_VERSION),
		SLEG_CONDVAR(         _cargo_days,           SLE_UINT8,                    0,  67),
		SLEG_CONDVAR(         _cargo_source,         SLE_FILE_U8  | SLE_VAR_U16,   0,   6),
		SLEG_CONDVAR(         _cargo_source,         SLE_UINT16,                   7,  67),
		SLEG_CONDVAR(         _cargo_source_xy,      SLE_UINT32,                  44,  67),
		     SLE_VAR(Vehicle, cargo_cap,             SLE_UINT16),
		SLEG_CONDVAR(         _cargo_count,          SLE_UINT16,                   0,  67),
		 SLE_CONDLST(Vehicle, cargo,                 REF_CARGO_PACKET,            68, SL_MAX_VERSION),

		     SLE_VAR(Vehicle, day_counter,           SLE_UINT8),
		     SLE_VAR(Vehicle, tick_counter,          SLE_UINT8),
		 SLE_CONDVAR(Vehicle, running_ticks,         SLE_UINT8,                   88, SL_MAX_VERSION),

		     SLE_VAR(Vehicle, cur_order_index,       SLE_UINT8),
		/* num_orders is now part of OrderList and is not saved but counted */
		SLE_CONDNULL(1,                                                            0, 104),

		/* This next line is for version 4 and prior compatibility.. it temporarily reads
		 type and flags (which were both 4 bits) into type. Later on this is
		 converted correctly */
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, type),           SLE_UINT8,                    0,   4),
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest),           SLE_FILE_U8  | SLE_VAR_U16,   0,   4),

		/* Orders for version 5 and on */
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, type),           SLE_UINT8,                    5, SL_MAX_VERSION),
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, flags),          SLE_UINT8,                    5, SL_MAX_VERSION),
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest),           SLE_UINT16,                   5, SL_MAX_VERSION),

		/* Refit in current order */
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, refit_cargo),    SLE_UINT8,                   36, SL_MAX_VERSION),
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, refit_subtype),  SLE_UINT8,                   36, SL_MAX_VERSION),

		/* Timetable in current order */
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, wait_time),      SLE_UINT16,                  67, SL_MAX_VERSION),
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, travel_time),    SLE_UINT16,                  67, SL_MAX_VERSION),

		 SLE_CONDREF(Vehicle, orders,                REF_ORDER,                    0, 104),
		 SLE_CONDREF(Vehicle, orders,                REF_ORDERLIST,              105, SL_MAX_VERSION),

		 SLE_CONDVAR(Vehicle, age,                   SLE_FILE_U16 | SLE_VAR_I32,   0,  30),
		 SLE_CONDVAR(Vehicle, age,                   SLE_INT32,                   31, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, max_age,               SLE_FILE_U16 | SLE_VAR_I32,   0,  30),
		 SLE_CONDVAR(Vehicle, max_age,               SLE_INT32,                   31, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, date_of_last_service,  SLE_FILE_U16 | SLE_VAR_I32,   0,  30),
		 SLE_CONDVAR(Vehicle, date_of_last_service,  SLE_INT32,                   31, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, service_interval,      SLE_FILE_U16 | SLE_VAR_I32,   0,  30),
		 SLE_CONDVAR(Vehicle, service_interval,      SLE_INT32,                   31, SL_MAX_VERSION),
		     SLE_VAR(Vehicle, reliability,           SLE_UINT16),
		     SLE_VAR(Vehicle, reliability_spd_dec,   SLE_UINT16),
		     SLE_VAR(Vehicle, breakdown_ctr,         SLE_UINT8),
		     SLE_VAR(Vehicle, breakdown_delay,       SLE_UINT8),
		     SLE_VAR(Vehicle, breakdowns_since_last_service, SLE_UINT8),
		     SLE_VAR(Vehicle, breakdown_chance,      SLE_UINT8),
		 SLE_CONDVAR(Vehicle, build_year,            SLE_FILE_U8 | SLE_VAR_I32,    0,  30),
		 SLE_CONDVAR(Vehicle, build_year,            SLE_INT32,                   31, SL_MAX_VERSION),

		     SLE_VAR(Vehicle, load_unload_time_rem,  SLE_UINT16),
		SLEG_CONDVAR(         _cargo_paid_for,       SLE_UINT16,                  45, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, vehicle_flags,         SLE_UINT8,                   40, SL_MAX_VERSION),

		 SLE_CONDVAR(Vehicle, profit_this_year,      SLE_FILE_I32 | SLE_VAR_I64,   0,  64),
		 SLE_CONDVAR(Vehicle, profit_this_year,      SLE_INT64,                   65, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, profit_last_year,      SLE_FILE_I32 | SLE_VAR_I64,   0,  64),
		 SLE_CONDVAR(Vehicle, profit_last_year,      SLE_INT64,                   65, SL_MAX_VERSION),
		SLEG_CONDVAR(         _cargo_feeder_share,   SLE_FILE_I32 | SLE_VAR_I64,  51,  64),
		SLEG_CONDVAR(         _cargo_feeder_share,   SLE_INT64,                   65,  67),
		SLEG_CONDVAR(         _cargo_loaded_at_xy,   SLE_UINT32,                  51,  67),
		 SLE_CONDVAR(Vehicle, value,                 SLE_FILE_I32 | SLE_VAR_I64,   0,  64),
		 SLE_CONDVAR(Vehicle, value,                 SLE_INT64,                   65, SL_MAX_VERSION),

		 SLE_CONDVAR(Vehicle, random_bits,           SLE_UINT8,                    2, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, waiting_triggers,      SLE_UINT8,                    2, SL_MAX_VERSION),

		 SLE_CONDREF(Vehicle, next_shared,           REF_VEHICLE,                  2, SL_MAX_VERSION),
		SLE_CONDNULL(2,                                                            2,  68),
		SLE_CONDNULL(4,                                                           69, 100),

		 SLE_CONDVAR(Vehicle, group_id,              SLE_UINT16,                  60, SL_MAX_VERSION),

		 SLE_CONDVAR(Vehicle, current_order_time,    SLE_UINT32,                  67, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, lateness_counter,      SLE_INT32,                   67, SL_MAX_VERSION),

		/* reserve extra space in savegame here. (currently 10 bytes) */
		SLE_CONDNULL(10,                                                           2, SL_MAX_VERSION),

		     SLE_END()
	};


	static const SaveLoad _train_desc[] = {
		SLE_WRITEBYTE(Vehicle, type, VEH_TRAIN),
		SLE_VEH_INCLUDEX(),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, crash_anim_pos),      SLE_UINT16),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, force_proceed),       SLE_UINT8),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, railtype),            SLE_UINT8),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, track),               SLE_UINT8),

		SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, flags),               SLE_FILE_U8  | SLE_VAR_U16,   2,  99),
		SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, flags),               SLE_UINT16,                 100, SL_MAX_VERSION),
		SLE_CONDNULL(2, 2, 59),

		SLE_CONDNULL(2, 2, 19),
		/* reserve extra space in savegame here. (currently 11 bytes) */
		SLE_CONDNULL(11, 2, SL_MAX_VERSION),

		     SLE_END()
	};

	static const SaveLoad _roadveh_desc[] = {
		SLE_WRITEBYTE(Vehicle, type, VEH_ROAD),
		SLE_VEH_INCLUDEX(),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, state),                SLE_UINT8),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, frame),                SLE_UINT8),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, blocked_ctr),          SLE_UINT16),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, overtaking),           SLE_UINT8),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, overtaking_ctr),       SLE_UINT8),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, crashed_ctr),          SLE_UINT16),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, reverse_ctr),          SLE_UINT8),

		SLE_CONDREFX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, slot),                 REF_ROADSTOPS,                6, SL_MAX_VERSION),
		SLE_CONDNULL(1,                                                            6, SL_MAX_VERSION),
		SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, slot_age),             SLE_UINT8,                    6, SL_MAX_VERSION),
		/* reserve extra space in savegame here. (currently 16 bytes) */
		SLE_CONDNULL(16,                                                           2, SL_MAX_VERSION),

		     SLE_END()
	};

	static const SaveLoad _ship_desc[] = {
		SLE_WRITEBYTE(Vehicle, type, VEH_SHIP),
		SLE_VEH_INCLUDEX(),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleShip, state),  SLE_UINT8),

		/* reserve extra space in savegame here. (currently 16 bytes) */
		SLE_CONDNULL(16, 2, SL_MAX_VERSION),

		     SLE_END()
	};

	static const SaveLoad _aircraft_desc[] = {
		SLE_WRITEBYTE(Vehicle, type, VEH_AIRCRAFT),
		SLE_VEH_INCLUDEX(),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, crashed_counter),       SLE_UINT16),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, pos),                   SLE_UINT8),

		SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, targetairport),         SLE_FILE_U8  | SLE_VAR_U16,   0, 4),
		SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, targetairport),         SLE_UINT16,                   5, SL_MAX_VERSION),

		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, state),                 SLE_UINT8),

		SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, previous_pos),          SLE_UINT8,                    2, SL_MAX_VERSION),

		/* reserve extra space in savegame here. (currently 15 bytes) */
		SLE_CONDNULL(15,                                                           2, SL_MAX_VERSION),

		     SLE_END()
	};

	static const SaveLoad _special_desc[] = {
		SLE_WRITEBYTE(Vehicle, type, VEH_EFFECT),

		     SLE_VAR(Vehicle, subtype,               SLE_UINT8),

		 SLE_CONDVAR(Vehicle, tile,                  SLE_FILE_U16 | SLE_VAR_U32,   0,   5),
		 SLE_CONDVAR(Vehicle, tile,                  SLE_UINT32,                   6, SL_MAX_VERSION),

		 SLE_CONDVAR(Vehicle, x_pos,                 SLE_FILE_I16 | SLE_VAR_I32,   0,   5),
		 SLE_CONDVAR(Vehicle, x_pos,                 SLE_INT32,                    6, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, y_pos,                 SLE_FILE_I16 | SLE_VAR_I32,   0,   5),
		 SLE_CONDVAR(Vehicle, y_pos,                 SLE_INT32,                    6, SL_MAX_VERSION),
		     SLE_VAR(Vehicle, z_pos,                 SLE_UINT8),

		     SLE_VAR(Vehicle, cur_image,             SLE_UINT16),
		SLE_CONDNULL(5,                                                            0,  57),
		     SLE_VAR(Vehicle, progress,              SLE_UINT8),
		     SLE_VAR(Vehicle, vehstatus,             SLE_UINT8),

		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleEffect, animation_state),    SLE_UINT16),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleEffect, animation_substate), SLE_UINT8),

		 SLE_CONDVAR(Vehicle, spritenum,             SLE_UINT8,                    2, SL_MAX_VERSION),

		/* reserve extra space in savegame here. (currently 15 bytes) */
		SLE_CONDNULL(15,                                                           2, SL_MAX_VERSION),

		     SLE_END()
	};

	static const SaveLoad _disaster_desc[] = {
		SLE_WRITEBYTE(Vehicle, type, VEH_DISASTER),

		     SLE_REF(Vehicle, next,                  REF_VEHICLE_OLD),

		     SLE_VAR(Vehicle, subtype,               SLE_UINT8),
		 SLE_CONDVAR(Vehicle, tile,                  SLE_FILE_U16 | SLE_VAR_U32,   0,   5),
		 SLE_CONDVAR(Vehicle, tile,                  SLE_UINT32,                   6, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, dest_tile,             SLE_FILE_U16 | SLE_VAR_U32,   0,   5),
		 SLE_CONDVAR(Vehicle, dest_tile,             SLE_UINT32,                   6, SL_MAX_VERSION),

		 SLE_CONDVAR(Vehicle, x_pos,                 SLE_FILE_I16 | SLE_VAR_I32,   0,   5),
		 SLE_CONDVAR(Vehicle, x_pos,                 SLE_INT32,                    6, SL_MAX_VERSION),
		 SLE_CONDVAR(Vehicle, y_pos,                 SLE_FILE_I16 | SLE_VAR_I32,   0,   5),
		 SLE_CONDVAR(Vehicle, y_pos,                 SLE_INT32,                    6, SL_MAX_VERSION),
		     SLE_VAR(Vehicle, z_pos,                 SLE_UINT8),
		     SLE_VAR(Vehicle, direction,             SLE_UINT8),

		SLE_CONDNULL(5,                                                            0,  57),
		     SLE_VAR(Vehicle, owner,                 SLE_UINT8),
		     SLE_VAR(Vehicle, vehstatus,             SLE_UINT8),
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest),           SLE_FILE_U8 | SLE_VAR_U16,   0,   4),
		SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest),           SLE_UINT16,                  5, SL_MAX_VERSION),

		     SLE_VAR(Vehicle, cur_image,             SLE_UINT16),
		 SLE_CONDVAR(Vehicle, age,                   SLE_FILE_U16 | SLE_VAR_I32,   0,  30),
		 SLE_CONDVAR(Vehicle, age,                   SLE_INT32,                   31, SL_MAX_VERSION),
		     SLE_VAR(Vehicle, tick_counter,          SLE_UINT8),

		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleDisaster, image_override),            SLE_UINT16),
		    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleDisaster, big_ufo_destroyer_target),  SLE_UINT16),

		/* reserve extra space in savegame here. (currently 16 bytes) */
		SLE_CONDNULL(16,                                                           2, SL_MAX_VERSION),

		     SLE_END()
	};


	static const SaveLoad *_veh_descs[] = {
		_train_desc,
		_roadveh_desc,
		_ship_desc,
		_aircraft_desc,
		_special_desc,
		_disaster_desc,
		_common_veh_desc,
	};

	return _veh_descs[vt];
}

/** Will be called when the vehicles need to be saved. */
static void Save_VEHS()
{
	Vehicle *v;
	/* Write the vehicles */
	FOR_ALL_VEHICLES(v) {
		SlSetArrayIndex(v->index);
		SlObject(v, GetVehicleDescription(v->type));
	}
}

/** Will be called when vehicles need to be loaded. */
void Load_VEHS()
{
	int index;

	_cargo_count = 0;

	while ((index = SlIterateArray()) != -1) {
		Vehicle *v;
		VehicleType vtype = (VehicleType)SlReadByte();

		switch (vtype) {
			case VEH_TRAIN:    v = new (index) Train();           break;
			case VEH_ROAD:     v = new (index) RoadVehicle();     break;
			case VEH_SHIP:     v = new (index) Ship();            break;
			case VEH_AIRCRAFT: v = new (index) Aircraft();        break;
			case VEH_EFFECT:   v = new (index) EffectVehicle();   break;
			case VEH_DISASTER: v = new (index) DisasterVehicle(); break;
			case VEH_INVALID:  v = new (index) InvalidVehicle();  break;
			default: NOT_REACHED();
		}

		SlObject(v, GetVehicleDescription(vtype));

		if (_cargo_count != 0 && IsCompanyBuildableVehicleType(v)) {
			/* Don't construct the packet with station here, because that'll fail with old savegames */
			CargoPacket *cp = new CargoPacket();
			cp->source          = _cargo_source;
			cp->source_xy       = _cargo_source_xy;
			cp->count           = _cargo_count;
			cp->days_in_transit = _cargo_days;
			cp->feeder_share    = _cargo_feeder_share;
			cp->loaded_at_xy    = _cargo_loaded_at_xy;
			v->cargo.Append(cp);
		}

		/* Old savegames used 'last_station_visited = 0xFF' */
		if (CheckSavegameVersion(5) && v->last_station_visited == 0xFF)
			v->last_station_visited = INVALID_STATION;

		if (CheckSavegameVersion(5)) {
			/* Convert the current_order.type (which is a mix of type and flags, because
			 *  in those versions, they both were 4 bits big) to type and flags */
			v->current_order.flags = GB(v->current_order.type, 4, 4);
			v->current_order.type &= 0x0F;
		}

		/* Advanced vehicle lists got added */
		if (CheckSavegameVersion(60)) v->group_id = DEFAULT_GROUP;
	}
}

extern const ChunkHandler _veh_chunk_handlers[] = {
	{ 'VEHS', Save_VEHS, Load_VEHS, CH_SPARSE_ARRAY | CH_LAST},
};
