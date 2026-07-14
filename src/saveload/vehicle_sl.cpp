/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vehicle_sl.cpp Code handling saving and loading of vehicles. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/vehicle_sl_compat.h"

#include "../debug.h"
#include "../vehicle_func.h"
#include "../train.h"
#include "../roadveh.h"
#include "../ship.h"
#include "../aircraft.h"
#include "../timetable.h"
#include "../station_base.h"
#include "../effectvehicle_base.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../disaster_vehicle.h"
#include "../economy_base.h"

#include "../safeguards.h"

/**
 * Link front and rear multiheaded engines to each other
 * This is done when loading a savegame
 */
void ConnectMultiheadedTrains()
{
	for (Train *v : Train::Iterate()) {
		v->other_multiheaded_part = nullptr;
	}

	for (Train *v : Train::Iterate()) {
		if (v->IsFrontEngine() || v->IsFreeWagon()) {
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

			bool sequential_matching = v->IsFrontEngine();

			for (Train *u = v; u != nullptr; u = u->GetNextVehicle()) {
				if (u->other_multiheaded_part != nullptr) continue; // we already linked this one

				if (u->IsMultiheaded()) {
					if (!u->IsEngine()) {
						/* we got a rear car without a front car. We will convert it to a front one */
						u->SetEngine();
						u->spritenum--;
					}

					/* Find a matching back part */
					EngineID eid = u->engine_type;
					Train *w;
					if (sequential_matching) {
						for (w = u->GetNextVehicle(); w != nullptr; w = w->GetNextVehicle()) {
							if (w->engine_type != eid || w->other_multiheaded_part != nullptr || !w->IsMultiheaded()) continue;

							/* we found a car to partner with this engine. Now we will make sure it face the right way */
							if (w->IsEngine()) {
								w->ClearEngine();
								w->spritenum++;
							}
							break;
						}
					} else {
						uint stack_pos = 0;
						for (w = u->GetNextVehicle(); w != nullptr; w = w->GetNextVehicle()) {
							if (w->engine_type != eid || w->other_multiheaded_part != nullptr || !w->IsMultiheaded()) continue;

							if (w->IsEngine()) {
								stack_pos++;
							} else {
								if (stack_pos == 0) break;
								stack_pos--;
							}
						}
					}

					if (w != nullptr) {
						w->other_multiheaded_part = u;
						u->other_multiheaded_part = w;
					} else {
						/* we got a front car and no rear cars. We will fake this one for forget that it should have been multiheaded */
						u->ClearMultiheaded();
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
	for (Train *t : Train::Iterate()) SetBit(t->subtype, 7); // indicates that it's the old format and needs to be converted in the next loop

	for (Train *t : Train::Iterate()) {
		if (HasBit(t->subtype, 7) && ((t->subtype & ~0x80) == 0 || (t->subtype & ~0x80) == 4)) {
			for (Train *u = t; u != nullptr; u = u->Next()) {
				const RailVehicleInfo *rvi = RailVehInfo(u->engine_type);

				ClrBit(u->subtype, 7);
				switch (u->subtype) {
					case 0: // TS_Front_Engine
						if (rvi->railveh_type == RailVehicleType::Multihead) u->SetMultiheaded();
						u->SetFrontEngine();
						u->SetEngine();
						break;

					case 1: // TS_Artic_Part
						u->subtype = 0;
						u->SetArticulatedPart();
						break;

					case 2: // TS_Not_First
						u->subtype = 0;
						if (rvi->railveh_type == RailVehicleType::Wagon) {
							/* normal wagon */
							u->SetWagon();
							break;
						}
						if (rvi->railveh_type == RailVehicleType::Multihead && rvi->image_index == u->spritenum - 1) {
							/* rear end of a multiheaded engine */
							u->SetMultiheaded();
							break;
						}
						if (rvi->railveh_type == RailVehicleType::Multihead) u->SetMultiheaded();
						u->SetEngine();
						break;

					case 4: // TS_Free_Car
						u->subtype = 0;
						u->SetWagon();
						u->SetFreeWagon();
						break;
					default: SlErrorCorrupt("Invalid train subtype");
				}
			}
		}
	}
}


/** need to be called to load aircraft from old version */
void UpdateOldAircraft()
{
	/* set airport_flags to 0 for all airports just to be sure */
	for (Station *st : Station::Iterate()) {
		st->airport.blocks = {}; // reset airport
	}

	for (Aircraft *a : Aircraft::Iterate()) {
		/* airplane has another vehicle with subtype 4 (shadow), helicopter also has 3 (rotor)
		 * skip those */
		if (a->IsNormalAircraft()) {
			/* airplane in terminal stopped doesn't hurt anyone, so goto next */
			if (a->vehstatus.Test(VehState::Stopped) && a->state == 0) {
				a->state = HANGAR;
				continue;
			}

			AircraftLeaveHangar(a, a->direction); // make airplane visible if it was in a depot for example
			a->vehstatus.Reset(VehState::Stopped); // make airplane moving
			UpdateAircraftCache(a);
			a->cur_speed = a->vcache.cached_max_speed; // so aircraft don't have zero speed while in air
			if (!a->current_order.IsType(OT_GOTO_STATION) && !a->current_order.IsType(OT_GOTO_DEPOT)) {
				/* reset current order so aircraft doesn't have invalid "station-only" order */
				a->current_order.MakeDummy();
			}
			a->state = FLYING;
			AircraftNextAirportPos_and_Order(a); // move it to the entry point of the airport
			GetNewVehiclePosResult gp = GetNewVehiclePos(a);
			a->tile = TileIndex{}; // aircraft in air is tile=0

			/* correct speed of helicopter-rotors */
			if (a->subtype == AIR_HELICOPTER) a->Next()->Next()->cur_speed = 32;

			/* set new position x,y,z */
			GetAircraftFlightLevelBounds(a, &a->z_pos, nullptr);
			SetAircraftPosition(a, gp.x, gp.y, GetAircraftFlightLevel(a));
		}
	}

	/* Clear aircraft from loading vehicles, if we bumped them into the air. */
	for (Station *st : Station::Iterate()) {
		for (auto iter = st->loading_vehicles.begin(); iter != st->loading_vehicles.end(); /* nothing */) {
			Vehicle *v = *iter;
			if (v->type == VehicleType::Aircraft && !v->current_order.IsType(OT_LOADING)) {
				iter = st->loading_vehicles.erase(iter);
				delete v->cargo_payment;
			} else {
				++iter;
			}
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
	size_t total_engines = Engine::GetPoolSize();
	VehicleTypeIndexArray<EngineID> first_engine = { EngineID::Invalid(), EngineID::Invalid(), EngineID::Invalid(), EngineID::Invalid() };

	for (const Engine *e : Engine::IterateType(VehicleType::Train)) { first_engine[VehicleType::Train] = e->index; break; }
	for (const Engine *e : Engine::IterateType(VehicleType::Road)) { first_engine[VehicleType::Road] = e->index; break; }
	for (const Engine *e : Engine::IterateType(VehicleType::Ship)) { first_engine[VehicleType::Ship] = e->index; break; }
	for (const Engine *e : Engine::IterateType(VehicleType::Aircraft)) { first_engine[VehicleType::Aircraft] = e->index; break; }

	for (Vehicle *v : Vehicle::Iterate()) {
		/* Test if engine types match */
		switch (v->type) {
			case VehicleType::Train:
			case VehicleType::Road:
			case VehicleType::Ship:
			case VehicleType::Aircraft:
				if (v->engine_type >= total_engines || v->type != v->GetEngine()->type) {
					v->engine_type = first_engine[v->type];
				}
				break;

			default:
				break;
		}
	}
}

extern uint8_t _age_cargo_skip_counter; // From misc_sl.cpp

/**
 * Called after load for phase 1 of vehicle initialisation.
 * @param part_of_load Whether we are being called during loading a savegame, or due to NewGRFs being changed.
 */
void AfterLoadVehiclesPhase1(bool part_of_load)
{
	for (Vehicle *v : Vehicle::Iterate()) {
		/* Reinstate the previous pointer */
		if (v->Next() != nullptr) v->Next()->previous = v;
		if (v->NextShared() != nullptr) v->NextShared()->previous_shared = v;

		if (part_of_load) v->fill_percent_te_id = INVALID_TE_ID;
		v->first = nullptr;
		v->last = nullptr;
		if (v->IsGroundVehicle()) v->GetGroundVehicleCache()->first_engine = EngineID::Invalid();
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
		std::map<uint32_t, OrderList *> mapping;

		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->orders != nullptr) {
				if (IsSavegameVersionBefore(SaveLoadVersion::OrderList)) { // Pre-105 didn't save an OrderList
					if (mapping[v->old_orders] == nullptr) {
						/* This adds the whole shared vehicle chain for case b */

						/* Creating an OrderList here is safe because the number of vehicles
						 * allowed in these savegames matches the number of OrderLists. As
						 * such each vehicle can get an OrderList and it will (still) fit. */
						assert(OrderList::CanAllocateItem());
						std::vector<Order> orders;
						for (const OldOrderSaveLoadItem *old_order = GetOldOrder(v->old_orders); old_order != nullptr; old_order = GetOldOrder(old_order->next)) {
							orders.push_back(std::move(old_order->order));
						}
						v->orders = mapping[v->old_orders] = OrderList::Create(std::move(orders), v);
					} else {
						v->orders = mapping[v->old_orders];
						/* For old games (case a) we must create the shared vehicle chain */
						if (IsSavegameVersionBefore(SaveLoadVersion::BigMap, 2)) {
							v->AddToShared(v->orders->GetFirstSharedVehicle());
						}
					}
				} else { // OrderList was saved as such, only recalculate not saved values
					if (v->PreviousShared() == nullptr) {
						v->orders->Initialize(v);
					}
				}
			}
		}
	}

	for (Vehicle *v : Vehicle::Iterate()) {
		/* Fill the first pointers. */
		if (v->Previous() == nullptr) {
			for (Vehicle *u = v; u != nullptr; u = u->Next()) {
				u->first = v;
			}
		}

		/* Fill the last pointers. */
		if (v->Next() == nullptr) {
			for (Vehicle *u = v; u != nullptr; u = u->Previous()) {
				u->last = v;
			}
		}
	}

	if (part_of_load) {
		if (IsSavegameVersionBefore(SaveLoadVersion::OrderList)) {
			/* Before 105 there was no order for shared orders, thus it messed up horribly */
			for (Vehicle *v : Vehicle::Iterate()) {
				if (v->First() != v || v->orders != nullptr || v->previous_shared != nullptr || v->next_shared == nullptr) continue;

				/* As above, allocating OrderList here is safe. */
				assert(OrderList::CanAllocateItem());
				v->orders = OrderList::Create();
				v->orders->first_shared = v;
				for (Vehicle *u = v; u != nullptr; u = u->next_shared) {
					u->orders = v->orders;
				}
			}
		}

		if (IsSavegameVersionBefore(SaveLoadVersion::UnifyGroundVehicles)) {
			/* The road vehicle subtype was converted to a flag. */
			for (RoadVehicle *rv : RoadVehicle::Iterate()) {
				if (rv->subtype == 0) {
					/* The road vehicle is at the front. */
					rv->SetFrontEngine();
				} else if (rv->subtype == 1) {
					/* The road vehicle is an articulated part. */
					rv->subtype = 0;
					rv->SetArticulatedPart();
				} else {
					SlErrorCorrupt("Invalid road vehicle subtype");
				}
			}
		}

		if (IsSavegameVersionBefore(SaveLoadVersion::DisallowRoadReconstruction)) {
			/* In some old savegames there might be some "crap" stored. */
			for (Vehicle *v : Vehicle::Iterate()) {
				if (!v->IsPrimaryVehicle() && v->type != VehicleType::Disaster) {
					v->current_order.Free();
					v->unitnumber = 0;
				}
			}
		}

		if (IsSavegameVersionBefore(SaveLoadVersion::NewGRFCustomCargoAging)) {
			/* Set the vehicle-local cargo age counter from the old global counter. */
			for (Vehicle *v : Vehicle::Iterate()) {
				v->cargo_age_counter = _age_cargo_skip_counter;
			}
		}

		if (IsSavegameVersionBefore(SaveLoadVersion::ServiceIntervalPercent)) {
			/* Set service interval flags */
			for (Vehicle *v : Vehicle::Iterate()) {
				if (!v->IsPrimaryVehicle()) continue;

				const Company *c = Company::Get(v->owner);
				int interval = CompanyServiceInterval(c, v->type);

				v->SetServiceIntervalIsCustom(v->GetServiceInterval() != interval);
				v->SetServiceIntervalIsPercent(c->settings.vehicle.servint_ispercent);
			}
		}

		if (IsSavegameVersionBefore(SaveLoadVersion::ShipRotation)) {
			/* Ship rotation added */
			for (Ship *s : Ship::Iterate()) {
				s->rotation = s->direction;
			}
		} else {
			for (Ship *s : Ship::Iterate()) {
				if (s->rotation == s->direction) continue;
				/* In case we are rotating on gameload, set the rotation position to
				 * the current position, otherwise the applied workaround offset would
				 * be with respect to 0,0.
				 */
				s->rotation_x_pos = s->x_pos;
				s->rotation_y_pos = s->y_pos;
			}
		}

		if (IsSavegameVersionBefore(SaveLoadVersion::TimetableStartTicks)) {
			/* Convert timetable start from a date to an absolute tick in TimerGameTick::counter. */
			for (Vehicle *v : Vehicle::Iterate()) {
				/* If the start date is 0, the vehicle is not waiting to start and can be ignored. */
				if (v->timetable_start == 0) continue;

				v->timetable_start = GetStartTickFromDate(TimerGameEconomy::Date(v->timetable_start));
			}
		}

		if (IsSavegameVersionBefore(SaveLoadVersion::VehicleEconomyAge)) {
			/* Set vehicle economy age based on calendar age. */
			for (Vehicle *v : Vehicle::Iterate()) {
				v->economy_age = TimerGameEconomy::Date{v->age.base()};
			}
		}
	}

	CheckValidVehicles();
}

/**
 * Called after load for phase 2 of vehicle initialisation.
 * @param part_of_load Whether we are being called during loading a savegame, or due to NewGRFs being changed.
 */
void AfterLoadVehiclesPhase2(bool part_of_load)
{
	for (Vehicle *v : Vehicle::Iterate()) {
		assert(v->First() != nullptr);

		v->trip_occupancy = CalcPercentVehicleFilled(v, nullptr);

		switch (v->type) {
			case VehicleType::Train: {
				Train *t = Train::From(v);
				if (t->IsFrontEngine() || t->IsFreeWagon()) {
					t->gcache.last_speed = t->cur_speed; // update displayed train speed
					t->ConsistChanged(CCF_SAVELOAD);
				}
				break;
			}

			case VehicleType::Road: {
				RoadVehicle *rv = RoadVehicle::From(v);
				if (rv->IsFrontEngine()) {
					rv->gcache.last_speed = rv->cur_speed; // update displayed road vehicle speed

					rv->roadtype = Engine::Get(rv->engine_type)->VehInfo<RoadVehicleInfo>().roadtype;
					rv->compatible_roadtypes = GetRoadTypeInfo(rv->roadtype)->powered_roadtypes;
					RoadTramType rtt = GetRoadTramType(rv->roadtype);
					for (RoadVehicle *u = rv; u != nullptr; u = u->Next()) {
						u->roadtype = rv->roadtype;
						u->compatible_roadtypes = rv->compatible_roadtypes;
						if (GetRoadType(u->tile, rtt) == INVALID_ROADTYPE) SlErrorCorrupt("Road vehicle on invalid road type");
					}

					RoadVehUpdateCache(rv);
					if (_settings_game.vehicle.roadveh_acceleration_model != AccelerationModel::Original) {
						rv->CargoChanged();
					}
				}
				break;
			}

			case VehicleType::Ship:
				Ship::From(v)->UpdateCache();
				break;

			default: break;
		}
	}

	/* Stop non-front engines */
	if (part_of_load && IsSavegameVersionBefore(SaveLoadVersion::SplitHQ)) {
		for (Vehicle *v : Vehicle::Iterate()) {
			if (v->type == VehicleType::Train) {
				Train *t = Train::From(v);
				if (!t->IsFrontEngine()) {
					if (t->IsEngine()) t->vehstatus.Set(VehState::Stopped);
					/* cur_speed is now relevant for non-front parts - nonzero breaks
					 * moving-wagons-inside-depot- and autoreplace- code */
					t->cur_speed = 0;
				}
			}
			/* trains weren't stopping gradually in old OTTD versions (and TTO/TTD)
			 * other vehicle types didn't have zero speed while stopped (even in 'recent' OTTD versions) */
			if (v->vehstatus.Test(VehState::Stopped) && (v->type != VehicleType::Train || IsSavegameVersionBefore(SaveLoadVersion::VehicleCurrencyStationChanges, 1))) {
				v->cur_speed = 0;
			}
		}
	}

	for (Vehicle *v : Vehicle::Iterate()) {
		switch (v->type) {
			case VehicleType::Road:
			case VehicleType::Train:
			case VehicleType::Ship:
				v->GetImage(v->direction, EngineImageType::OnMap, &v->sprite_cache.sprite_seq);
				break;

			case VehicleType::Aircraft:
				if (Aircraft::From(v)->IsNormalAircraft()) {
					v->GetImage(v->direction, EngineImageType::OnMap, &v->sprite_cache.sprite_seq);

					/* The aircraft's shadow will have the same image as the aircraft, but no colour */
					Vehicle *shadow = v->Next();
					if (shadow == nullptr) SlErrorCorrupt("Missing shadow for aircraft");

					shadow->sprite_cache.sprite_seq.CopyWithoutPalette(v->sprite_cache.sprite_seq);

					/* In the case of a helicopter we will update the rotor sprites */
					if (v->subtype == AIR_HELICOPTER) {
						Vehicle *rotor = shadow->Next();
						if (rotor == nullptr) SlErrorCorrupt("Missing rotor for helicopter");

						GetRotorImage(Aircraft::From(v), EngineImageType::OnMap, &rotor->sprite_cache.sprite_seq);
					}

					UpdateAircraftCache(Aircraft::From(v), true);
				}
				break;

			case VehicleType::Disaster: {
				auto *dv = DisasterVehicle::From(v);
				if (dv->subtype == ST_SMALL_UFO && dv->state != 0) {
					RoadVehicle *u = RoadVehicle::GetIfValid(v->dest_tile.base());
					if (u != nullptr && u->IsFrontEngine()) {
						/* Delete UFO targetting a vehicle which is already a target. */
						if (u->disaster_vehicle != VehicleID::Invalid() && u->disaster_vehicle != dv->index) {
							delete v;
							continue;
						} else {
							u->disaster_vehicle = dv->index;
						}
					}
				}
				break;
			}

			default: break;
		}

		if (part_of_load && v->unitnumber != 0) {
			Company::Get(v->owner)->freeunits[v->type].UseID(v->unitnumber);
		}

		v->UpdateDeltaXY();
		v->coord.left = INVALID_COORD;
		v->sprite_cache.old_coord.left = INVALID_COORD;
		if (v->type != VehicleType::Effect) v->UpdatePosition();
		v->UpdateViewport(false);
	}
}

bool TrainController(Train *v, Vehicle *nomove, bool reverse = true); // From train_cmd.cpp
void ReverseTrainSwapVehicles(Train *v);

/** Fixup old train spacing. */
void FixupTrainLengths()
{
	/* Vehicle center was moved from 4 units behind the front to half the length
	 * behind the front. Move vehicles so they end up on the same spot. */
	for (Vehicle *v : Vehicle::Iterate()) {
		if (v->type == VehicleType::Train && v->IsPrimaryVehicle()) {
			/* The vehicle center is now more to the front depending on vehicle length,
			 * so we need to move all vehicles forward to cover the difference to the
			 * old center, otherwise wagon spacing in trains would be broken upon load. */
			for (Train *u = Train::From(v); u != nullptr; u = u->Next()) {
				if (u->track == Track::Depot || u->vehstatus.Test(VehState::Crashed)) continue;

				Train *next = u->Next();

				/* Try to pull the vehicle half its length forward. */
				int diff = (VEHICLE_LENGTH - u->gcache.cached_veh_length) / 2;
				int done;
				for (done = 0; done < diff; done++) {
					if (!TrainController(u, next, false)) break;
				}

				if (next != nullptr && done < diff && u->IsFrontEngine()) {
					/* Pulling the front vehicle forwards failed, we either encountered a dead-end
					 * or a red signal. To fix this, we try to move the whole train the required
					 * space backwards and re-do the fix up of the front vehicle. */

					/* Ignore any signals when backtracking. */
					TrainForceProceeding old_tfp = u->force_proceed;
					u->force_proceed = TFP_SIGNAL;

					/* Swap start<>end, start+1<>end-1, ... */
					ReverseTrainSwapVehicles(u);

					/* We moved the first vehicle which is now the last. Move it back to the
					 * original position as we will fix up the last vehicle later in the loop. */
					for (int i = 0; i < done; i++) TrainController(u->Last(), nullptr);

					/* Move the train backwards to get space for the first vehicle. As the stopping
					 * distance from a line end is rounded up, move the train one unit more to cater
					 * for front vehicles with odd lengths. */
					int moved;
					for (moved = 0; moved < diff + 1; moved++) {
						if (!TrainController(u, nullptr, false)) break;
					}

					/* Swap start<>end, start+1<>end-1, ... again. */
					ReverseTrainSwapVehicles(u);

					u->force_proceed = old_tfp;

					/* Tracks are too short to fix the train length. The player has to fix the
					 * train in a depot. Bail out so we don't damage the vehicle chain any more. */
					if (moved < diff + 1) break;

					/* Re-do the correction for the first vehicle. */
					for (done = 0; done < diff; done++) TrainController(u, next, false);

					/* We moved one unit more backwards than needed for even-length front vehicles,
					 * try to move that unit forward again. We don't care if this step fails. */
					TrainController(u, nullptr, false);
				}

				/* If the next wagon is still in a depot, check if it shouldn't be outside already. */
				if (next != nullptr && next->track == Track::Depot) {
					int d = TicksToLeaveDepot(u);
					if (d <= 0) {
						/* Next vehicle should have left the depot already, show it and pull forward. */
						next->vehstatus.Reset(VehState::Hidden);
						next->track = GetRailDepotTrack(next->tile);
						for (int i = 0; i >= d; i--) TrainController(next, nullptr);
					}
				}
			}

			/* Update all cached properties after moving the vehicle chain around. */
			Train::From(v)->ConsistChanged(CCF_TRACK);
		}
	}
}

static uint8_t  _cargo_periods;
static StationID _cargo_source;
static TileIndex _cargo_source_xy;
static uint16_t _cargo_count;
static uint16_t _cargo_paid_for;
static Money  _cargo_feeder_share;

class SlVehicleCommon : public DefaultSaveLoadHandler<SlVehicleCommon, Vehicle> {
public:
	static inline const SaveLoad description[] = {
		    SLE_VAR(Vehicle, subtype,               VarTypes::U8),

		    SLE_REF(Vehicle, next,                  SLRefType::OldVehicle),
		SLE_CONDVAR(Vehicle, name, VarTypes::NAME, SaveLoadVersion::MinVersion, SaveLoadVersion::ReplaceCustomNameArray),
		SLE_CONDSSTR(Vehicle, name, VarTypes::STR | StringValidationSetting::AllowControlCode, SaveLoadVersion::ReplaceCustomNameArray, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, unitnumber, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::LargerUnitNumber),
		SLE_CONDVAR(Vehicle, unitnumber, VarTypes::U16, SaveLoadVersion::LargerUnitNumber, SaveLoadVersion::MaxVersion),
		    SLE_VAR(Vehicle, owner,                 VarTypes::U8),
		SLE_CONDVAR(Vehicle, tile, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		SLE_CONDVAR(Vehicle, tile, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, dest_tile, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		SLE_CONDVAR(Vehicle, dest_tile, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),

		SLE_CONDVAR(Vehicle, x_pos, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		SLE_CONDVAR(Vehicle, x_pos, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, y_pos, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		SLE_CONDVAR(Vehicle, y_pos, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, z_pos, VarFileType::U8 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::VehicleCentreAndZPos),
		SLE_CONDVAR(Vehicle, z_pos, VarTypes::I32, SaveLoadVersion::VehicleCentreAndZPos, SaveLoadVersion::MaxVersion),
		    SLE_VAR(Vehicle, direction,             VarTypes::U8),

		    SLE_VAR(Vehicle, spritenum,             VarTypes::U8),
		    SLE_VAR(Vehicle, engine_type,           VarTypes::U16),
		    SLE_VAR(Vehicle, cur_speed,             VarTypes::U16),
		    SLE_VAR(Vehicle, subspeed,              VarTypes::U8),
		    SLE_VAR(Vehicle, acceleration,          VarTypes::U8),
		SLE_CONDVAR(Vehicle, motion_counter, VarTypes::U32, SaveLoadVersion::VehMotionCounter, SaveLoadVersion::MaxVersion),
		    SLE_VAR(Vehicle, progress,              VarTypes::U8),

		    SLE_VAR(Vehicle, vehstatus,             VarTypes::U8),
		SLE_CONDVAR(Vehicle, last_station_visited, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),
		SLE_CONDVAR(Vehicle, last_station_visited, VarTypes::U16, SaveLoadVersion::BigMap, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, last_loading_station, VarTypes::U16, SaveLoadVersion::GoalProgressPlaneAcceleration, SaveLoadVersion::MaxVersion),

		    SLE_VAR(Vehicle, cargo_type,            VarTypes::U8),
		SLE_CONDVAR(Vehicle, cargo_subtype, VarTypes::U8, SaveLoadVersion::LiveryRefit, SaveLoadVersion::MaxVersion),
		SLEG_CONDVAR("cargo_days", _cargo_periods, VarTypes::U8, SaveLoadVersion::MinVersion, SaveLoadVersion::CargoPackets),
		SLEG_CONDVAR("cargo_source", _cargo_source, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::LargerCargoSource),
		SLEG_CONDVAR("cargo_source", _cargo_source, VarTypes::U16, SaveLoadVersion::LargerCargoSource, SaveLoadVersion::CargoPackets),
		SLEG_CONDVAR("cargo_source_xy", _cargo_source_xy, VarTypes::U32, SaveLoadVersion::CargoSourceTile, SaveLoadVersion::CargoPackets),
		    SLE_VAR(Vehicle, cargo_cap,             VarTypes::U16),
		SLE_CONDVAR(Vehicle, refit_cap, VarTypes::U16, SaveLoadVersion::GoalProgressPlaneAcceleration, SaveLoadVersion::MaxVersion),
		SLEG_CONDVAR("cargo_count", _cargo_count, VarTypes::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::CargoPackets),
		SLE_CONDREFLIST(Vehicle, cargo.packets, SLRefType::CargoPacket, SaveLoadVersion::CargoPackets, SaveLoadVersion::MaxVersion),
		SLE_CONDARR(Vehicle, cargo.action_counts, VarTypes::U32, to_underlying(VehicleCargoList::MoveToAction::End), SaveLoadVersion::CargoReservation, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, cargo_age_counter, VarTypes::U16, SaveLoadVersion::NewGRFCustomCargoAging, SaveLoadVersion::MaxVersion),

		    SLE_VAR(Vehicle, day_counter,           VarTypes::U8),
		    SLE_VAR(Vehicle, tick_counter,          VarTypes::U8),
		SLE_CONDVAR(Vehicle, running_ticks, VarTypes::U8, SaveLoadVersion::FractionProfitRunningTicks, SaveLoadVersion::MaxVersion),

		    SLE_VAR(Vehicle, cur_implicit_order_index,  VarTypes::U8),
		SLE_CONDVAR(Vehicle, cur_real_order_index, VarTypes::U8, SaveLoadVersion::TrackRealAndAutoOrders, SaveLoadVersion::MaxVersion),

		/* This next line is for version 4 and prior compatibility.. it temporarily reads
		type and flags (which were both 4 bits) into type. Later on this is
		converted correctly */
		SLE_CONDVAR(Vehicle, current_order.type, VarTypes::U8, SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),
		SLE_CONDVAR(Vehicle, current_order.dest, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),

		/* Orders for version 5 and on */
		SLE_CONDVAR(Vehicle, current_order.type, VarTypes::U8, SaveLoadVersion::BigMap, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, current_order.flags, VarTypes::U8, SaveLoadVersion::BigMap, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, current_order.dest, VarTypes::U16, SaveLoadVersion::BigMap, SaveLoadVersion::MaxVersion),

		/* Refit in current order */
		SLE_CONDVAR(Vehicle, current_order.refit_cargo, VarTypes::U8, SaveLoadVersion::RefitOrders, SaveLoadVersion::MaxVersion),

		/* Timetable in current order */
		SLE_CONDVAR(Vehicle, current_order.wait_time, VarTypes::U16, SaveLoadVersion::Timetables, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, current_order.travel_time, VarTypes::U16, SaveLoadVersion::Timetables, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, current_order.max_speed, VarTypes::U16, SaveLoadVersion::CurrentOrderMaxSpeed, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, timetable_start, VarFileType::I32 | VarMemType::U64, SaveLoadVersion::TimetableStart, SaveLoadVersion::TimetableStartTicks),
		SLE_CONDVAR(Vehicle, timetable_start, VarTypes::U64, SaveLoadVersion::TimetableStartTicks, SaveLoadVersion::MaxVersion),

		SLE_CONDVARNAME(Vehicle, old_orders, "orders", VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::MoreCargoPackets),
		SLE_CONDVARNAME(Vehicle, old_orders, "orders", VarTypes::U32, SaveLoadVersion::MoreCargoPackets, SaveLoadVersion::OrderList),
		SLE_CONDREF(Vehicle, orders, SLRefType::OrderList, SaveLoadVersion::OrderList, SaveLoadVersion::MaxVersion),

		SLE_CONDVAR(Vehicle, age, VarFileType::U16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
		SLE_CONDVAR(Vehicle, age, VarTypes::I32, SaveLoadVersion::BigDates, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, economy_age, VarTypes::I32, SaveLoadVersion::VehicleEconomyAge, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, max_age, VarFileType::U16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
		SLE_CONDVAR(Vehicle, max_age, VarTypes::I32, SaveLoadVersion::BigDates, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, date_of_last_service, VarFileType::U16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
		SLE_CONDVAR(Vehicle, date_of_last_service, VarTypes::I32, SaveLoadVersion::BigDates, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, date_of_last_service_newgrf, VarTypes::I32, SaveLoadVersion::NewGRFLastService, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, service_interval, VarTypes::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
		SLE_CONDVAR(Vehicle, service_interval, VarFileType::U32 | VarMemType::U16, SaveLoadVersion::BigDates, SaveLoadVersion::ServiceIntervalPercent),
		SLE_CONDVAR(Vehicle, service_interval, VarTypes::U16, SaveLoadVersion::ServiceIntervalPercent, SaveLoadVersion::MaxVersion),
		    SLE_VAR(Vehicle, reliability,           VarTypes::U16),
		    SLE_VAR(Vehicle, reliability_spd_dec,   VarTypes::U16),
		    SLE_VAR(Vehicle, breakdown_ctr,         VarTypes::U8),
		    SLE_VAR(Vehicle, breakdown_delay,       VarTypes::U8),
		    SLE_VAR(Vehicle, breakdowns_since_last_service, VarTypes::U8),
		    SLE_VAR(Vehicle, breakdown_chance,      VarTypes::U8),
		SLE_CONDVAR(Vehicle, build_year, VarFileType::U8 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
		SLE_CONDVAR(Vehicle, build_year, VarTypes::I32, SaveLoadVersion::BigDates, SaveLoadVersion::MaxVersion),

		    SLE_VAR(Vehicle, load_unload_ticks,     VarTypes::U16),
		SLEG_CONDVAR("cargo_paid_for", _cargo_paid_for, VarTypes::U16, SaveLoadVersion::CountPaidForCargo, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, vehicle_flags, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::GradualLoading, SaveLoadVersion::ServiceIntervalPercent),
		SLE_CONDVAR(Vehicle, vehicle_flags, VarTypes::U16, SaveLoadVersion::ServiceIntervalPercent, SaveLoadVersion::MaxVersion),

		SLE_CONDVAR(Vehicle, profit_this_year, VarFileType::I32 | VarMemType::I64, SaveLoadVersion::MinVersion, SaveLoadVersion::UnifyCurrency),
		SLE_CONDVAR(Vehicle, profit_this_year, VarTypes::I64, SaveLoadVersion::UnifyCurrency, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, profit_last_year, VarFileType::I32 | VarMemType::I64, SaveLoadVersion::MinVersion, SaveLoadVersion::UnifyCurrency),
		SLE_CONDVAR(Vehicle, profit_last_year, VarTypes::I64, SaveLoadVersion::UnifyCurrency, SaveLoadVersion::MaxVersion),
		SLEG_CONDVAR("cargo_feeder_share", _cargo_feeder_share, VarFileType::I32 | VarMemType::I64, SaveLoadVersion::FeederShare, SaveLoadVersion::UnifyCurrency),
		SLEG_CONDVAR("cargo_feeder_share", _cargo_feeder_share, VarTypes::I64, SaveLoadVersion::UnifyCurrency, SaveLoadVersion::CargoPackets),
		SLE_CONDVAR(Vehicle, value, VarFileType::I32 | VarMemType::I64, SaveLoadVersion::MinVersion, SaveLoadVersion::UnifyCurrency),
		SLE_CONDVAR(Vehicle, value, VarTypes::I64, SaveLoadVersion::UnifyCurrency, SaveLoadVersion::MaxVersion),

		SLE_CONDVAR(Vehicle, random_bits, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::VehicleCurrencyStationChanges, SaveLoadVersion::ExtendVehicleRandom),
		SLE_CONDVAR(Vehicle, random_bits, VarTypes::U16, SaveLoadVersion::ExtendVehicleRandom, SaveLoadVersion::MaxVersion),
		SLE_CONDVARNAME(Vehicle, waiting_random_triggers, "waiting_triggers", VarTypes::U8, SaveLoadVersion::VehicleCurrencyStationChanges, SaveLoadVersion::MaxVersion),

		SLE_CONDREF(Vehicle, next_shared, SLRefType::Vehicle, SaveLoadVersion::VehicleCurrencyStationChanges, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, group_id, VarTypes::U16, SaveLoadVersion::VehicleGroups, SaveLoadVersion::MaxVersion),

		SLE_CONDVAR(Vehicle, current_order_time, VarFileType::U32 | VarMemType::I32, SaveLoadVersion::Timetables, SaveLoadVersion::TimetableTicksType),
		SLE_CONDVAR(Vehicle, current_order_time, VarTypes::I32, SaveLoadVersion::TimetableTicksType, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, last_loading_tick, VarTypes::U64, SaveLoadVersion::LastLoadingTick, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, lateness_counter, VarTypes::I32, SaveLoadVersion::Timetables, SaveLoadVersion::MaxVersion),

		SLE_CONDVAR(Vehicle, depot_unbunching_last_departure, VarTypes::U64, SaveLoadVersion::DepotUnbunching, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, depot_unbunching_next_departure, VarTypes::U64, SaveLoadVersion::DepotUnbunching, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, round_trip_time, VarTypes::I32, SaveLoadVersion::DepotUnbunching, SaveLoadVersion::MaxVersion),
	};

	static inline const SaveLoadCompatTable compat_description = _vehicle_common_sl_compat;

	void Save(Vehicle *v) const override
	{
		SlObject(v, this->GetDescription());
	}

	void Load(Vehicle *v) const override
	{
		SlObject(v, this->GetLoadDescription());
	}

	void FixPointers(Vehicle *v) const override
	{
		SlObject(v, this->GetDescription());
	}
};

class SlVehicleTrain : public DefaultSaveLoadHandler<SlVehicleTrain, Vehicle> {
public:
	static inline const SaveLoad description[] = {
		 SLEG_STRUCT("common", SlVehicleCommon),
		     SLE_VAR(Train, crash_anim_pos,      VarTypes::U16),
		     SLE_VAR(Train, force_proceed,       VarTypes::U8),
		     SLE_VAR(Train, track,               VarTypes::U8),

		 SLE_CONDVAR(Train, flags, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::VehicleCurrencyStationChanges, SaveLoadVersion::Yapp),
		 SLE_CONDVAR(Train, flags, VarTypes::U16, SaveLoadVersion::Yapp, SaveLoadVersion::MaxVersion),
		 SLE_CONDVAR(Train, wait_counter, VarTypes::U16, SaveLoadVersion::SplitLoadWaitCounters, SaveLoadVersion::MaxVersion),
		 SLE_CONDVAR(Train, gv_flags, VarTypes::U16, SaveLoadVersion::RvRealisticAcceleration, SaveLoadVersion::MaxVersion),
	};
	static inline const SaveLoadCompatTable compat_description = _vehicle_train_sl_compat;

	void Save(Vehicle *v) const override
	{
		if (v->type != VehicleType::Train) return;
		SlObject(v, this->GetDescription());
	}

	void Load(Vehicle *v) const override
	{
		if (v->type != VehicleType::Train) return;
		SlObject(v, this->GetLoadDescription());
	}

	void FixPointers(Vehicle *v) const override
	{
		if (v->type != VehicleType::Train) return;
		SlObject(v, this->GetDescription());
	}
};

class SlVehicleRoadVehPath : public VectorSaveLoadHandler<SlVehicleRoadVehPath, RoadVehicle, RoadVehPathElement> {
public:
	static inline const SaveLoad description[] = {
		SLE_VAR(RoadVehPathElement, trackdir, VarTypes::U8),
		SLE_VAR(RoadVehPathElement, tile, VarTypes::U32),
	};
	static inline const SaveLoadCompatTable compat_description = {};

	std::vector<RoadVehPathElement> &GetVector(RoadVehicle *rv) const override { return rv->path; }
};

class SlVehicleRoadVeh : public DefaultSaveLoadHandler<SlVehicleRoadVeh, Vehicle> {
public:
	/** @{
	 * RoadVehicle path is stored in std::pair which cannot be directly saved. */
	static inline std::vector<Trackdir> rv_path_td;
	static inline std::vector<TileIndex> rv_path_tile;
	/** @} */

	static inline const SaveLoad description[] = {
		  SLEG_STRUCT("common", SlVehicleCommon),
		      SLE_VAR(RoadVehicle, state,                VarTypes::U8),
		      SLE_VAR(RoadVehicle, frame,                VarTypes::U8),
		      SLE_VAR(RoadVehicle, blocked_ctr,          VarTypes::U16),
		      SLE_VAR(RoadVehicle, overtaking,           VarTypes::U8),
		      SLE_VAR(RoadVehicle, overtaking_ctr,       VarTypes::U8),
		      SLE_VAR(RoadVehicle, crashed_ctr,          VarTypes::U16),
		      SLE_VAR(RoadVehicle, reverse_ctr,          VarTypes::U8),
		SLEG_CONDVECTOR("path.td", rv_path_td, VarTypes::U8, SaveLoadVersion::RoadvehPathCache, SaveLoadVersion::PathCacheFormat),
		SLEG_CONDVECTOR("path.tile", rv_path_tile, VarTypes::U32, SaveLoadVersion::RoadvehPathCache, SaveLoadVersion::PathCacheFormat),
		SLEG_CONDSTRUCTLIST("path", SlVehicleRoadVehPath, SaveLoadVersion::PathCacheFormat, SaveLoadVersion::MaxVersion),
		 SLE_CONDVAR(RoadVehicle, gv_flags, VarTypes::U16, SaveLoadVersion::RvRealisticAcceleration, SaveLoadVersion::MaxVersion),
	};
	static inline const SaveLoadCompatTable compat_description = _vehicle_roadveh_sl_compat;

	static void ConvertPathCache(RoadVehicle &rv)
	{
		/* The two vectors should be the same size, but if not we can just ignore the cache and not cause more issues. */
		if (rv_path_td.size() != rv_path_tile.size()) {
			Debug(sl, 1, "Found RoadVehicle {} with invalid path cache, ignoring.", rv.index);
			return;
		}
		size_t n = std::min(rv_path_td.size(), rv_path_tile.size());
		if (n == 0) return;

		rv.path.reserve(n);
		for (size_t c = 0; c < n; ++c) {
			rv.path.emplace_back(rv_path_td[c], rv_path_tile[c]);
		}

		/* Path cache is now taken from back instead of front, so needs reversing. */
		std::reverse(std::begin(rv.path), std::end(rv.path));
	}

	void Save(Vehicle *v) const override
	{
		if (v->type != VehicleType::Road) return;
		SlObject(v, this->GetDescription());
	}

	void Load(Vehicle *v) const override
	{
		if (v->type != VehicleType::Road) return;
		SlObject(v, this->GetLoadDescription());
		if (!IsSavegameVersionBefore(SaveLoadVersion::RoadvehPathCache) && IsSavegameVersionBefore(SaveLoadVersion::PathCacheFormat)) {
			ConvertPathCache(*static_cast<RoadVehicle *>(v));
		}
	}

	void FixPointers(Vehicle *v) const override
	{
		if (v->type != VehicleType::Road) return;
		SlObject(v, this->GetDescription());
	}
};

class SlVehicleShipPath : public VectorSaveLoadHandler<SlVehicleShipPath, Ship, ShipPathElement> {
public:
	static inline const SaveLoad description[] = {
		SLE_VAR(ShipPathElement, trackdir, VarTypes::U8),
	};
	static inline const SaveLoadCompatTable compat_description = {};

	std::vector<ShipPathElement> &GetVector(Ship *s) const override { return s->path; }
};

class SlVehicleShip : public DefaultSaveLoadHandler<SlVehicleShip, Vehicle> {
public:
	static inline std::vector<Trackdir> ship_path_td;

	static inline const SaveLoad description[] = {
		  SLEG_STRUCT("common", SlVehicleCommon),
		      SLE_VAR(Ship, state,                     VarTypes::U8),
		SLEG_CONDVECTOR("path", ship_path_td, VarTypes::U8, SaveLoadVersion::ShipPathCache, SaveLoadVersion::PathCacheFormat),
		SLEG_CONDSTRUCTLIST("path", SlVehicleShipPath, SaveLoadVersion::PathCacheFormat, SaveLoadVersion::MaxVersion),
		 SLE_CONDVAR(Ship, rotation, VarTypes::U8, SaveLoadVersion::ShipRotation, SaveLoadVersion::MaxVersion),
	};
	static inline const SaveLoadCompatTable compat_description = _vehicle_ship_sl_compat;

	void Save(Vehicle *v) const override
	{
		if (v->type != VehicleType::Ship) return;
		SlObject(v, this->GetDescription());
	}

	void Load(Vehicle *v) const override
	{
		if (v->type != VehicleType::Ship) return;
		SlObject(v, this->GetLoadDescription());

		if (IsSavegameVersionBefore(SaveLoadVersion::PathCacheFormat)) {
			/* Path cache is now taken from back instead of front, so needs reversing. */
			Ship *s = static_cast<Ship *>(v);
			std::transform(std::rbegin(ship_path_td), std::rend(ship_path_td), std::back_inserter(s->path), [](Trackdir trackdir) { return trackdir; });
		}
	}

	void FixPointers(Vehicle *v) const override
	{
		if (v->type != VehicleType::Ship) return;
		SlObject(v, this->GetDescription());
	}
};

class SlVehicleAircraft : public DefaultSaveLoadHandler<SlVehicleAircraft, Vehicle> {
public:
	static inline const SaveLoad description[] = {
		 SLEG_STRUCT("common", SlVehicleCommon),
		     SLE_VAR(Aircraft, crashed_counter,       VarTypes::U16),
		     SLE_VAR(Aircraft, pos,                   VarTypes::U8),

		 SLE_CONDVAR(Aircraft, targetairport, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),
		 SLE_CONDVAR(Aircraft, targetairport, VarTypes::U16, SaveLoadVersion::BigMap, SaveLoadVersion::MaxVersion),

		     SLE_VAR(Aircraft, state,                 VarTypes::U8),

		 SLE_CONDVAR(Aircraft, previous_pos, VarTypes::U8, SaveLoadVersion::VehicleCurrencyStationChanges, SaveLoadVersion::MaxVersion),
		 SLE_CONDVAR(Aircraft, last_direction, VarTypes::U8, SaveLoadVersion::VehicleCurrencyStationChanges, SaveLoadVersion::MaxVersion),
		 SLE_CONDVAR(Aircraft, number_consecutive_turns, VarTypes::U8, SaveLoadVersion::VehicleCurrencyStationChanges, SaveLoadVersion::MaxVersion),

		 SLE_CONDVAR(Aircraft, turn_counter, VarTypes::U8, SaveLoadVersion::SplitLoadWaitCounters, SaveLoadVersion::MaxVersion),
		 SLE_CONDVAR(Aircraft, flags, VarTypes::U8, SaveLoadVersion::NewGRFAircraftRange, SaveLoadVersion::MaxVersion),
	};
	static inline const SaveLoadCompatTable compat_description = _vehicle_aircraft_sl_compat;

	void Save(Vehicle *v) const override
	{
		if (v->type != VehicleType::Aircraft) return;
		SlObject(v, this->GetDescription());
	}

	void Load(Vehicle *v) const override
	{
		if (v->type != VehicleType::Aircraft) return;
		SlObject(v, this->GetLoadDescription());
	}

	void FixPointers(Vehicle *v) const override
	{
		if (v->type != VehicleType::Aircraft) return;
		SlObject(v, this->GetDescription());
	}
};

class SlVehicleEffect : public DefaultSaveLoadHandler<SlVehicleEffect, Vehicle> {
public:
	static inline const SaveLoad description[] = {
		     SLE_VAR(Vehicle, subtype,               VarTypes::U8),

		 SLE_CONDVAR(Vehicle, tile, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		 SLE_CONDVAR(Vehicle, tile, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),

		 SLE_CONDVAR(Vehicle, x_pos, VarFileType::I16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		 SLE_CONDVAR(Vehicle, x_pos, VarTypes::I32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
		 SLE_CONDVAR(Vehicle, y_pos, VarFileType::I16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		 SLE_CONDVAR(Vehicle, y_pos, VarTypes::I32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
		 SLE_CONDVAR(Vehicle, z_pos, VarFileType::U8 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::VehicleCentreAndZPos),
		 SLE_CONDVAR(Vehicle, z_pos, VarTypes::I32, SaveLoadVersion::VehicleCentreAndZPos, SaveLoadVersion::MaxVersion),

		     SLE_VAR(Vehicle, sprite_cache.sprite_seq.seq[0].sprite, VarFileType::U16 | VarMemType::U32),
		     SLE_VAR(Vehicle, progress,              VarTypes::U8),
		     SLE_VAR(Vehicle, vehstatus,             VarTypes::U8),

		     SLE_VAR(EffectVehicle, animation_state,    VarTypes::U16),
		     SLE_VAR(EffectVehicle, animation_substate, VarTypes::U8),

		 SLE_CONDVAR(Vehicle, spritenum, VarTypes::U8, SaveLoadVersion::VehicleCurrencyStationChanges, SaveLoadVersion::MaxVersion),
	};
	static inline const SaveLoadCompatTable compat_description = _vehicle_effect_sl_compat;

	void Save(Vehicle *v) const override
	{
		if (v->type != VehicleType::Effect) return;
		SlObject(v, this->GetDescription());
	}

	void Load(Vehicle *v) const override
	{
		if (v->type != VehicleType::Effect) return;
		SlObject(v, this->GetLoadDescription());
	}

	void FixPointers(Vehicle *v) const override
	{
		if (v->type != VehicleType::Effect) return;
		SlObject(v, this->GetDescription());
	}
};

class SlVehicleDisaster : public DefaultSaveLoadHandler<SlVehicleDisaster, Vehicle> {
public:
	static inline const SaveLoad description[] = {
		    SLE_REF(Vehicle, next,                  SLRefType::OldVehicle),

		    SLE_VAR(Vehicle, subtype,               VarTypes::U8),
		SLE_CONDVAR(Vehicle, tile, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		SLE_CONDVAR(Vehicle, tile, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, dest_tile, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		SLE_CONDVAR(Vehicle, dest_tile, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),

		SLE_CONDVAR(Vehicle, x_pos, VarFileType::I16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		SLE_CONDVAR(Vehicle, x_pos, VarTypes::I32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, y_pos, VarFileType::I16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
		SLE_CONDVAR(Vehicle, y_pos, VarTypes::I32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(Vehicle, z_pos, VarFileType::U8 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::VehicleCentreAndZPos),
		SLE_CONDVAR(Vehicle, z_pos, VarTypes::I32, SaveLoadVersion::VehicleCentreAndZPos, SaveLoadVersion::MaxVersion),
		    SLE_VAR(Vehicle, direction,             VarTypes::U8),

		    SLE_VAR(Vehicle, owner,                 VarTypes::U8),
		    SLE_VAR(Vehicle, vehstatus,             VarTypes::U8),
		SLE_CONDVARNAME(DisasterVehicle, state, "current_order.dest", VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),
		SLE_CONDVARNAME(DisasterVehicle, state, "current_order.dest", VarTypes::U16, SaveLoadVersion::BigMap, SaveLoadVersion::DisasterVehState),
		SLE_CONDVAR(DisasterVehicle, state, VarTypes::U16, SaveLoadVersion::DisasterVehState, SaveLoadVersion::MaxVersion),

		    SLE_VAR(Vehicle, sprite_cache.sprite_seq.seq[0].sprite, VarFileType::U16 | VarMemType::U32),
		SLE_CONDVAR(Vehicle, age, VarFileType::U16 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
		SLE_CONDVAR(Vehicle, age, VarTypes::I32, SaveLoadVersion::BigDates, SaveLoadVersion::MaxVersion),
		    SLE_VAR(Vehicle, tick_counter,          VarTypes::U8),

		SLE_CONDVAR(DisasterVehicle, image_override, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::LinkgraphLocationDisasterStore),
		SLE_CONDVAR(DisasterVehicle, image_override, VarTypes::U32, SaveLoadVersion::LinkgraphLocationDisasterStore, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(DisasterVehicle, big_ufo_destroyer_target, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::LinkgraphLocationDisasterStore),
		SLE_CONDVAR(DisasterVehicle, big_ufo_destroyer_target, VarTypes::U32, SaveLoadVersion::LinkgraphLocationDisasterStore, SaveLoadVersion::MaxVersion),
		SLE_CONDVAR(DisasterVehicle, flags, VarTypes::U8, SaveLoadVersion::MaxBridgeMapHeight, SaveLoadVersion::MaxVersion),
	};

	static inline const SaveLoadCompatTable compat_description = _vehicle_disaster_sl_compat;

	void Save(Vehicle *v) const override
	{
		if (v->type != VehicleType::Disaster) return;
		SlObject(v, this->GetDescription());
	}

	void Load(Vehicle *v) const override
	{
		if (v->type != VehicleType::Disaster) return;
		SlObject(v, this->GetLoadDescription());
	}

	void FixPointers(Vehicle *v) const override
	{
		if (v->type != VehicleType::Disaster) return;
		SlObject(v, this->GetDescription());
	}
};

static const SaveLoad _vehicle_desc[] = {
	SLE_SAVEBYTE(Vehicle, type),
	SLEG_STRUCT("train", SlVehicleTrain),
	SLEG_STRUCT("roadveh", SlVehicleRoadVeh),
	SLEG_STRUCT("ship", SlVehicleShip),
	SLEG_STRUCT("aircraft", SlVehicleAircraft),
	SLEG_STRUCT("effect", SlVehicleEffect),
	SLEG_STRUCT("disaster", SlVehicleDisaster),
};

struct VEHSChunkHandler : ChunkHandler {
	VEHSChunkHandler() : ChunkHandler("VEHS", ChunkType::SparseTable) {}

	void Save() const override
	{
		SlTableHeader(_vehicle_desc);

		/* Write the vehicles */
		for (Vehicle *v : Vehicle::Iterate()) {
			SlSetArrayIndex(v->index);
			SlObject(v, _vehicle_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_vehicle_desc, _vehicle_sl_compat);

		int index;

		_cargo_count = 0;

		while ((index = SlIterateArray()) != -1) {
			Vehicle *v;
			VehicleType vtype = (VehicleType)SlReadByte();

			switch (vtype) {
				case VehicleType::Train: v = Train::CreateAtIndex(VehicleID(index)); break;
				case VehicleType::Road: v = RoadVehicle::CreateAtIndex(VehicleID(index)); break;
				case VehicleType::Ship: v = Ship::CreateAtIndex(VehicleID(index)); break;
				case VehicleType::Aircraft: v = Aircraft::CreateAtIndex(VehicleID(index)); break;
				case VehicleType::Effect: v = EffectVehicle::CreateAtIndex(VehicleID(index)); break;
				case VehicleType::Disaster: v = DisasterVehicle::CreateAtIndex(VehicleID(index)); break;
				case VehicleType::Invalid: // Savegame shouldn't contain invalid vehicles
				default: SlErrorCorrupt("Invalid vehicle type");
			}

			SlObject(v, slt);

			if (_cargo_count != 0 && IsCompanyBuildableVehicleType(v) && CargoPacket::CanAllocateItem()) {
				/* Don't construct the packet with station here, because that'll fail with old savegames */
				CargoPacket *cp = CargoPacket::Create(_cargo_count, _cargo_periods, _cargo_source, _cargo_source_xy, _cargo_feeder_share);
				v->cargo.Append(cp);
			}

			/* Old savegames used 'last_station_visited = 0xFF' */
			if (IsSavegameVersionBefore(SaveLoadVersion::BigMap) && v->last_station_visited == 0xFF) {
				v->last_station_visited = StationID::Invalid();
			}

			if (IsSavegameVersionBefore(SaveLoadVersion::GoalProgressPlaneAcceleration)) v->last_loading_station = StationID::Invalid();

			if (IsSavegameVersionBefore(SaveLoadVersion::BigMap)) {
				/* Convert the current_order.type (which is a mix of type and flags, because
				 *  in those versions, they both were 4 bits big) to type and flags */
				v->current_order.flags = GB(v->current_order.type, 4, 4);
				v->current_order.type &= 0x0F;
			}

			/* Advanced vehicle lists got added */
			if (IsSavegameVersionBefore(SaveLoadVersion::VehicleGroups)) v->group_id = DEFAULT_GROUP;
		}
	}

	void FixPointers() const override
	{
		for (Vehicle *v : Vehicle::Iterate()) {
			SlObject(v, _vehicle_desc);
		}
	}
};

static const VEHSChunkHandler VEHS;
static const ChunkHandlerRef veh_chunk_handlers[] = {
	VEHS,
};

extern const ChunkHandlerTable _veh_chunk_handlers(veh_chunk_handlers);
