/*
 * Next part is in Dutch, and only here for me, TrueLight, the maker of this new AI
 */

// TODO: als iemand een vehicle stil zet op een weg waar de AI wil bouwen
//         doet de AI helemaal niets meer
// TODO: depot rondjes rijden stom iets dingus
// TODO: jezelf afvragen of competitor_intelligence op niveau 2 wel meer geld moet opleverne...
// TODO: als er iets in path komt, bouwt AI gewoon verder :(
// TODO: mail routes

/*
 * End of Dutch part
 */

#include "stdafx.h"
#include "ttd.h"
#include "debug.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "command.h"
#include "ai.h"
#include "town.h"
#include "industry.h"
#include "station.h"
#include "engine.h"
#include "gui.h"

// This function is called after StartUp. It is the init of an AI
static void AiNew_State_FirstTime(Player *p) {
    // This assert is used to protect those function from misuse
    //   You have quickly a small mistake in the state-array
    //   With that, everything would go wrong. Finding that, is almost impossible
    //   With this assert, that problem can never happen.
    assert(p->ainew.state == AI_STATE_FIRST_TIME);
	// We first have to init some things

	if (_current_player == 1) {
		ShowErrorMessage(-1, TEMP_AI_IN_PROGRESS, 0, 0);
	}

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
static void AiNew_State_Nothing(Player *p) {
    assert(p->ainew.state == AI_STATE_NOTHING);
    // If we are done idling, start over again
	if (p->ainew.idle == 0) p->ainew.idle = RandomRange(DAY_TICKS * 2) + DAY_TICKS;
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
static void AiNew_State_WakeUp(Player *p) {
    int32 money;
    int c;
    assert(p->ainew.state == AI_STATE_WAKE_UP);
	// First, check if we have a HQ
	if (p->location_of_house == 0) {
		// We have no HQ yet, build one on a random place
		// Random till we found a place for it!
		// TODO: this should not be on a random place..
		while (!AiNew_Build_CompanyHQ(p, Random() % MapSize())) { }
		// Enough for now, but we want to come back here the next time
		//  so we do not change any status
		return;
	}

	money = p->player_money - AI_MINIMUM_MONEY;

	// Let's pick an action!
	if (p->ainew.action == AI_ACTION_NONE) {
		c = Random() & 0xFF;
		if (p->current_loan > 0 && p->old_economy[1].income > AI_MINIMUM_INCOME_FOR_LOAN &&
  			c < 10) {
  			p->ainew.action = AI_ACTION_REPAY_LOAN;
		} else if (p->ainew.last_vehiclecheck_date + AI_DAYS_BETWEEN_VEHICLE_CHECKS < _date) {
			// Check all vehicles once in a while
			p->ainew.action = AI_ACTION_CHECK_ALL_VEHICLES;
			p->ainew.last_vehiclecheck_date = _date;
		} else if (c < 100 && !_patches.ai_disable_veh_roadveh) {
			// Do we have any spots for road-vehicles left open?
			if (GetFreeUnitNumber(VEH_Road) <= _patches.max_roadveh) {
				if (c < 85) p->ainew.action = AI_ACTION_TRUCK_ROUTE;
				else p->ainew.action = AI_ACTION_BUS_ROUTE;
			}
		}/* else if (c < 200 && !_patches.ai_disable_veh_train) {
			if (GetFreeUnitNumber(VEH_Train) <= _patches.max_trains) {
				p->ainew.action = AI_ACTION_TRAIN_ROUTE;
			}
		}*/

		p->ainew.counter = 0;
	}

	if (p->ainew.counter++ > AI_MAX_TRIES_FOR_SAME_ROUTE) {
		p->ainew.action = AI_ACTION_NONE;
		return;
	}

	if (_patches.ai_disable_veh_roadveh && (
		p->ainew.action == AI_ACTION_BUS_ROUTE || p->ainew.action == AI_ACTION_TRUCK_ROUTE)) {
		p->ainew.action = AI_ACTION_NONE;
		return;
	}

	if (_patches.ai_disable_veh_roadveh && (
		p->ainew.action == AI_ACTION_BUS_ROUTE || p->ainew.action == AI_ACTION_TRUCK_ROUTE)) {
		p->ainew.action = AI_ACTION_NONE;
		return;
	}

	if (p->ainew.action == AI_ACTION_REPAY_LOAN && money > AI_MINIMUM_LOAN_REPAY_MONEY) {
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
	if (p->ainew.action == AI_ACTION_BUS_ROUTE && money > AI_MINIMUM_BUS_ROUTE_MONEY) {
		if (GetFreeUnitNumber(VEH_Road) > _patches.max_roadveh) {
			p->ainew.action = AI_ACTION_NONE;
			return;
		}
		p->ainew.cargo = AI_NEED_CARGO;
		p->ainew.state = AI_STATE_LOCATE_ROUTE;
		p->ainew.tbt = AI_BUS; // Bus-route
		return;
	}
	if (p->ainew.action == AI_ACTION_TRUCK_ROUTE && money > AI_MINIMUM_TRUCK_ROUTE_MONEY) {
		if (GetFreeUnitNumber(VEH_Road) > _patches.max_roadveh) {
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

static void AiNew_State_ActionDone(Player *p) {
    p->ainew.action = AI_ACTION_NONE;
    p->ainew.state = AI_STATE_NOTHING;
}

// Check if a city or industry is good enough to start a route there
static bool AiNew_Check_City_or_Industry(Player *p, int ic, byte type) {
	if (type == AI_CITY) {
		Town *t = GetTown(ic);
		Station *st;
		int count = 0;
		int j = 0;

		// We don't like roadconstructions, don't even true such a city
		if (t->road_build_months != 0) return false;

		// Check if the rating in a city is high enough
		//  If not, take a chance if we want to continue
		if (t->ratings[_current_player] < 0 && CHANCE16(1,4)) return false;

		if (t->max_pass - t->act_pass < AI_CHECKCITY_NEEDED_CARGO && !CHANCE16(1,AI_CHECKCITY_CITY_CHANCE)) return false;

		// Check if we have build a station in this town the last 6 months
		//  else we don't do it. This is done, because stat updates can be slow
		//  and sometimes it takes up to 4 months before the stats are corectly.
		//  This way we don't get 12 busstations in one city of 100 population ;)
		FOR_ALL_STATIONS(st) {
			// Is it an active station
			if (st->xy == 0) continue;
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
				if (j == 2 && CHANCE16(1, AI_CHECKCITY_CARGO_RATING_CHANCE)) continue;
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
		Industry *i = GetIndustry(ic);
		Station *st;
		int count = 0;
		int j = 0;

		if (i->town != NULL && i->town->ratings[_current_player] < 0 && CHANCE16(1,4)) return false;

		// No limits on delevering stations!
		//  Or for industry that does not give anything yet
		if (i->produced_cargo[0] == 0xFF || i->total_production[0] == 0) return true;

		if (i->total_production[0] - i->total_transported[0] < AI_CHECKCITY_NEEDED_CARGO) return false;

		// Check if we have build a station in this town the last 6 months
		//  else we don't do it. This is done, because stat updates can be slow
		//  and sometimes it takes up to 4 months before the stats are corectly.
		FOR_ALL_STATIONS(st) {
			// Is it an active station
			if (st->xy == 0) continue;

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
				if (i->produced_cargo[0] == 0xFF) continue;
				// It does not take this cargo
				if (!st->goods[i->produced_cargo[0]].last_speed) continue;
				// Is it around our industry
				if (DistanceManhattan(st->xy, i->xy) > 5) continue;
				// It does take this cargo.. what is his rating?
				if (st->goods[i->produced_cargo[0]].rating < AI_CHECKCITY_CARGO_RATING) continue;
				j++;
				// The rating is high.. a little chance that we still continue
				//  But if there are 2 stations of this size, we never go on...
				if (j == 1 && CHANCE16(1, AI_CHECKCITY_CARGO_RATING_CHANCE)) continue;
				// We don't like this station :(
				return false;
			}
		}

		// We are about to add one...
		count++;
		// Check if we the city can provide enough cargo for this amount of stations..
		if (count * AI_CHECKCITY_CARGO_PER_STATION > i->total_production[0]) return false;

		// All check are okay, so we can build here!
		return true;
	}

	return true;
}

// This functions tries to locate a good route
static void AiNew_State_LocateRoute(Player *p) {
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
        	if (p->ainew.from_type == AI_CITY)
        		p->ainew.temp = RandomRange(_total_towns);
       		else
        		p->ainew.temp = RandomRange(_total_industries);
        }

    	if (!AiNew_Check_City_or_Industry(p, p->ainew.temp, p->ainew.from_type)) {
    		// It was not a valid city
    		//  increase the temp with one, and return. We will come back later here
    		//  to try again
    		p->ainew.temp++;
				if (p->ainew.from_type == AI_CITY) {
					if (p->ainew.temp >= (int)_total_towns) p->ainew.temp = 0;
				} else {
					if (p->ainew.temp >= _total_industries) p->ainew.temp = 0;
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
       	if (p->ainew.to_type == AI_CITY)
       		p->ainew.temp = RandomRange(_total_towns);
       	else
       		p->ainew.temp = RandomRange(_total_industries);
	}

	// The same city is not allowed
	// Also check if the city is valid
   	if (p->ainew.temp != p->ainew.from_ic && AiNew_Check_City_or_Industry(p, p->ainew.temp, p->ainew.to_type)) {
   		// Maybe it is valid..

   		// We need to know if they are not to far apart from eachother..
   		// We do that by checking how much cargo we have to move and how long the route
   		//   is.

   		if (p->ainew.from_type == AI_CITY && p->ainew.tbt == AI_BUS) {
   			int max_cargo = GetTown(p->ainew.from_ic)->max_pass + GetTown(p->ainew.temp)->max_pass;
   			max_cargo -= GetTown(p->ainew.from_ic)->act_pass + GetTown(p->ainew.temp)->act_pass;
   			// max_cargo is now the amount of cargo we can move between the two cities
   			// If it is more than the distance, we allow it
   			if (DistanceManhattan(GetTown(p->ainew.from_ic)->xy, GetTown(p->ainew.temp)->xy) <= max_cargo * AI_LOCATEROUTE_BUS_CARGO_DISTANCE) {
   				// We found a good city/industry, save the data of it
   				p->ainew.to_ic = p->ainew.temp;
   				p->ainew.state = AI_STATE_FIND_STATION;

   				DEBUG(ai,1)(
						"[AiNew - LocateRoute] Found bus-route of %d tiles long (from %d to %d)",
						DistanceManhattan(GetTown(p->ainew.from_ic)->xy, GetTown(p->ainew.temp)->xy),
						p->ainew.from_ic,
						p->ainew.temp
					);

   				p->ainew.from_tile = 0;
   				p->ainew.to_tile = 0;

   				return;
   			}
   		} else if (p->ainew.tbt == AI_TRUCK) {
         	bool found = false;
         	int max_cargo = 0;
         	int i;
         	// TODO: in max_cargo, also check other cargo (beside [0])
         	// First we check if the from_ic produces cargo that this ic accepts
         	if (GetIndustry(p->ainew.from_ic)->produced_cargo[0] != 0xFF && GetIndustry(p->ainew.from_ic)->total_production[0] != 0) {
	         	for (i=0;i<3;i++) {
	         		if (GetIndustry(p->ainew.temp)->accepts_cargo[i] == 0xFF) break;
					if (GetIndustry(p->ainew.from_ic)->produced_cargo[0] == GetIndustry(p->ainew.temp)->accepts_cargo[i]) {
						// Found a compatbiel industry
						max_cargo = GetIndustry(p->ainew.from_ic)->total_production[0] - GetIndustry(p->ainew.from_ic)->total_transported[0];
						found = true;
	   					p->ainew.from_deliver = true;
	   					p->ainew.to_deliver = false;
	       				break;
	       			}
	       		}
   			}
   			if (!found && GetIndustry(p->ainew.temp)->produced_cargo[0] != 0xFF && GetIndustry(p->ainew.temp)->total_production[0] != 0) {
   				// If not check if the current ic produces cargo that the from_ic accepts
	         	for (i=0;i<3;i++) {
	         		if (GetIndustry(p->ainew.from_ic)->accepts_cargo[i] == 0xFF) break;
					if (GetIndustry(p->ainew.temp)->produced_cargo[0] == GetIndustry(p->ainew.from_ic)->accepts_cargo[i]) {
						// Found a compatbiel industry
						found = true;
						max_cargo = GetIndustry(p->ainew.temp)->total_production[0] - GetIndustry(p->ainew.from_ic)->total_transported[0];
	   					p->ainew.from_deliver = false;
	   					p->ainew.to_deliver = true;
	       				break;
	       			}
	   			}
   			}
   			if (found) {
   				// Yeah, they are compatible!!!
   				// Check the length against the amount of goods
   				if (DistanceManhattan(GetIndustry(p->ainew.from_ic)->xy, GetIndustry(p->ainew.temp)->xy) > AI_LOCATEROUTE_TRUCK_MIN_DISTANCE &&
       				DistanceManhattan(GetIndustry(p->ainew.from_ic)->xy, GetIndustry(p->ainew.temp)->xy) <= max_cargo * AI_LOCATEROUTE_TRUCK_CARGO_DISTANCE) {
	   				p->ainew.to_ic = p->ainew.temp;
	   				if (p->ainew.from_deliver) {
	   					p->ainew.cargo = GetIndustry(p->ainew.from_ic)->produced_cargo[0];
	   				} else {
   						p->ainew.cargo = GetIndustry(p->ainew.temp)->produced_cargo[0];
   					}
	   				p->ainew.state = AI_STATE_FIND_STATION;

	   				DEBUG(ai,1)(
							"[AiNew - LocateRoute] Found truck-route of %d tiles long (from %d to %d)",
							DistanceManhattan(GetIndustry(p->ainew.from_ic)->xy, GetIndustry(p->ainew.temp)->xy),
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
		if (p->ainew.temp >= (int)_total_towns) p->ainew.temp = 0;
	} else {
		if (p->ainew.temp >= _total_industries) p->ainew.temp = 0;
	}

   	// Don't do an attempt if we are trying the same id as the last time...
   	if (p->ainew.last_id == p->ainew.temp) return;
   	p->ainew.last_id = p->ainew.temp;
}

// Check if there are not more than a certain amount of vehicles pointed to a certain
//  station. This to prevent 10 busses going to one station, which gives... problems ;)
static bool AiNew_CheckVehicleStation(Player *p, Station *st) {
	int count = 0;
	Vehicle *v;

	// Also check if we don't have already a lot of busses to this city...
	FOR_ALL_VEHICLES(v) {
		if (v->owner == _current_player) {
			const Order *order;

			FOR_VEHICLE_ORDERS(v, order) {
				if (order->type == OT_GOTO_STATION && GetStation(order->station) == st) {
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
static void AiNew_State_FindStation(Player *p) {
    TileIndex tile;
    Station *st;
    int i, count = 0;
    TileIndex new_tile = 0;
    byte direction = 0;
    Town *town = NULL;
    Industry *industry = NULL;
    assert(p->ainew.state == AI_STATE_FIND_STATION);

    if (p->ainew.from_tile == 0) {
        // First we scan for a station in the from-city
        if (p->ainew.from_type == AI_CITY) {
        	town = GetTown(p->ainew.from_ic);
        	tile = town->xy;
        } else {
        	industry = GetIndustry(p->ainew.from_ic);
        	tile = industry->xy;
        }
    } else if (p->ainew.to_tile == 0) {
    	// Second we scan for a station in the to-city
        if (p->ainew.to_type == AI_CITY) {
        	town = GetTown(p->ainew.to_ic);
        	tile = town->xy;
        } else {
        	industry = GetIndustry(p->ainew.to_ic);
        	tile = industry->xy;
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
	if (i == -1) { p->ainew.state = AI_STATE_NOTHING; return; }

	FOR_ALL_STATIONS(st) {
		if (st->xy != 0) {
			if (st->owner == _current_player) {
				if (p->ainew.tbt == AI_BUS && (FACIL_BUS_STOP & st->facilities) == FACIL_BUS_STOP) {
					if (st->town == town) {
						// Check how much cargo there is left in the station
						if ((st->goods[p->ainew.cargo].waiting_acceptance & 0xFFF) > RoadVehInfo(i)->capacity * AI_STATION_REUSE_MULTIPLER) {
							if (AiNew_CheckVehicleStation(p, st)) {
								// We did found a station that was good enough!
								new_tile = st->xy;
								// Cheap way to get the direction of the station...
								//  Bus stations save it as 0x47 .. 0x4A, so decrease it with 0x47, and tada!
								direction = _map5[st->xy] - 0x47;
								break;
							}
						}
						count++;
					}
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
	    int r;
	    uint best;
	    uint accepts[NUM_CARGO];
	    TileIndex found_spot[AI_FINDSTATION_TILE_RANGE*AI_FINDSTATION_TILE_RANGE*4];
	    uint found_best[AI_FINDSTATION_TILE_RANGE*AI_FINDSTATION_TILE_RANGE*4];
	    // To find a good spot we scan a range from the center, a get the point
	    //  where we get the most cargo and where it is buildable.
	    // TODO: also check for station of myself and make sure we are not
	    //   taking eachothers passangers away (bad result when it does not)
	    for (x = TileX(tile) - AI_FINDSTATION_TILE_RANGE; x <= TileX(tile) + AI_FINDSTATION_TILE_RANGE; x++) {
	    	for (y = TileY(tile) - AI_FINDSTATION_TILE_RANGE; y <= TileY(tile) + AI_FINDSTATION_TILE_RANGE; y++) {
	    		new_tile = TILE_XY(x,y);
	    		if (IsTileType(new_tile, MP_CLEAR) || IsTileType(new_tile, MP_TREES)) {
	    			// This tile we can build on!
	    			// Check acceptance
						// XXX - Get the catchment area
	    			GetAcceptanceAroundTiles(accepts, new_tile, 1, 1, 4);
	    			// >> 3 == 0 means no cargo
	    			if (accepts[p->ainew.cargo] >> 3 == 0) continue;
	    			// See if we can build the station
   	    			r = AiNew_Build_Station(p, p->ainew.tbt, new_tile, 0, 0, 0, DC_QUERY_COST);
   	    			if (r == CMD_ERROR) continue;
	    			// We can build it, so add it to found_spot
	    			found_spot[i] = new_tile;
	    			found_best[i++] = accepts[p->ainew.cargo];
	    		}
	    	}
	    }

	    // If i is still zero, we did not found anything :(
	    if (i == 0) {
	    	p->ainew.state = AI_STATE_NOTHING;
	    	return;
	    }

	    // Go through all the found_best and check which has the highest value
	    best = 0;
	    new_tile = 0;

	    for (x=0;x<i;x++) {
	    	if (found_best[x] > best ||
	     		(found_best[x] == best && DistanceManhattan(tile, new_tile) > DistanceManhattan(tile, found_spot[x]))) {
	     		new_tile = found_spot[x];
	     		best = found_best[x];
	    	}
	    }

		// See how much it is going to cost us...
		r = AiNew_Build_Station(p, p->ainew.tbt, new_tile, 0, 0, 0, DC_QUERY_COST);
		p->ainew.new_cost += r;

		direction = AI_PATHFINDER_NO_DIRECTION;
	} else if (new_tile == 0 && p->ainew.tbt == AI_TRUCK) {
		// Truck station locater works differently.. a station can be on any place
		//  as long as it is in range. So we give back code AI_STATION_RANGE
		//  so the pathfinder routine can work it out!
		new_tile = AI_STATION_RANGE;
		direction = AI_PATHFINDER_NO_DIRECTION;
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
static void AiNew_State_FindPath(Player *p) {
    int r;
    assert(p->ainew.state == AI_STATE_FIND_PATH);

    // First time, init some data
    if (p->ainew.temp == -1) {
    	// Init path_info
    	if (p->ainew.from_tile == AI_STATION_RANGE) {
    		// For truck routes we take a range around the industry
	    	p->ainew.path_info.start_tile_tl = GetIndustry(p->ainew.from_ic)->xy - TILE_XY(1,1);
	    	p->ainew.path_info.start_tile_br = GetIndustry(p->ainew.from_ic)->xy + TILE_XY(GetIndustry(p->ainew.from_ic)->width, GetIndustry(p->ainew.from_ic)->height) + TILE_XY(1,1);
	    	p->ainew.path_info.start_direction = p->ainew.from_direction;
	    } else {
	    	p->ainew.path_info.start_tile_tl = p->ainew.from_tile;
	    	p->ainew.path_info.start_tile_br = p->ainew.from_tile;
	    	p->ainew.path_info.start_direction = p->ainew.from_direction;
	    }

	    if (p->ainew.to_tile == AI_STATION_RANGE) {
	    	p->ainew.path_info.end_tile_tl = GetIndustry(p->ainew.to_ic)->xy - TILE_XY(1,1);
	    	p->ainew.path_info.end_tile_br = GetIndustry(p->ainew.to_ic)->xy + TILE_XY(GetIndustry(p->ainew.to_ic)->width, GetIndustry(p->ainew.to_ic)->height) + TILE_XY(1,1);
	   	    p->ainew.path_info.end_direction = p->ainew.to_direction;
    	} else {
	   	    p->ainew.path_info.end_tile_tl = p->ainew.to_tile;
	   	    p->ainew.path_info.end_tile_br = p->ainew.to_tile;
	   	    p->ainew.path_info.end_direction = p->ainew.to_direction;
	   	}

		if (p->ainew.tbt == AI_TRAIN)
			p->ainew.path_info.rail_or_road = true;
	   	else
   			p->ainew.path_info.rail_or_road = false;

		// First, clean the pathfinder with our new begin and endpoints
        clean_AyStar_AiPathFinder(p->ainew.pathfinder, &p->ainew.path_info);

   		p->ainew.temp = 0;
	}

    // Start the pathfinder
	r = p->ainew.pathfinder->main(p->ainew.pathfinder);
	// If it return: no match, stop it...
	if (r == AYSTAR_NO_PATH) {
		DEBUG(ai,1)("[AiNew] PathFinder found no route!");
		// Start all over again...
		p->ainew.state = AI_STATE_NOTHING;
		return;
	}
	if (r == AYSTAR_FOUND_END_NODE) {
		// We found the end-point
		p->ainew.temp = -1;
		p->ainew.state = AI_STATE_FIND_DEPOT;
		return;
	}
	// In any other case, we are still busy finding the route...
}

// This function tries to locate a good place for a depot!
static void AiNew_State_FindDepot(Player *p) {
    // To place the depot, we walk through the route, and if we find a lovely spot (MP_CLEAR, MP_TREES), we place it there..
    // Simple, easy, works!
    // To make the depot stand in the middle of the route, we start from the center..
    // But first we walk through the route see if we can find a depot that is ours
    //  this keeps things nice ;)
	int g, i, j, r;
	TileIndex tile;
	assert(p->ainew.state == AI_STATE_FIND_DEPOT);

	p->ainew.depot_tile = 0;

	for (i=2;i<p->ainew.path_info.route_length-2;i++) {
		tile = p->ainew.path_info.route[i];
		for (j = 0; j < 4; j++) {
			if (IsTileType(tile + TileOffsByDir(j), MP_STREET)) {
				// Its a street, test if it is a depot
				if (_map5[tile + TileOffsByDir(j)] & 0x20) {
					// We found a depot, is it ours? (TELL ME!!!)
					if (_map_owner[tile + TileOffsByDir(j)] == _current_player) {
						// Now, is it pointing to the right direction.........
						if ((_map5[tile + TileOffsByDir(j)] & 3) == (j ^ 2)) {
							// Yeah!!!
							p->ainew.depot_tile = tile + TileOffsByDir(j);
							p->ainew.depot_direction = j ^ 2; // Reverse direction
							p->ainew.state = AI_STATE_VERIFY_ROUTE;
							return;
						}
					}
				}
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

		for (j = 0; j < 4; j++) {
			// It may not be placed on the road/rail itself
			// And because it is not build yet, we can't see it on the tile..
			// So check the surrounding tiles :)
			if (tile + TileOffsByDir(j) == p->ainew.path_info.route[i-1] ||
					tile + TileOffsByDir(j) == p->ainew.path_info.route[i+1]) continue;
			// Not around a bridge?
			if (p->ainew.path_info.route_extra[i] != 0) continue;
			if (IsTileType(tile, MP_TUNNELBRIDGE)) continue;
			// Is the terrain clear?
			if (IsTileType(tile + TileOffsByDir(j), MP_CLEAR) ||
					IsTileType(tile + TileOffsByDir(j), MP_TREES)) {
				TileInfo ti;
				FindLandscapeHeightByTile(&ti, tile);
				// If the current tile is on a slope (tileh != 0) then we do not allow this
				if (ti.tileh != 0) continue;
				// Check if everything went okay..
				r = AiNew_Build_Depot(p, tile + TileOffsByDir(j), j ^ 2, 0);
				if (r == CMD_ERROR) continue;
				// Found a spot!
				p->ainew.new_cost += r;
				p->ainew.depot_tile = tile + TileOffsByDir(j);
				p->ainew.depot_direction = j ^ 2; // Reverse direction
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
static int AiNew_HowManyVehicles(Player *p) {
	if (p->ainew.tbt == AI_BUS) {
     	// For bus-routes we look at the time before we are back in the station
		int i, length, tiles_a_day;
		int amount;
		i = AiNew_PickVehicle(p);
		if (i == -1) return 0;
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
		int i, length, amount, tiles_a_day;
		int max_cargo;
		i = AiNew_PickVehicle(p);
		if (i == -1) return 0;
    	// Passenger run.. how long is the route?
    	length = p->ainew.path_info.route_length;
    	// Calculating tiles a day a vehicle moves is not easy.. this is how it must be done!
    	tiles_a_day = RoadVehInfo(i)->max_speed * DAY_TICKS / 256 / 16;
    	if (p->ainew.from_deliver)
    		max_cargo = GetIndustry(p->ainew.from_ic)->total_production[0];
    	else
    		max_cargo = GetIndustry(p->ainew.to_ic)->total_production[0];

    	// This is because moving 60% is more than we can dream of!
    	max_cargo *= 0.6;
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
static void AiNew_State_VerifyRoute(Player *p) {
    int res, i;
    assert(p->ainew.state == AI_STATE_VERIFY_ROUTE);

    // Let's calculate the cost of the path..
    //  new_cost already contains the cost of the stations
    p->ainew.path_info.position = -1;

    do {
	    p->ainew.path_info.position++;
	    p->ainew.new_cost += AiNew_Build_RoutePart(p, &p->ainew.path_info, DC_QUERY_COST);
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
		p->ainew.new_cost += AiNew_Build_Vehicle(p, 0, DC_QUERY_COST);
	}

	// Now we know how much the route is going to cost us
	//  Check if we have enough money for it!
	if (p->ainew.new_cost > p->player_money - AI_MINIMUM_MONEY) {
		// Too bad..
		DEBUG(ai,1)("[AiNew] Can't pay for this route (%d)", p->ainew.new_cost);
		p->ainew.state = AI_STATE_NOTHING;
		return;
	}

	// Now we can build the route, check the direction of the stations!
	if (p->ainew.from_direction == AI_PATHFINDER_NO_DIRECTION) {
		p->ainew.from_direction = AiNew_GetDirection(p->ainew.path_info.route[p->ainew.path_info.route_length-1], p->ainew.path_info.route[p->ainew.path_info.route_length-2]);
	}
	if (p->ainew.to_direction == AI_PATHFINDER_NO_DIRECTION) {
		p->ainew.to_direction = AiNew_GetDirection(p->ainew.path_info.route[0], p->ainew.path_info.route[1]);
	}
	if (p->ainew.from_tile == AI_STATION_RANGE)
		p->ainew.from_tile = p->ainew.path_info.route[p->ainew.path_info.route_length-1];
	if (p->ainew.to_tile == AI_STATION_RANGE)
		p->ainew.to_tile = p->ainew.path_info.route[0];

	p->ainew.state = AI_STATE_BUILD_STATION;
	p->ainew.temp = 0;

	DEBUG(ai,1)("[AiNew] The route is set and buildable.. going to build it!");
}

// Build the stations
static void AiNew_State_BuildStation(Player *p) {
	int res = 0;
    assert(p->ainew.state == AI_STATE_BUILD_STATION);
    if (p->ainew.temp == 0) {
    	if (!IsTileType(p->ainew.from_tile, MP_STATION))
    		res = AiNew_Build_Station(p, p->ainew.tbt, p->ainew.from_tile, 0, 0, p->ainew.from_direction, DC_EXEC);
   	}
    else {
       	if (!IsTileType(p->ainew.to_tile, MP_STATION))
       	   	res = AiNew_Build_Station(p, p->ainew.tbt, p->ainew.to_tile, 0, 0, p->ainew.to_direction, DC_EXEC);
    	p->ainew.state = AI_STATE_BUILD_PATH;
    }
    if (res == CMD_ERROR) {
		DEBUG(ai,0)("[AiNew - BuildStation] Strange but true... station can not be build!");
		p->ainew.state = AI_STATE_NOTHING;
		// If the first station _was_ build, destroy it
		if (p->ainew.temp != 0)
			DoCommandByTile(p->ainew.from_tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		return;
    }
    p->ainew.temp++;
}

// Build the path
static void AiNew_State_BuildPath(Player *p) {
	assert(p->ainew.state == AI_STATE_BUILD_PATH);
	// p->ainew.temp is set to -1 when this function is called for the first time
	if (p->ainew.temp == -1) {
		DEBUG(ai,1)("[AiNew] Starting to build the path..");
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
			static const byte _roadbits_by_dir[4] = {2,1,8,4};
        	// If they not queue, they have to go up and down to try again at a station...
        	// We don't want that, so try building some road left or right of the station
        	int dir1, dir2, dir3;
        	TileIndex tile;
        	int i, r;
        	for (i=0;i<2;i++) {
            	if (i == 0) {
            		tile = p->ainew.from_tile + TileOffsByDir(p->ainew.from_direction);
            		dir1 = p->ainew.from_direction - 1;
            		if (dir1 < 0) dir1 = 3;
            		dir2 = p->ainew.from_direction + 1;
            		if (dir2 > 3) dir2 = 0;
            		dir3 = p->ainew.from_direction;
            	} else {
            		tile = p->ainew.to_tile + TileOffsByDir(p->ainew.to_direction);
            		dir1 = p->ainew.to_direction - 1;
            		if (dir1 < 0) dir1 = 3;
            		dir2 = p->ainew.to_direction + 1;
            		if (dir2 > 3) dir2 = 0;
            		dir3 = p->ainew.to_direction;
            	}

            	r = DoCommandByTile(tile, _roadbits_by_dir[dir1], 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
				if (r != CMD_ERROR) {
					dir1 = TileOffsByDir(dir1);
					if (IsTileType(tile + dir1, MP_CLEAR) || IsTileType(tile + dir1, MP_TREES)) {
						r = DoCommandByTile(tile+dir1, AiNew_GetRoadDirection(tile, tile+dir1, tile+dir1+dir1), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						if (r != CMD_ERROR) {
							if (IsTileType(tile + dir1 + dir1, MP_CLEAR) || IsTileType(tile + dir1 + dir1, MP_TREES))
								DoCommandByTile(tile+dir1+dir1, AiNew_GetRoadDirection(tile+dir1, tile+dir1+dir1, tile+dir1+dir1+dir1), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						}
					}
				}

				r = DoCommandByTile(tile, _roadbits_by_dir[dir2], 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
				if (r != CMD_ERROR) {
					dir2 = TileOffsByDir(dir2);
					if (IsTileType(tile + dir2, MP_CLEAR) || IsTileType(tile + dir2, MP_TREES)) {
						r = DoCommandByTile(tile+dir2, AiNew_GetRoadDirection(tile, tile+dir2, tile+dir2+dir2), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						if (r != CMD_ERROR) {
							if (IsTileType(tile + dir2 + dir2, MP_CLEAR) || IsTileType(tile + dir2 + dir2, MP_TREES))
								DoCommandByTile(tile+dir2+dir2, AiNew_GetRoadDirection(tile+dir2, tile+dir2+dir2, tile+dir2+dir2+dir2), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						}
					}
				}

				r = DoCommandByTile(tile, _roadbits_by_dir[dir3^2], 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
				if (r != CMD_ERROR) {
					dir3 = TileOffsByDir(dir3);
					if (IsTileType(tile + dir3, MP_CLEAR) || IsTileType(tile + dir3, MP_TREES)) {
						r = DoCommandByTile(tile+dir3, AiNew_GetRoadDirection(tile, tile+dir3, tile+dir3+dir3), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						if (r != CMD_ERROR) {
							if (IsTileType(tile + dir3 + dir3, MP_CLEAR) || IsTileType(tile + dir3 + dir3, MP_TREES))
								DoCommandByTile(tile+dir3+dir3, AiNew_GetRoadDirection(tile+dir3, tile+dir3+dir3, tile+dir3+dir3+dir3), 0, DC_EXEC | DC_NO_WATER, CMD_BUILD_ROAD);
						}
					}
				}
            }
        }


		DEBUG(ai,1)("[AiNew] Done building the path (cost: %d)", p->ainew.new_cost);
		p->ainew.state = AI_STATE_BUILD_DEPOT;
	}
}

// Builds the depot
static void AiNew_State_BuildDepot(Player *p) {
	int res = 0;
	assert(p->ainew.state == AI_STATE_BUILD_DEPOT);

	if (IsTileType(p->ainew.depot_tile, MP_STREET) && _map5[p->ainew.depot_tile] & 0x20) {
		if (_map_owner[p->ainew.depot_tile] == _current_player) {
			// The depot is already builded!
			p->ainew.state = AI_STATE_BUILD_VEHICLE;
			return;
		} else {
			// There is a depot, but not of our team! :(
			p->ainew.state = AI_STATE_NOTHING;
			return;
		}
	}

	// There is a bus on the tile we want to build road on... idle till he is gone! (BAD PERSON! :p)
	if (!EnsureNoVehicle(p->ainew.depot_tile + TileOffsByDir(p->ainew.depot_direction)))
		return;

	res = AiNew_Build_Depot(p, p->ainew.depot_tile, p->ainew.depot_direction, DC_EXEC);
    if (res == CMD_ERROR) {
		DEBUG(ai,0)("[AiNew - BuildDepot] Strange but true... depot can not be build!");
		p->ainew.state = AI_STATE_NOTHING;
		return;
    }

	p->ainew.state = AI_STATE_BUILD_VEHICLE;
	p->ainew.idle = 1;
	p->ainew.veh_main_id = (VehicleID)-1;
}

// Build vehicles
static void AiNew_State_BuildVehicle(Player *p) {
	int res;
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
    if (res == CMD_ERROR) {
    	// This happens when the AI can't build any more vehicles!
    	p->ainew.state = AI_STATE_NOTHING;
    	return;
    }
    // Increase the current counter
    p->ainew.cur_veh++;
    // Decrease the total counter
    p->ainew.amount_veh--;
    // Get the new ID
    if (p->ainew.tbt == AI_TRAIN) {
    } else {
        p->ainew.veh_id = _new_roadveh_id;
    }
    // Go give some orders!
   	p->ainew.state = AI_STATE_GIVE_ORDERS;
}

// Put the stations in the order list
static void AiNew_State_GiveOrders(Player *p) {
		int idx;
		Order order;

    assert(p->ainew.state == AI_STATE_GIVE_ORDERS);

    if (p->ainew.veh_main_id != (VehicleID)-1) {
        DoCommandByTile(0, p->ainew.veh_id + (p->ainew.veh_main_id << 16), 0, DC_EXEC, CMD_CLONE_ORDER);

        // Skip the first order if it is a second vehicle
        //  This to make vehicles go different ways..
        if (p->ainew.veh_id & 1)
            DoCommandByTile(0, p->ainew.veh_id, 0, DC_EXEC, CMD_SKIP_ORDER);
        p->ainew.state = AI_STATE_START_VEHICLE;
        return;
    } else {
        p->ainew.veh_main_id = p->ainew.veh_id;
    }

    // When more than 1 vehicle, we send them to different directions
		idx = 0;
		order.type = OT_GOTO_STATION;
		order.flags = 0;
		order.station = _map2[p->ainew.from_tile];
    if (p->ainew.tbt == AI_TRUCK && p->ainew.from_deliver)
			order.flags |= OF_FULL_LOAD;
		DoCommandByTile(0, p->ainew.veh_id + (idx << 16), PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);

		idx = 1;
		order.type = OT_GOTO_STATION;
		order.flags = 0;
		order.station = _map2[p->ainew.to_tile];
    if (p->ainew.tbt == AI_TRUCK && p->ainew.to_deliver)
			order.flags |= OF_FULL_LOAD;
		DoCommandByTile(0, p->ainew.veh_id + (idx << 16), PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);

	// Very handy for AI, goto depot.. but yeah, it needs to be activated ;)
    if (_patches.gotodepot) {
			idx = 2;
			order.type = OT_GOTO_DEPOT;
			order.flags = OF_UNLOAD;
			order.station = GetDepotByTile(p->ainew.depot_tile);
			DoCommandByTile(0, p->ainew.veh_id + (idx << 16), PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);
	}

    // Start the engines!
	p->ainew.state = AI_STATE_START_VEHICLE;
}

// Start the vehicle
static void AiNew_State_StartVehicle(Player *p) {
	assert(p->ainew.state == AI_STATE_START_VEHICLE);

	// 3, 2, 1... go! (give START_STOP command ;))
	DoCommandByTile(0, p->ainew.veh_id, 0, DC_EXEC, CMD_START_STOP_ROADVEH);
	// Try to build an other vehicle (that function will stop building when needed)
	p->ainew.state = AI_STATE_BUILD_VEHICLE;
}

// Repays money
static void AiNew_State_RepayMoney(Player *p) {
    int i;
    for (i=0;i<AI_LOAN_REPAY;i++)
    	DoCommandByTile(0, _current_player, 0, DC_EXEC, CMD_DECREASE_LOAN);
    p->ainew.state = AI_STATE_ACTION_DONE;
}

static void AiNew_CheckVehicle(Player *p, Vehicle *v) {
	// When a vehicle is under the 6 months, we don't check for anything
	if (v->age < 180) return;

	// When a vehicle is older then 1 year, it should make money...
	if (v->age > 360) {
		// If both years together are not more than AI_MINIMUM_ROUTE_PROFIT,
		//  it is not worth the line I guess...
		if (v->profit_last_year + v->profit_this_year < AI_MINIMUM_ROUTE_PROFIT ||
			(v->reliability * 100 >> 16) < 40) {
			// There is a possibility that the route is fucked up...
			if (v->cargo_days > AI_VEHICLE_LOST_DAYS) {
				// The vehicle is lost.. check the route, or else, get the vehicle
				//  back to a depot
				// TODO: make this piece of code
			}


			// We are already sending him back
			if (AiNew_GetSpecialVehicleFlag(p, v) & AI_VEHICLEFLAG_SELL) {
				if (v->type == VEH_Road && IsRoadDepotTile(v->tile) &&
					(v->vehstatus&VS_STOPPED)) {
					// We are at the depot, sell the vehicle
					DoCommandByTile(0, v->index, 0, DC_EXEC, CMD_SELL_ROAD_VEH);
				}
				return;
			}

			if (!AiNew_SetSpecialVehicleFlag(p, v, AI_VEHICLEFLAG_SELL)) return;
			{
				int res = 0;
				if (v->type == VEH_Road)
					res = DoCommandByTile(0, v->index, 0, DC_EXEC, CMD_SEND_ROADVEH_TO_DEPOT);
				// This means we can not find a depot :s
//				if (res == CMD_ERROR)
			}
		}
	}
}

// Checks all vehicles if they are still valid and make money and stuff
static void AiNew_State_CheckAllVehicles(Player *p) {
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == 0) continue;
		if (v->owner != p->index) continue;
		// Currently, we only know how to handle road-vehicles
		if (v->type != VEH_Road) continue;

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
    AiNew_State_GiveOrders,
    AiNew_State_StartVehicle,
    AiNew_State_RepayMoney,
	AiNew_State_CheckAllVehicles,
    AiNew_State_ActionDone,
    NULL,
};

static void AiNew_OnTick(Player *p) {
    if (_ainew_state[p->ainew.state] != NULL)
	    _ainew_state[p->ainew.state](p);
}

void AiNewDoGameLoop(Player *p) {
    // If it is a human player, it is not an AI, so bubye!
	if (IS_HUMAN_PLAYER(_current_player))
		return;

	if (p->ainew.state == AI_STATE_STARTUP) {
		// The AI just got alive!
		p->ainew.state = AI_STATE_FIRST_TIME;
		p->ainew.tick = 0;

		// Only startup the AI
		return;
	}

	// We keep a ticker. We use it for competitor_speed
	p->ainew.tick++;

	// See what the speed is
	switch (_opt.diff.competitor_speed) {
		case 0: // Very slow
			if (!(p->ainew.tick&8)) return;
			break;
		case 1: // Slow
			if (!(p->ainew.tick&4)) return;
			break;
		case 2:
			if (!(p->ainew.tick&2)) return;
			break;
		case 3:
			if (!(p->ainew.tick&1)) return;
			break;
		case 4: // Very fast
		default: // Cool, a new speed setting.. ;) VERY fast ;)
			break;
	}

	// If we come here, we can do a tick.. do so!
	AiNew_OnTick(p);
}
