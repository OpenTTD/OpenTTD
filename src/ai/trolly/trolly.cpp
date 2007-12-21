/* $Id$ */

/*
 * This AI was created as a direct reaction to the big demand for some good AIs
 * in OTTD. Too bad it never left alpha-stage, and it is considered dead in its
 * current form.
 * By the time of writing this, we, the creator of this AI and a good friend of
 * mine, are designing a whole new AI-system that allows us to create AIs
 * easier and without all the fuzz we encountered while I was working on this
 * AI. By the time that system is finished, you can expect that this AI will
 * dissapear, because it is pretty obselete and bad programmed.
 *
 * Meanwhile I wish you all much fun with this AI; if you are interested as
 * AI-developer in this AI, I advise you not stare too long to some code, some
 * things in here really are... strange ;) But in either way: enjoy :)
 *
 *  -- TrueLight :: 2005-09-01
 */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../debug.h"
#include "../../functions.h"
#include "../../road_map.h"
#include "../../station_map.h"
#include "table/strings.h"
#include "../../map.h"
#include "../../command_func.h"
#include "trolly.h"
#include "../../town.h"
#include "../../industry.h"
#include "../../station.h"
#include "../../engine.h"
#include "../../gui.h"
#include "../../depot.h"
#include "../../vehicle.h"
#include "../../date.h"
#include "../ai.h"
#include "../../order.h"

// This function is called after StartUp. It is the init of an AI
static void AiNew_State_FirstTime(Player *p)
{
	// This assert is used to protect those function from misuse
	//   You have quickly a small mistake in the state-array
	//   With that, everything would go wrong. Finding that, is almost impossible
	//   With this assert, that problem can never happen.
	assert(p->ainew.state == AI_STATE_FIRST_TIME);
	// We first have to init some things

	if (_current_player == 1) ShowErrorMessage(INVALID_STRING_ID, TEMP_AI_IN_PROGRESS, 0, 0);

	// The PathFinder (AyStar)
	// TODO: Maybe when an AI goes bankrupt, this is de-init
	//  or when coming from a savegame.. should be checked out!
	p->ainew.path_info.start_tile_tl = 0;
	p->ainew.path_info.start_tile_br = 0;
	p->ainew.path_info.end_tile_tl = 0;
	p->ainew.path_info.end_tile_br = 0;
	p->ainew.pathfinder = new_AyStar_AiPathFinder(12, &p->ainew.path_info);

	p->ainew.idle = 0;
	p->ainew.last_vehiclecheck_date = _date;

	// We ALWAYS start with a bus route.. just some basic money ;)
	p->ainew.action = AI_ACTION_BUS_ROUTE;

	// Let's popup the news, and after that, start building..
	p->ainew.state = AI_STATE_WAKE_UP;
}


// This function just waste some time
//  It keeps it more real. The AI can build on such tempo no normal user
//  can ever keep up with that. The competitor_speed already delays a bit
//  but after the AI finished a track it really needs to go to sleep.
//
// Let's say, we sleep between one and three days if the AI is put on Very Fast.
//  This means that on Very Slow it will be between 16 and 48 days.. slow enough?
static void AiNew_State_Nothing(Player *p)
{
	assert(p->ainew.state == AI_STATE_NOTHING);
	// If we are done idling, start over again
	if (p->ainew.idle == 0) p->ainew.idle = AI_RandomRange(DAY_TICKS * 2) + DAY_TICKS;
	if (--p->ainew.idle == 0) {
		// We are done idling.. what you say? Let's do something!
		// I mean.. the next tick ;)
		p->ainew.state = AI_STATE_WAKE_UP;
	}
}


// This function picks out a task we are going to do.
//  Currently supported:
//    - Make new route
//    - Check route
//    - Build HQ
static void AiNew_State_WakeUp(Player *p)
{
	int c;
	assert(p->ainew.state == AI_STATE_WAKE_UP);
	// First, check if we have a HQ
	if (p->location_of_house == 0) {
		// We have no HQ yet, build one on a random place
		// Random till we found a place for it!
		// TODO: this should not be on a random place..
		AiNew_Build_CompanyHQ(p, AI_Random() % MapSize());
		// Enough for now, but we want to come back here the next time
		//  so we do not change any status
		return;
	}

	Money money = p->player_money - AI_MINIMUM_MONEY;

	// Let's pick an action!
	if (p->ainew.action == AI_ACTION_NONE) {
		c = AI_Random() & 0xFF;
		if (p->current_loan > 0 &&
				p->old_economy[1].income > AI_MINIMUM_INCOME_FOR_LOAN &&
				c < 10) {
			p->ainew.action = AI_ACTION_REPAY_LOAN;
		} else if (p->ainew.last_vehiclecheck_date + AI_DAYS_BETWEEN_VEHICLE_CHECKS < _date) {
			// Check all vehicles once in a while
			p->ainew.action = AI_ACTION_CHECK_ALL_VEHICLES;
			p->ainew.last_vehiclecheck_date = _date;
		} else if (c < 100 && !_patches.ai_disable_veh_roadveh) {
			// Do we have any spots for road-vehicles left open?
			if (GetFreeUnitNumber(VEH_ROAD) <= _patches.max_roadveh) {
				if (c < 85) {
					p->ainew.action = AI_ACTION_TRUCK_ROUTE;
				} else {
					p->ainew.action = AI_ACTION_BUS_ROUTE;
				}
			}
#if 0
		} else if (c < 200 && !_patches.ai_disable_veh_train) {
			if (GetFreeUnitNumber(VEH_TRAIN) <= _patches.max_trains) {
				p->ainew.action = AI_ACTION_TRAIN_ROUTE;
			}
#endif
		}

		p->ainew.counter = 0;
	}

	if (p->ainew.counter++ > AI_MAX_TRIES_FOR_SAME_ROUTE) {
		p->ainew.action = AI_ACTION_NONE;
		return;
	}

	if (_patches.ai_disable_veh_roadveh && (
				p->ainew.action == AI_ACTION_BUS_ROUTE ||
				p->ainew.action == AI_ACTION_TRUCK_ROUTE
			)) {
		p->ainew.action = AI_ACTION_NONE;
		return;
	}

	if (p->ainew.action == AI_ACTION_REPAY_LOAN &&
			money > AI_MINIMUM_LOAN_REPAY_MONEY) {
		// We start repaying some money..
		p->ainew.state = AI_STATE_REPAY_MONEY;
		return;
	}

	if (p->ainew.action == AI_ACTION_CHECK_ALL_VEHICLES) {
		p->ainew.state = AI_STATE_CHECK_ALL_VEHICLES;
		return;
	}

	// It is useless to start finding a route if we don't have enough money
	//  to build the route anyway..
	if (p->ainew.action == AI_ACTION_BUS_ROUTE &&
			money > AI_MINIMUM_BUS_ROUTE_MONEY) {
		if (GetFreeUnitNumber(VEH_ROAD) > _patches.max_roadveh) {
			p->ainew.action = AI_ACTION_NONE;
			return;
		}
		p->ainew.cargo = AI_NEED_CARGO;
		p->ainew.state = AI_STATE_LOCATE_ROUTE;
		p->ainew.tbt = AI_BUS; // Bus-route
		return;
	}
	if (p->ainew.action == AI_ACTION_TRUCK_ROUTE &&
			money > AI_MINIMUM_TRUCK_ROUTE_MONEY) {
		if (GetFreeUnitNumber(VEH_ROAD) > _patches.max_roadveh) {
			p->ainew.action = AI_ACTION_NONE;
			return;
		}
		p->ainew.cargo = AI_NEED_CARGO;
		p->ainew.last_id = 0;
		p->ainew.state = AI_STATE_LOCATE_ROUTE;
		p->ainew.tbt = AI_TRUCK;
		return;
	}

	p->ainew.state = AI_STATE_NOTHING;
}


static void AiNew_State_ActionDone(Player *p)
{
	p->ainew.action = AI_ACTION_NONE;
	p->ainew.state = AI_STATE_NOTHING;
}


// Check if a city or industry is good enough to start a route there
static bool AiNew_Check_City_or_Industry(Player *p, int ic, byte type)
{
	if (type == AI_CITY) {
		const Town* t = GetTown(ic);
		const Station* st;
		uint count = 0;
		int j = 0;

		// We don't like roadconstructions, don't even true such a city
		if (t->road_build_months != 0) return false;

		// Check if the rating in a city is high enough
		//  If not, take a chance if we want to continue
		if (t->ratings[_current_player] < 0 && AI_CHANCE16(1, 4)) return false;

		if (t->max_pass - t->act_pass < AI_CHECKCITY_NEEDED_CARGO && !AI_CHANCE16(1, AI_CHECKCITY_CITY_CHANCE)) return false;

		// Check if we have build a station in this town the last 6 months
		//  else we don't do it. This is done, because stat updates can be slow
		//  and sometimes it takes up to 4 months before the stats are corectly.
		//  This way we don't get 12 busstations in one city of 100 population ;)
		FOR_ALL_STATIONS(st) {
			// Do we own it?
			if (st->owner == _current_player) {
				// Are we talking busses?
				if (p->ainew.tbt == AI_BUS && (FACIL_BUS_STOP & st->facilities) != FACIL_BUS_STOP) continue;
				// Is it the same city as we are in now?
				if (st->town != t) continue;
				// When was this station build?
				if (_date - st->build_date < AI_CHECKCITY_DATE_BETWEEN) return false;
				// Cound the amount of stations in this city that we own
				count++;
			} else {
				// We do not own it, request some info about the station
				//  we want to know if this station gets the same good. If so,
				//  we want to know its rating. If it is too high, we are not going
				//  to build there
				if (!st->goods[CT_PASSENGERS].last_speed) continue;
				// Is it around our city
				if (DistanceManhattan(st->xy, t->xy) > 10) continue;
				// It does take this cargo.. what is his rating?
				if (st->goods[CT_PASSENGERS].rating < AI_CHECKCITY_CARGO_RATING) continue;
				j++;
				// When this is the first station, we build a second with no problem ;)
				if (j == 1) continue;
				// The rating is high.. second station...
				//  a little chance that we still continue
				//  But if there are 3 stations of this size, we never go on...
				if (j == 2 && AI_CHANCE16(1, AI_CHECKCITY_CARGO_RATING_CHANCE)) continue;
				// We don't like this station :(
				return false;
			}
		}

		// We are about to add one...
		count++;
		// Check if we the city can provide enough cargo for this amount of stations..
		if (count * AI_CHECKCITY_CARGO_PER_STATION > t->max_pass) return false;

		// All check are okay, so we can build here!
		return true;
	}
	if (type == AI_INDUSTRY) {
		const Industry* i = GetIndustry(ic);
		const Station* st;
		int count = 0;
		int j = 0;

		if (i->town != NULL && i->town->ratings[_current_player] < 0 && AI_CHANCE16(1, 4)) return false;

		// No limits on delevering stations!
		//  Or for industry that does not give anything yet
		if (i->produced_cargo[0] == CT_INVALID || i->last_month_production[0] == 0) return true;

		if (i->last_month_production[0] - i->last_month_transported[0] < AI_CHECKCITY_NEEDED_CARGO) return false;

		// Check if we have build a station in this town the last 6 months
		//  else we don't do it. This is done, because stat updates can be slow
		//  and sometimes it takes up to 4 months before the stats are corectly.
		FOR_ALL_STATIONS(st) {
			// Do we own it?
			if (st->owner == _current_player) {
				// Are we talking trucks?
				if (p->ainew.tbt == AI_TRUCK && (FACIL_TRUCK_STOP & st->facilities) != FACIL_TRUCK_STOP) continue;
				// Is it the same city as we are in now?
				if (st->town != i->town) continue;
				// When was this station build?
				if (_date - st->build_date < AI_CHECKCITY_DATE_BETWEEN) return false;
				// Cound the amount of stations in this city that we own
				count++;
			} else {
				// We do not own it, request some info about the station
				//  we want to know if this station gets the same good. If so,
				//  we want to know its rating. If it is too high, we are not going
				//  to build there
				if (i->produced_cargo[0] == CT_INVALID) continue;
				// It does not take this cargo
				if (!st->goods[i->produced_cargo[0]].last_speed) continue;
				// Is it around our industry
				if (DistanceManhattan(st->xy, i->xy) > 5) continue;
				// It does take this cargo.. what is his rating?
				if (st->goods[i->produced_cargo[0]].rating < AI_CHECKCITY_CARGO_RATING) continue;
				j++;
				// The rating is high.. a little chance that we still continue
				//  But if there are 2 stations of this size, we never go on...
				if (j == 1 && AI_CHANCE16(1, AI_CHECKCITY_CARGO_RATING_CHANCE)) continue;
				// We don't like this station :(
				return false;
			}
		}

		// We are about to add one...
		count++;
		// Check if we the city can provide enough cargo for this amount of stations..
		if (count * AI_CHECKCITY_CARGO_PER_STATION > i->last_month_production[0]) return false;

		// All check are okay, so we can build here!
		return true;
	}

	return true;
}


// This functions tries to locate a good route
static void AiNew_State_LocateRoute(Player *p)
{
	assert(p->ainew.state == AI_STATE_LOCATE_ROUTE);
	// For now, we only support PASSENGERS, CITY and BUSSES

	// We don't have a route yet
	if (p->ainew.cargo == AI_NEED_CARGO) {
		p->ainew.new_cost = 0; // No cost yet
		p->ainew.temp = -1;
		// Reset the counter
		p->ainew.counter = 0;

		p->ainew.from_ic = -1;
		p->ainew.to_ic = -1;
		if (p->ainew.tbt == AI_BUS) {
			// For now we only have a passenger route
			p->ainew.cargo = CT_PASSENGERS;

			// Find a route to cities
			p->ainew.from_type = AI_CITY;
			p->ainew.to_type = AI_CITY;
		} else if (p->ainew.tbt == AI_TRUCK) {
			p->ainew.cargo = AI_NO_CARGO;

			p->ainew.from_type = AI_INDUSTRY;
			p->ainew.to_type = AI_INDUSTRY;
		}

		// Now we are doing initing, we wait one tick
		return;
	}

	// Increase the counter and abort if it is taking too long!
	p->ainew.counter++;
	if (p->ainew.counter > AI_LOCATE_ROUTE_MAX_COUNTER) {
		// Switch back to doing nothing!
		p->ainew.state = AI_STATE_NOTHING;
		return;
	}

	// We are going to locate a city from where we are going to connect
	if (p->ainew.from_ic == -1) {
		if (p->ainew.temp == -1) {
			// First, we pick a random spot to search from
			if (p->ainew.from_type == AI_CITY) {
				p->ainew.temp = AI_RandomRange(GetMaxTownIndex() + 1);
			} else {
				p->ainew.temp = AI_RandomRange(GetMaxIndustryIndex() + 1);
			}
		}

		if (!AiNew_Check_City_or_Industry(p, p->ainew.temp, p->ainew.from_type)) {
			// It was not a valid city
			//  increase the temp with one, and return. We will come back later here
			//  to try again
			p->ainew.temp++;
			if (p->ainew.from_type == AI_CITY) {
				if (p->ainew.temp > GetMaxTownIndex()) p->ainew.temp = 0;
			} else {
				if (p->ainew.temp > GetMaxIndustryIndex()) p->ainew.temp = 0;
			}

			// Don't do an attempt if we are trying the same id as the last time...
			if (p->ainew.last_id == p->ainew.temp) return;
			p->ainew.last_id = p->ainew.temp;

			return;
		}

		// We found a good city/industry, save the data of it
		p->ainew.from_ic = p->ainew.temp;

		// Start the next tick with finding a to-city
		p->ainew.temp = -1;
		return;
	}

	// Find a to-city
	if (p->ainew.temp == -1) {
		// First, we pick a random spot to search to
		if (p->ainew.to_type == AI_CITY) {
			p->ainew.temp = AI_RandomRange(GetMaxTownIndex() + 1);
		} else {
			p->ainew.temp = AI_RandomRange(GetMaxIndustryIndex() + 1);
		}
	}

	// The same city is not allowed
	// Also check if the city is valid
	if (p->ainew.temp != p->ainew.from_ic && AiNew_Check_City_or_Industry(p, p->ainew.temp, p->ainew.to_type)) {
		// Maybe it is valid..

		/* We need to know if they are not to far apart from eachother..
		 * We do that by checking how much cargo we have to move and how long the
		 * route is.
		 */

		if (p->ainew.from_type == AI_CITY && p->ainew.tbt == AI_BUS) {
			const Town* town_from = GetTown(p->ainew.from_ic);
			const Town* town_temp = GetTown(p->ainew.temp);
			uint distance = DistanceManhattan(town_from->xy, town_temp->xy);
			int max_cargo;

			max_cargo  = town_from->max_pass + town_temp->max_pass;
			max_cargo -= town_from->act_pass + town_temp->act_pass;

			// max_cargo is now the amount of cargo we can move between the two cities
			// If it is more than the distance, we allow it
			if (distance <= max_cargo * AI_LOCATEROUTE_BUS_CARGO_DISTANCE) {
				// We found a good city/industry, save the data of it
				p->ainew.to_ic = p->ainew.temp;
				p->ainew.state = AI_STATE_FIND_STATION;

				DEBUG(ai, 1, "[LocateRoute] found bus-route of %d tiles long (from %d to %d)",
					distance,
					p->ainew.from_ic,
					p->ainew.temp
				);

				p->ainew.from_tile = 0;
				p->ainew.to_tile = 0;

				return;
			}
		} else if (p->ainew.tbt == AI_TRUCK) {
			const Industry* ind_from = GetIndustry(p->ainew.from_ic);
			const Industry* ind_temp = GetIndustry(p->ainew.temp);
			bool found = false;
			int max_cargo = 0;
			uint i;

			// TODO: in max_cargo, also check other cargo (beside [0])
			// First we check if the from_ic produces cargo that this ic accepts
			if (ind_from->produced_cargo[0] != CT_INVALID && ind_from->last_month_production[0] != 0) {
				for (i = 0; i < lengthof(ind_temp->accepts_cargo); i++) {
					if (ind_temp->accepts_cargo[i] == CT_INVALID) break;
					if (ind_from->produced_cargo[0] == ind_temp->accepts_cargo[i]) {
						// Found a compatible industry
						max_cargo = ind_from->last_month_production[0] - ind_from->last_month_transported[0];
						found = true;
						p->ainew.from_deliver = true;
						p->ainew.to_deliver = false;
						break;
					}
				}
			}
			if (!found && ind_temp->produced_cargo[0] != CT_INVALID && ind_temp->last_month_production[0] != 0) {
				// If not check if the current ic produces cargo that the from_ic accepts
				for (i = 0; i < lengthof(ind_from->accepts_cargo); i++) {
					if (ind_from->accepts_cargo[i] == CT_INVALID) break;
					if (ind_from->produced_cargo[0] == ind_from->accepts_cargo[i]) {
						// Found a compatbiel industry
						found = true;
						max_cargo = ind_temp->last_month_production[0] - ind_temp->last_month_transported[0];
						p->ainew.from_deliver = false;
						p->ainew.to_deliver = true;
						break;
					}
				}
			}
			if (found) {
				// Yeah, they are compatible!!!
				// Check the length against the amount of goods
				uint distance = DistanceManhattan(ind_from->xy, ind_temp->xy);

				if (distance > AI_LOCATEROUTE_TRUCK_MIN_DISTANCE &&
						distance <= max_cargo * AI_LOCATEROUTE_TRUCK_CARGO_DISTANCE) {
					p->ainew.to_ic = p->ainew.temp;
					if (p->ainew.from_deliver) {
						p->ainew.cargo = ind_from->produced_cargo[0];
					} else {
						p->ainew.cargo = ind_temp->produced_cargo[0];
					}
					p->ainew.state = AI_STATE_FIND_STATION;

					DEBUG(ai, 1, "[LocateRoute] found truck-route of %d tiles long (from %d to %d)",
						distance,
						p->ainew.from_ic,
						p->ainew.temp
					);

					p->ainew.from_tile = 0;
					p->ainew.to_tile = 0;

					return;
				}
			}
		}
	}

	// It was not a valid city
	//  increase the temp with one, and return. We will come back later here
	//  to try again
	p->ainew.temp++;
	if (p->ainew.to_type == AI_CITY) {
		if (p->ainew.temp > GetMaxTownIndex()) p->ainew.temp = 0;
	} else {
		if (p->ainew.temp > GetMaxIndustryIndex()) p->ainew.temp = 0;
	}

	// Don't do an attempt if we are trying the same id as the last time...
	if (p->ainew.last_id == p->ainew.temp) return;
	p->ainew.last_id = p->ainew.temp;
}


// Check if there are not more than a certain amount of vehicles pointed to a certain
//  station. This to prevent 10 busses going to one station, which gives... problems ;)
static bool AiNew_CheckVehicleStation(Player *p, Station *st)
{
	int count = 0;
	Vehicle *v;

	// Also check if we don't have already a lot of busses to this city...
	FOR_ALL_VEHICLES(v) {
		if (v->owner == _current_player) {
			const Order *order;

			FOR_VEHICLE_ORDERS(v, order) {
				if (order->type == OT_GOTO_STATION && GetStation(order->dest) == st) {
					// This vehicle has this city in its list
					count++;
				}
			}
		}
	}

	if (count > AI_CHECK_MAX_VEHICLE_PER_STATION) return false;
	return true;
}

// This function finds a good spot for a station
static void AiNew_State_FindStation(Player *p)
{
	TileIndex tile;
	Station *st;
	int count = 0;
	EngineID i;
	TileIndex new_tile = 0;
	DiagDirection direction = DIAGDIR_NE;
	Town *town = NULL;
	assert(p->ainew.state == AI_STATE_FIND_STATION);

	if (p->ainew.from_tile == 0) {
		// First we scan for a station in the from-city
		if (p->ainew.from_type == AI_CITY) {
			town = GetTown(p->ainew.from_ic);
			tile = town->xy;
		} else {
			tile = GetIndustry(p->ainew.from_ic)->xy;
		}
	} else if (p->ainew.to_tile == 0) {
		// Second we scan for a station in the to-city
		if (p->ainew.to_type == AI_CITY) {
			town = GetTown(p->ainew.to_ic);
			tile = town->xy;
		} else {
			tile = GetIndustry(p->ainew.to_ic)->xy;
		}
	} else {
		// Unsupported request
		// Go to FIND_PATH
		p->ainew.temp = -1;
		p->ainew.state = AI_STATE_FIND_PATH;
		return;
	}

	// First, we are going to look at the stations that already exist inside the city
	//  If there is enough cargo left in the station, we take that station
	//  If that is not possible, and there are more than 2 stations in the city, abort
	i = AiNew_PickVehicle(p);
	// Euhmz, this should not happen _EVER_
	// Quit finding a route...
	if (i == INVALID_ENGINE) {
		p->ainew.state = AI_STATE_NOTHING;
		return;
	}

	FOR_ALL_STATIONS(st) {
		if (st->owner == _current_player) {
			if (p->ainew.tbt == AI_BUS && (FACIL_BUS_STOP & st->facilities) == FACIL_BUS_STOP) {
				if (st->town == town) {
					// Check how much cargo there is left in the station
					if ((int)st->goods[p->ainew.cargo].cargo.Count() > RoadVehInfo(i)->capacity * AI_STATION_REUSE_MULTIPLER) {
						if (AiNew_CheckVehicleStation(p, st)) {
							// We did found a station that was good enough!
							new_tile = st->xy;
							direction = GetRoadStopDir(st->xy);
							break;
						}
					}
					count++;
				}
			}
		}
	}
	// We are going to add a new station...
	if (new_tile == 0) count++;
	// No more than 2 stations allowed in a city
	//  This is because only the best 2 stations of one cargo do get any cargo
	if (count > 2) {
		p->ainew.state = AI_STATE_NOTHING;
		return;
	}

	if (new_tile == 0 && p->ainew.tbt == AI_BUS) {
		uint x, y, i = 0;
		CommandCost r;
		uint best;
		uint accepts[NUM_CARGO];
		TileIndex found_spot[AI_FINDSTATION_TILE_RANGE*AI_FINDSTATION_TILE_RANGE * 4];
		uint found_best[AI_FINDSTATION_TILE_RANGE*AI_FINDSTATION_TILE_RANGE * 4];
		// To find a good spot we scan a range from the center, a get the point
		//  where we get the most cargo and where it is buildable.
		// TODO: also check for station of myself and make sure we are not
		//   taking eachothers passengers away (bad result when it does not)
		for (x = TileX(tile) - AI_FINDSTATION_TILE_RANGE; x <= TileX(tile) + AI_FINDSTATION_TILE_RANGE; x++) {
			for (y = TileY(tile) - AI_FINDSTATION_TILE_RANGE; y <= TileY(tile) + AI_FINDSTATION_TILE_RANGE; y++) {
				new_tile = TileXY(x, y);
				if (IsTileType(new_tile, MP_CLEAR) || IsTileType(new_tile, MP_TREES)) {
					// This tile we can build on!
					// Check acceptance
					// XXX - Get the catchment area
					GetAcceptanceAroundTiles(accepts, new_tile, 1, 1, 4);
					// >> 3 == 0 means no cargo
					if (accepts[p->ainew.cargo] >> 3 == 0) continue;
					// See if we can build the station
					r = AiNew_Build_Station(p, p->ainew.tbt, new_tile, 0, 0, 0, DC_QUERY_COST);
					if (CmdFailed(r)) continue;
					// We can build it, so add it to found_spot
					found_spot[i] = new_tile;
					found_best[i++] = accepts[p->ainew.cargo];
				}
			}
		}

		// If i is still zero, we did not find anything
		if (i == 0) {
			p->ainew.state = AI_STATE_NOTHING;
			return;
		}

		// Go through all the found_best and check which has the highest value
		best = 0;
		new_tile = 0;

		for (x = 0; x < i; x++) {
			if (found_best[x] > best ||
					(found_best[x] == best && DistanceManhattan(tile, new_tile) > DistanceManhattan(tile, found_spot[x]))) {
				new_tile = found_spot[x];
				best = found_best[x];
			}
		}

		// See how much it is going to cost us...
		r = AiNew_Build_Station(p, p->ainew.tbt, new_tile, 0, 0, 0, DC_QUERY_COST);
		p->ainew.new_cost += r.GetCost();

		direction = (DiagDirection)AI_PATHFINDER_NO_DIRECTION;
	} else if (new_tile == 0 && p->ainew.tbt == AI_TRUCK) {
		// Truck station locater works differently.. a station can be on any place
		//  as long as it is in range. So we give back code AI_STATION_RANGE
		//  so the pathfinder routine can work it out!
		new_tile = AI_STATION_RANGE;
		direction = (DiagDirection)AI_PATHFINDER_NO_DIRECTION;
	}

	if (p->ainew.from_tile == 0) {
		p->ainew.from_tile = new_tile;
		p->ainew.from_direction = direction;
		// Now we found thisone, go in for to_tile
		return;
	} else if (p->ainew.to_tile == 0) {
		p->ainew.to_tile = new_tile;
		p->ainew.to_direction = direction;
		// K, done placing stations!
		p->ainew.temp = -1;
		p->ainew.state = AI_STATE_FIND_PATH;
		return;
	}
}


// We try to find a path between 2 points
static void AiNew_State_FindPath(Player *p)
{
	int r;
	assert(p->ainew.state == AI_STATE_FIND_PATH);

	// First time, init some data
	if (p->ainew.temp == -1) {
		// Init path_info
		if (p->ainew.from_tile == AI_STATION_RANGE) {
			const Industry* i = GetIndustry(p->ainew.from_ic);

			// For truck routes we take a range around the industry
			p->ainew.path_info.start_tile_tl = i->xy - TileDiffXY(1, 1);
			p->ainew.path_info.start_tile_br = i->xy + TileDiffXY(i->width + 1, i->height + 1);
			p->ainew.path_info.start_direction = p->ainew.from_direction;
		} else {
			p->ainew.path_info.start_tile_tl = p->ainew.from_tile;
			p->ainew.path_info.start_tile_br = p->ainew.from_tile;
			p->ainew.path_info.start_direction = p->ainew.from_direction;
		}

		if (p->ainew.to_tile == AI_STATION_RANGE) {
			const Industry* i = GetIndustry(p->ainew.to_ic);

			p->ainew.path_info.end_tile_tl = i->xy - TileDiffXY(1, 1);
			p->ainew.path_info.end_tile_br = i->xy + TileDiffXY(i->width + 1, i->height + 1);
			p->ainew.path_info.end_direction = p->ainew.to_direction;
		} else {
			p->ainew.path_info.end_tile_tl = p->ainew.to_tile;
			p->ainew.path_info.end_tile_br = p->ainew.to_tile;
			p->ainew.path_info.end_direction = p->ainew.to_direction;
		}

		p->ainew.path_info.rail_or_road = (p->ainew.tbt == AI_TRAIN);

		// First, clean the pathfinder with our new begin and endpoints
		clean_AyStar_AiPathFinder(p->ainew.pathfinder, &p->ainew.path_info);

		p->ainew.temp = 0;
	}

	// Start the pathfinder
	r = p->ainew.pathfinder->main(p->ainew.pathfinder);
	switch (r) {
		case AYSTAR_NO_PATH:
			DEBUG(ai, 1, "No route found by pathfinder");
			// Start all over again
			p->ainew.state = AI_STATE_NOTHING;
			break;

		case AYSTAR_FOUND_END_NODE: // We found the end-point
			p->ainew.temp = -1;
			p->ainew.state = AI_STATE_FIND_DEPOT;
			break;

		// In any other case, we are still busy finding the route
		default: break;
	}
}


// This function tries to locate a good place for a depot!
static void AiNew_State_FindDepot(Player *p)
{
	// To place the depot, we walk through the route, and if we find a lovely spot (MP_CLEAR, MP_TREES), we place it there..
	// Simple, easy, works!
	// To make the depot stand in the middle of the route, we start from the center..
	// But first we walk through the route see if we can find a depot that is ours
	//  this keeps things nice ;)
	int g, i;
	CommandCost r;
	DiagDirection j;
	TileIndex tile;
	assert(p->ainew.state == AI_STATE_FIND_DEPOT);

	p->ainew.depot_tile = 0;

	for (i=2;i<p->ainew.path_info.route_length-2;i++) {
		tile = p->ainew.path_info.route[i];
		for (j = DIAGDIR_BEGIN; j < DIAGDIR_END; j++) {
			TileIndex t = tile + TileOffsByDiagDir(j);

			if (IsTileType(t, MP_ROAD) &&
					GetRoadTileType(t) == ROAD_TILE_DEPOT &&
					IsTileOwner(t, _current_player) &&
					GetRoadDepotDirection(t) == ReverseDiagDir(j)) {
				p->ainew.depot_tile = t;
				p->ainew.depot_direction = ReverseDiagDir(j);
				p->ainew.state = AI_STATE_VERIFY_ROUTE;
				return;
			}
		}
	}

	// This routine let depot finding start in the middle, and work his way to the stations
	// It makes depot placing nicer :)
	i = p->ainew.path_info.route_length / 2;
	g = 1;
	while (i > 1 && i < p->ainew.path_info.route_length - 2) {
		i += g;
		g *= -1;
		(g < 0?g--:g++);

		if (p->ainew.path_info.route_extra[i] != 0 || p->ainew.path_info.route_extra[i+1] != 0) {
			// Bridge or tunnel.. we can't place a depot there
			continue;
		}

		tile = p->ainew.path_info.route[i];

		for (j = DIAGDIR_BEGIN; j < DIAGDIR_END; j++) {
			TileIndex t = tile + TileOffsByDiagDir(j);

			// It may not be placed on the road/rail itself
			// And because it is not build yet, we can't see it on the tile..
			// So check the surrounding tiles :)
			if (t == p->ainew.path_info.route[i - 1] ||
					t == p->ainew.path_info.route[i + 1]) {
				continue;
			}
			// Not around a bridge?
			if (p->ainew.path_info.route_extra[i] != 0) continue;
			if (IsTileType(tile, MP_TUNNELBRIDGE)) continue;
			// Is the terrain clear?
			if (IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)) {
				// If the current tile is on a slope then we do not allow this
				if (GetTileSlope(tile, NULL) != SLOPE_FLAT) continue;
				// Check if everything went okay..
				r = AiNew_Build_Depot(p, t, ReverseDiagDir(j), 0);
				if (CmdFailed(r)) continue;
				// Found a spot!
				p->ainew.new_cost += r.GetCost();
				p->ainew.depot_tile = t;
				p->ainew.depot_direction = ReverseDiagDir(j); // Reverse direction
				p->ainew.state = AI_STATE_VERIFY_ROUTE;
				return;
			}
		}
	}

	// Failed to find a depot?
	p->ainew.state = AI_STATE_NOTHING;
}


// This function calculates how many vehicles there are needed on this
//  traject.
// It works pretty simple: get the length, see how much we move around
//  and hussle that, and you know how many vehicles there are needed.
// It returns the cost for the vehicles
static int AiNew_HowManyVehicles(Player *p)
{
	if (p->ainew.tbt == AI_BUS) {
		// For bus-routes we look at the time before we are back in the station
		EngineID i;
		int length, tiles_a_day;
		int amount;
		i = AiNew_PickVehicle(p);
		if (i == INVALID_ENGINE) return 0;
		// Passenger run.. how long is the route?
		length = p->ainew.path_info.route_length;
		// Calculating tiles a day a vehicle moves is not easy.. this is how it must be done!
		tiles_a_day = RoadVehInfo(i)->max_speed * DAY_TICKS / 256 / 16;
		// We want a vehicle in a station once a month at least, so, calculate it!
		// (the * 2 is because we have 2 stations ;))
		amount = length * 2 * 2 / tiles_a_day / 30;
		if (amount == 0) amount = 1;
		return amount;
	} else if (p->ainew.tbt == AI_TRUCK) {
		// For truck-routes we look at the cargo
		EngineID i;
		int length, amount, tiles_a_day;
		int max_cargo;
		i = AiNew_PickVehicle(p);
		if (i == INVALID_ENGINE) return 0;
		// Passenger run.. how long is the route?
		length = p->ainew.path_info.route_length;
		// Calculating tiles a day a vehicle moves is not easy.. this is how it must be done!
		tiles_a_day = RoadVehInfo(i)->max_speed * DAY_TICKS / 256 / 16;
		if (p->ainew.from_deliver) {
			max_cargo = GetIndustry(p->ainew.from_ic)->last_month_production[0];
		} else {
			max_cargo = GetIndustry(p->ainew.to_ic)->last_month_production[0];
		}

		// This is because moving 60% is more than we can dream of!
		max_cargo *= 6;
		max_cargo /= 10;
		// We want all the cargo to be gone in a month.. so, we know the cargo it delivers
		//  we know what the vehicle takes with him, and we know the time it takes him
		//  to get back here.. now let's do some math!
		amount = 2 * length * max_cargo / tiles_a_day / 30 / RoadVehInfo(i)->capacity;
		amount += 1;
		return amount;
	} else {
		// Currently not supported
		return 0;
	}
}


// This function checks:
//   - If the route went okay
//   - Calculates the amount of money needed to build the route
//   - Calculates how much vehicles needed for the route
static void AiNew_State_VerifyRoute(Player *p)
{
	int res, i;
	assert(p->ainew.state == AI_STATE_VERIFY_ROUTE);

	// Let's calculate the cost of the path..
	//  new_cost already contains the cost of the stations
	p->ainew.path_info.position = -1;

	do {
		p->ainew.path_info.position++;
		p->ainew.new_cost += AiNew_Build_RoutePart(p, &p->ainew.path_info, DC_QUERY_COST).GetCost();
	} while (p->ainew.path_info.position != -2);

	// Now we know the price of build station + path. Now check how many vehicles
	//  we need and what the price for that will be
	res = AiNew_HowManyVehicles(p);
	// If res == 0, no vehicle was found, or an other problem did occour
	if (res == 0) {
		p->ainew.state = AI_STATE_NOTHING;
		return;
	}
	p->ainew.amount_veh = res;
	p->ainew.cur_veh = 0;

	// Check how much it it going to cost us..
	for (i=0;i<res;i++) {
		p->ainew.new_cost += AiNew_Build_Vehicle(p, 0, DC_QUERY_COST).GetCost();
	}

	// Now we know how much the route is going to cost us
	//  Check if we have enough money for it!
	if (p->ainew.new_cost > p->player_money - AI_MINIMUM_MONEY) {
		// Too bad..
		DEBUG(ai, 1, "Insufficient funds to build route (%" OTTD_PRINTF64 "d)", (int64)p->ainew.new_cost);
		p->ainew.state = AI_STATE_NOTHING;
		return;
	}

	// Now we can build the route, check the direction of the stations!
	if (p->ainew.from_direction == AI_PATHFINDER_NO_DIRECTION) {
		p->ainew.from_direction = AiNew_GetDirection(p->ainew.path_info.route[p->ainew.path_info.route_length - 1], p->ainew.path_info.route[p->ainew.path_info.route_length - 2]);
	}
	if (p->ainew.to_direction == AI_PATHFINDER_NO_DIRECTION) {
		p->ainew.to_direction = AiNew_GetDirection(p->ainew.path_info.route[0], p->ainew.path_info.route[1]);
	}
	if (p->ainew.from_tile == AI_STATION_RANGE)
		p->ainew.from_tile = p->ainew.path_info.route[p->ainew.path_info.route_length - 1];
	if (p->ainew.to_tile == AI_STATION_RANGE)
		p->ainew.to_tile = p->ainew.path_info.route[0];

	p->ainew.state = AI_STATE_BUILD_STATION;
	p->ainew.temp = 0;

	DEBUG(ai, 1, "The route is set and buildable, building 0x%X to 0x%X...", p->ainew.from_tile, p->ainew.to_tile);
}


// Build the stations
static void AiNew_State_BuildStation(Player *p)
{
	CommandCost res;
	assert(p->ainew.state == AI_STATE_BUILD_STATION);
	if (p->ainew.temp == 0) {
		if (!IsTileType(p->ainew.from_tile, MP_STATION))
			res = AiNew_Build_Station(p, p->ainew.tbt, p->ainew.from_tile, 0, 0, p->ainew.from_direction, DC_EXEC);
	} else {
		if (!IsTileType(p->ainew.to_tile, MP_STATION))
			res = AiNew_Build_Station(p, p->ainew.tbt, p->ainew.to_tile, 0, 0, p->ainew.to_direction, DC_EXEC);
		p->ainew.state = AI_STATE_BUILD_PATH;
	}
	if (CmdFailed(res)) {
		DEBUG(ai, 0, "[BuildStation] station could not be built (0x%X)", p->ainew.to_tile);
		p->ainew.state = AI_STATE_NOTHING;
		// If the first station _was_ build, destroy it
		if (p->ainew.temp != 0)
			AI_DoCommand(p->ainew.from_tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		return;
	}
	p->ainew.temp++;
}


// Build the path
static void AiNew_State_BuildPath(Player *p)
{
	assert(p->ainew.state == AI_STATE_BUILD_PATH);
	// p->ainew.temp is set to -1 when this function is called for the first time
	if (p->ainew.temp == -1) {
		DEBUG(ai, 1, "Starting to build new path");
		// Init the counter
		p->ainew.counter = (4 - _opt.diff.competitor_speed) * AI_BUILDPATH_PAUSE + 1;
		// Set the position to the startingplace (-1 because in a minute we do ++)
		p->ainew.path_info.position = -1;
		// And don't do this again
		p->ainew.temp = 0;
	}
	// Building goes very fast on normal rate, so we are going to slow it down..
	//  By let the counter count from AI_BUILDPATH_PAUSE to 0, we have a nice way :)
	if (--p->ainew.counter != 0) return;
	p->ainew.counter = (4 - _opt.diff.competitor_speed) * AI_BUILDPATH_PAUSE + 1;

	// Increase the building position
	p->ainew.path_info.position++;
	// Build route
	AiNew_Build_RoutePart(p, &p->ainew.path_info, DC_EXEC);
	if (p->ainew.path_info.position == -2) {
		// This means we are done building!

		if (p->ainew.tbt == AI_TRUCK && !_patches.roadveh_queue) {
			// If they not queue, they have to go up and down to try again at a station...
			// We don't want that, so try building some road left or right of the station
			DiagDirection dir1, dir2, dir3;
			TileIndex tile;
			CommandCost ret;
			for (int i = 0; i < 2; i++) {
				if (i == 0) {
					tile = p->ainew.from_tile + TileOffsByDiagDir(p->ainew.from_direction);
					dir1 = ChangeDiagDir(p->ainew.from_direction, DIAGDIRDIFF_90LEFT);
					dir2 = ChangeDiagDir(p->ainew.from_direction, DIAGDIRDIFF_90RIGHT);
					dir3 = p->ainew.from_direction;
				} else {
					tile = p->ainew.to_tile + TileOffsByDiagDir(p->ainew.to_direction);
					dir1 = ChangeDiagDir(p->ainew.to_direction, DIAGDIRDIFF_90LEFT);
					dir2 = ChangeDiagDir(p->ainew.to_direction, DIAGDIRDIFF_90RIGHT);
					dir3 = p->ainew.to_direction;
				}

				ret = AI_DoCommand(tile, DiagDirToRoadBits(ReverseDiagDir(dir1)), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
				if (CmdSucceeded(ret)) {
					TileIndex offset = TileOffsByDiagDir(dir1);
					if (IsTileType(tile + offset, MP_CLEAR) || IsTileType(tile + offset, MP_TREES)) {
						ret = AI_DoCommand(tile + offset, AiNew_GetRoadDirection(tile, tile + offset, tile + offset + offset), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						if (CmdSucceeded(ret)) {
							if (IsTileType(tile + offset + offset, MP_CLEAR) || IsTileType(tile + offset + offset, MP_TREES))
								AI_DoCommand(tile + offset + offset, AiNew_GetRoadDirection(tile + offset, tile + offset + offset, tile + offset + offset + offset), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						}
					}
				}

				ret = AI_DoCommand(tile, DiagDirToRoadBits(ReverseDiagDir(dir2)), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
				if (CmdSucceeded(ret)) {
					TileIndex offset = TileOffsByDiagDir(dir2);
					if (IsTileType(tile + offset, MP_CLEAR) || IsTileType(tile + offset, MP_TREES)) {
						ret = AI_DoCommand(tile + offset, AiNew_GetRoadDirection(tile, tile + offset, tile + offset + offset), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						if (CmdSucceeded(ret)) {
							if (IsTileType(tile + offset + offset, MP_CLEAR) || IsTileType(tile + offset + offset, MP_TREES))
								AI_DoCommand(tile + offset + offset, AiNew_GetRoadDirection(tile + offset, tile + offset + offset, tile + offset + offset + offset), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						}
					}
				}

				ret = AI_DoCommand(tile, DiagDirToRoadBits(dir3), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
				if (CmdSucceeded(ret)) {
					TileIndex offset = TileOffsByDiagDir(dir3);
					if (IsTileType(tile + offset, MP_CLEAR) || IsTileType(tile + offset, MP_TREES)) {
						ret = AI_DoCommand(tile + offset, AiNew_GetRoadDirection(tile, tile + offset, tile + offset + offset), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						if (CmdSucceeded(ret)) {
							if (IsTileType(tile + offset + offset, MP_CLEAR) || IsTileType(tile + offset + offset, MP_TREES))
								AI_DoCommand(tile + offset + offset, AiNew_GetRoadDirection(tile + offset, tile + offset + offset, tile + offset + offset + offset), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						}
					}
				}
			}
		}

		DEBUG(ai, 1, "Finished building path, cost: %" OTTD_PRINTF64 "d", (int64)p->ainew.new_cost);
		p->ainew.state = AI_STATE_BUILD_DEPOT;
	}
}


// Builds the depot
static void AiNew_State_BuildDepot(Player *p)
{
	CommandCost res;
	assert(p->ainew.state == AI_STATE_BUILD_DEPOT);

	if (IsTileType(p->ainew.depot_tile, MP_ROAD) && GetRoadTileType(p->ainew.depot_tile) == ROAD_TILE_DEPOT) {
		if (IsTileOwner(p->ainew.depot_tile, _current_player)) {
			// The depot is already built
			p->ainew.state = AI_STATE_BUILD_VEHICLE;
			return;
		} else {
			// There is a depot, but not of our team! :(
			p->ainew.state = AI_STATE_NOTHING;
			return;
		}
	}

	// There is a bus on the tile we want to build road on... idle till he is gone! (BAD PERSON! :p)
	if (!EnsureNoVehicleOnGround(p->ainew.depot_tile + TileOffsByDiagDir(p->ainew.depot_direction)))
		return;

	res = AiNew_Build_Depot(p, p->ainew.depot_tile, p->ainew.depot_direction, DC_EXEC);
	if (CmdFailed(res)) {
		DEBUG(ai, 0, "[BuildDepot] depot could not be built (0x%X)", p->ainew.depot_tile);
		p->ainew.state = AI_STATE_NOTHING;
		return;
	}

	p->ainew.state = AI_STATE_BUILD_VEHICLE;
	p->ainew.idle = 10;
	p->ainew.veh_main_id = INVALID_VEHICLE;
}


// Build vehicles
static void AiNew_State_BuildVehicle(Player *p)
{
	CommandCost res;
	assert(p->ainew.state == AI_STATE_BUILD_VEHICLE);

	// Check if we need to build a vehicle
	if (p->ainew.amount_veh == 0) {
		// Nope, we are done!
		// This means: we are all done! The route is open.. go back to NOTHING
		//  He will idle some time and it will all start over again.. :)
		p->ainew.state = AI_STATE_ACTION_DONE;
		return;
	}
	if (--p->ainew.idle != 0) return;
	// It is realistic that the AI can only build 1 vehicle a day..
	// This makes sure of that!
	p->ainew.idle = AI_BUILD_VEHICLE_TIME_BETWEEN;

	// Build the vehicle
	res = AiNew_Build_Vehicle(p, p->ainew.depot_tile, DC_EXEC);
	if (CmdFailed(res)) {
		// This happens when the AI can't build any more vehicles!
		p->ainew.state = AI_STATE_NOTHING;
		return;
	}
	// Increase the current counter
	p->ainew.cur_veh++;
	// Decrease the total counter
	p->ainew.amount_veh--;
	// Go give some orders!
	p->ainew.state = AI_STATE_WAIT_FOR_BUILD;
}


// Put the stations in the order list
static void AiNew_State_GiveOrders(Player *p)
{
	int idx;
	Order order;

	assert(p->ainew.state == AI_STATE_GIVE_ORDERS);

	if (p->ainew.veh_main_id != INVALID_VEHICLE) {
		AI_DoCommand(0, p->ainew.veh_id + (p->ainew.veh_main_id << 16), CO_SHARE, DC_EXEC, CMD_CLONE_ORDER);

		p->ainew.state = AI_STATE_START_VEHICLE;
		return;
	} else {
		p->ainew.veh_main_id = p->ainew.veh_id;
	}

	// Very handy for AI, goto depot.. but yeah, it needs to be activated ;)
	if (_patches.gotodepot) {
		idx = 0;
		order.type = OT_GOTO_DEPOT;
		order.flags = OF_UNLOAD;
		order.dest = GetDepotByTile(p->ainew.depot_tile)->index;
		AI_DoCommand(0, p->ainew.veh_id + (idx << 16), PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);
	}

	idx = 0;
	order.type = OT_GOTO_STATION;
	order.flags = 0;
	order.dest = GetStationIndex(p->ainew.to_tile);
	if (p->ainew.tbt == AI_TRUCK && p->ainew.to_deliver)
		order.flags |= OF_FULL_LOAD;
	AI_DoCommand(0, p->ainew.veh_id + (idx << 16), PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);

	idx = 0;
	order.type = OT_GOTO_STATION;
	order.flags = 0;
	order.dest = GetStationIndex(p->ainew.from_tile);
	if (p->ainew.tbt == AI_TRUCK && p->ainew.from_deliver)
		order.flags |= OF_FULL_LOAD;
	AI_DoCommand(0, p->ainew.veh_id + (idx << 16), PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);

	// Start the engines!
	p->ainew.state = AI_STATE_START_VEHICLE;
}


// Start the vehicle
static void AiNew_State_StartVehicle(Player *p)
{
	assert(p->ainew.state == AI_STATE_START_VEHICLE);

	// Skip the first order if it is a second vehicle
	//  This to make vehicles go different ways..
	if (p->ainew.cur_veh & 1)
		AI_DoCommand(0, p->ainew.veh_id, 1, DC_EXEC, CMD_SKIP_TO_ORDER);

	// 3, 2, 1... go! (give START_STOP command ;))
	AI_DoCommand(0, p->ainew.veh_id, 0, DC_EXEC, CMD_START_STOP_ROADVEH);
	// Try to build an other vehicle (that function will stop building when needed)
	p->ainew.idle  = 10;
	p->ainew.state = AI_STATE_BUILD_VEHICLE;
}


// Repays money
static void AiNew_State_RepayMoney(Player *p)
{
	uint i;

	for (i = 0; i < AI_LOAN_REPAY; i++) {
		AI_DoCommand(0, 0, 0, DC_EXEC, CMD_DECREASE_LOAN);
	}
	p->ainew.state = AI_STATE_ACTION_DONE;
}


static void AiNew_CheckVehicle(Player *p, Vehicle *v)
{
	// When a vehicle is under the 6 months, we don't check for anything
	if (v->age < 180) return;

	// When a vehicle is older then 1 year, it should make money...
	if (v->age > 360) {
		// If both years together are not more than AI_MINIMUM_ROUTE_PROFIT,
		//  it is not worth the line I guess...
		if (v->profit_last_year + v->profit_this_year < AI_MINIMUM_ROUTE_PROFIT ||
				(v->reliability * 100 >> 16) < 40) {
			// There is a possibility that the route is fucked up...
			if (v->cargo.DaysInTransit() > AI_VEHICLE_LOST_DAYS) {
				// The vehicle is lost.. check the route, or else, get the vehicle
				//  back to a depot
				// TODO: make this piece of code
			}


			// We are already sending him back
			if (AiNew_GetSpecialVehicleFlag(p, v) & AI_VEHICLEFLAG_SELL) {
				if (v->type == VEH_ROAD && IsTileDepotType(v->tile, TRANSPORT_ROAD) &&
						(v->vehstatus&VS_STOPPED)) {
					// We are at the depot, sell the vehicle
					AI_DoCommand(0, v->index, 0, DC_EXEC, CMD_SELL_ROAD_VEH);
				}
				return;
			}

			if (!AiNew_SetSpecialVehicleFlag(p, v, AI_VEHICLEFLAG_SELL)) return;
			{
				CommandCost ret;
				if (v->type == VEH_ROAD)
					ret = AI_DoCommand(0, v->index, 0, DC_EXEC, CMD_SEND_ROADVEH_TO_DEPOT);
				// This means we can not find a depot :s
				//				if (CmdFailed(ret))
			}
		}
	}
}


// Checks all vehicles if they are still valid and make money and stuff
static void AiNew_State_CheckAllVehicles(Player *p)
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->owner != p->index) continue;
		// Currently, we only know how to handle road-vehicles
		if (v->type != VEH_ROAD) continue;

		AiNew_CheckVehicle(p, v);
	}

	p->ainew.state = AI_STATE_ACTION_DONE;
}


// Using the technique simular to the original AI
//   Keeps things logical
// It really should be in the same order as the AI_STATE's are!
static AiNew_StateFunction* const _ainew_state[] = {
	NULL,
	AiNew_State_FirstTime,
	AiNew_State_Nothing,
	AiNew_State_WakeUp,
	AiNew_State_LocateRoute,
	AiNew_State_FindStation,
	AiNew_State_FindPath,
	AiNew_State_FindDepot,
	AiNew_State_VerifyRoute,
	AiNew_State_BuildStation,
	AiNew_State_BuildPath,
	AiNew_State_BuildDepot,
	AiNew_State_BuildVehicle,
	NULL,
	AiNew_State_GiveOrders,
	AiNew_State_StartVehicle,
	AiNew_State_RepayMoney,
	AiNew_State_CheckAllVehicles,
	AiNew_State_ActionDone,
	NULL,
};

static void AiNew_OnTick(Player *p)
{
	if (_ainew_state[p->ainew.state] != NULL)
		_ainew_state[p->ainew.state](p);
}


void AiNewDoGameLoop(Player *p)
{
	if (p->ainew.state == AI_STATE_STARTUP) {
		// The AI just got alive!
		p->ainew.state = AI_STATE_FIRST_TIME;
		p->ainew.tick = 0;

		// Only startup the AI
		return;
	}

	// We keep a ticker. We use it for competitor_speed
	p->ainew.tick++;

	// If we come here, we can do a tick.. do so!
	AiNew_OnTick(p);
}
