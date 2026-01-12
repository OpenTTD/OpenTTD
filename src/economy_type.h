/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file economy_type.h Types related to the economy. */

#ifndef ECONOMY_TYPE_H
#define ECONOMY_TYPE_H

#include "core/overflowsafe_type.hpp"
#include "core/enum_type.hpp"
#include "core/pool_type.hpp"

typedef OverflowSafeInt64 Money;

/** Type of the game economy. */
enum EconomyType : uint8_t {
	ET_BEGIN = 0, ///< The lowest valid value.
	ET_ORIGINAL = 0, ///< Imitates original TTD economy.
	ET_SMOOTH = 1, ///< Makes production changes more often, and in smaller steps.
	ET_FROZEN = 2, ///< Stops production changes and industry closures.
	ET_END = 3, ///< Economy type end marker.
};

/**
 * Minimum allowed value of town_cargo_scale/industry_cargo_scale.
 * Below 13, callback-based industries would produce less than once per month. We round up to 15% because it's a nicer number.
 * Towns use the same minimum to match, and because below this small towns often produce no cargo.
 */
static const int MIN_CARGO_SCALE = 15;
/**
 * Maximum allowed value of town_cargo_scale/industry_cargo_scale.
 * Above 340, callback-based industries would produce more than once per day, which GRFs do not expect.
 * Towns use the same maximum to match.
 */
static const int MAX_CARGO_SCALE = 300;
/** Default value of town_cargo_scale/industry_cargo_scale. */
static const int DEF_CARGO_SCALE = 100;

/** Data of the economy. */
struct Economy {
	Money max_loan;                       ///< NOSAVE: Maximum possible loan
	int16_t fluct;                          ///< Economy fluctuation status
	uint8_t interest_rate;                   ///< Interest
	uint8_t infl_amount;                     ///< inflation amount
	uint8_t infl_amount_pr;                  ///< inflation rate for payment rates
	uint32_t industry_daily_change_counter; ///< Bits 31-16 are number of industry to be performed, 15-0 are fractional collected daily
	uint32_t industry_daily_increment;      ///< The value which will increment industry_daily_change_counter. Computed value. NOSAVE
	uint64_t inflation_prices;              ///< Cumulated inflation of prices since game start; 16 bit fractional part
	uint64_t inflation_payment;             ///< Cumulated inflation of cargo payment since game start; 16 bit fractional part

	/* Old stuff for savegame conversion only */
	Money old_max_loan_unround;           ///< Old: Unrounded max loan
	uint16_t old_max_loan_unround_fract;    ///< Old: Fraction of the unrounded max loan
};

/** Score categories in the detailed performance rating. */
enum class ScoreID : uint8_t {
	Begin, ///< The lowest valid value.
	Vehicles = ScoreID::Begin, ///< Number of vehicles that turned profit last year.
	Stations, ///< Number of recently-serviced stations.
	MinProfit, ///< The profit of the vehicle with the lowest income.
	MinIncome, ///< Income in the quater with the lowest profit of the last 12 quaters.
	MaxIncome, ///< Income in the quater with the highest profit of the last 12 quaters.
	Delivered, ///< Units of cargo delivered in the last four quaters.
	Cargo, ///< Number of types of cargo delivered in the last four quaters.
	Money, ///< Amount of money company has in the bank.
	Loan, ///< The amount of money company can take as a loan.
	Total, ///< Total points out of possible points ,must always be the last entry.
	End, ///< Score ID end marker.
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(ScoreID)

/**
 * The max score that can be in the performance history.
 * The scores together of score_info is allowed to be more!
 */
static constexpr int SCORE_MAX = 1000;

/** Data structure for storing how the score is computed for a single score id. */
struct ScoreInfo {
	int score;  ///< How much score it will give
	int needed; ///< How much you need to get the perfect score
};

/**
 * Enumeration of all base prices for use with #Prices.
 * The prices are ordered as they are expected by NewGRF cost multipliers, so don't shuffle them.
 */
enum Price : uint8_t {
	PR_BEGIN = 0, ///< The lowest valid value.
	PR_STATION_VALUE = 0, ///< Stations value and additional constant company running fee.
	PR_BUILD_RAIL, ///< Price for building rails.
	PR_BUILD_ROAD, ///< Price for building roads.
	PR_BUILD_SIGNALS, ///< Price for building rail signals.
	PR_BUILD_BRIDGE, ///< Price for building bridges.
	PR_BUILD_DEPOT_TRAIN, ///< Price for building train depots.
	PR_BUILD_DEPOT_ROAD, ///< Price for building road vehicle depots.
	PR_BUILD_DEPOT_SHIP, ///< Price for building ship depots.
	PR_BUILD_TUNNEL, ///< Price for building tunnels.
	PR_BUILD_STATION_RAIL, ///< Price for building rail stations.
	PR_BUILD_STATION_RAIL_LENGTH, ///< Additional price for building rail stations dependent on their length.
	PR_BUILD_STATION_AIRPORT, ///< Price for building airports.
	PR_BUILD_STATION_BUS, ///< Price for building bus stops.
	PR_BUILD_STATION_TRUCK, ///< Price for building lorry stations.
	PR_BUILD_STATION_DOCK, ///< Price for building docks.
	PR_BUILD_VEHICLE_TRAIN, ///< Price for purchasing new train engines.
	PR_BUILD_VEHICLE_WAGON, ///< Price for purchasing new wagons.
	PR_BUILD_VEHICLE_AIRCRAFT, ///< Price for purchasing new aircrafts.
	PR_BUILD_VEHICLE_ROAD, ///< Price for purchasing new road vehicles.
	PR_BUILD_VEHICLE_SHIP, ///< Price for purchasing new ships.
	PR_BUILD_TREES, ///< Price for planting trees.
	PR_TERRAFORM, ///< Price for terraforming land, e.g. rising, lowering and flattening.
	PR_CLEAR_GRASS, ///< Price for destroying grass.
	PR_CLEAR_ROUGH, ///< Price for destroying rough land.
	PR_CLEAR_ROCKS, ///< Price for destroying rocks.
	PR_CLEAR_FIELDS, ///< Price for destroying fields.
	PR_CLEAR_TREES, ///< Price for destroying trees.
	PR_CLEAR_RAIL, ///< Price for destroying rails.
	PR_CLEAR_SIGNALS, ///< Price for destroying rail signals.
	PR_CLEAR_BRIDGE, ///< Price for destroying bridges.
	PR_CLEAR_DEPOT_TRAIN, ///< Price for destroying train depots.
	PR_CLEAR_DEPOT_ROAD, ///< Price for destroying road vehicle depots.
	PR_CLEAR_DEPOT_SHIP, ///< Price for destroying ship depots.
	PR_CLEAR_TUNNEL, ///< Price for destroying tunnels.
	PR_CLEAR_WATER, ///< Price for destroying water e.g. see, rives.
	PR_CLEAR_STATION_RAIL, ///< Price for destroying rail stations.
	PR_CLEAR_STATION_AIRPORT, ///< Price for destroying airports.
	PR_CLEAR_STATION_BUS, ///< Price for destroying bus stops.
	PR_CLEAR_STATION_TRUCK, ///< Price for destroying lorry stations.
	PR_CLEAR_STATION_DOCK, ///< Price for destroying docks.
	PR_CLEAR_HOUSE, ///< Price for destroying houses and other town buildings.
	PR_CLEAR_ROAD, ///< Price for destroying roads.
	PR_RUNNING_TRAIN_STEAM, ///< Running cost of steam trains.
	PR_RUNNING_TRAIN_DIESEL, ///< Running cost of diesel trains.
	PR_RUNNING_TRAIN_ELECTRIC, ///< Running cost of electric trains.
	PR_RUNNING_AIRCRAFT, ///< Running cost of aircrafts.
	PR_RUNNING_ROADVEH, ///< Running cost of road vehicles.
	PR_RUNNING_SHIP, ///< Running cost of ships.
	PR_BUILD_INDUSTRY, ///< Price for funding new industries.
	PR_CLEAR_INDUSTRY, ///< Price for destroying industries.
	PR_BUILD_OBJECT, ///< Price for building new objects.
	PR_CLEAR_OBJECT, ///< Price for destroying objects.
	PR_BUILD_WAYPOINT_RAIL, ///< Price for building new rail waypoints.
	PR_CLEAR_WAYPOINT_RAIL, ///< Price for destroying rail waypoints.
	PR_BUILD_WAYPOINT_BUOY, ///< Price for building new buoys.
	PR_CLEAR_WAYPOINT_BUOY, ///< Price for destroying buoys.
	PR_TOWN_ACTION, ///< Price for interaction with local authorities.
	PR_BUILD_FOUNDATION, ///< Price for building foundation under other constructions e.g. roads, rails, depots, objects, etc., etc..
	PR_BUILD_INDUSTRY_RAW, ///< Price for funding new raw industries, e.g. coal mine, forest.
	PR_BUILD_TOWN, ///< Price for funding new towns and cities.
	PR_BUILD_CANAL, ///< Price for building new canals.
	PR_CLEAR_CANAL, ///< Price for destroying canals.
	PR_BUILD_AQUEDUCT, ///< Price for building new aqueducts.
	PR_CLEAR_AQUEDUCT, ///< Price for destroying aqueducts.
	PR_BUILD_LOCK, ///< Price for building new locks.
	PR_CLEAR_LOCK, ///< Price for destroying locks.
	PR_INFRASTRUCTURE_RAIL, ///< Rails maintenance cost.
	PR_INFRASTRUCTURE_ROAD, ///< Roads maintenance cost.
	PR_INFRASTRUCTURE_WATER, ///< Canals maintenance cost.
	PR_INFRASTRUCTURE_STATION, ///< Stations maintenance cost.
	PR_INFRASTRUCTURE_AIRPORT, ///< Airports maintenance cost.
	PR_END, ///< Price base end marker.
	INVALID_PRICE = 0xFF ///< Invalid base price.
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(Price)

typedef Money Prices[PR_END]; ///< Prices of everything. @see Price
using PriceMultipliers = std::array<int8_t, PR_END>;

/** Types of expenses. */
enum ExpensesType : uint8_t {
	EXPENSES_CONSTRUCTION =  0,   ///< Construction costs.
	EXPENSES_NEW_VEHICLES,        ///< New vehicles.
	EXPENSES_TRAIN_RUN,           ///< Running costs trains.
	EXPENSES_ROADVEH_RUN,         ///< Running costs road vehicles.
	EXPENSES_AIRCRAFT_RUN,        ///< Running costs aircraft.
	EXPENSES_SHIP_RUN,            ///< Running costs ships.
	EXPENSES_PROPERTY,            ///< Property costs.
	EXPENSES_TRAIN_REVENUE,       ///< Revenue from trains.
	EXPENSES_ROADVEH_REVENUE,     ///< Revenue from road vehicles.
	EXPENSES_AIRCRAFT_REVENUE,    ///< Revenue from aircraft.
	EXPENSES_SHIP_REVENUE,        ///< Revenue from ships.
	EXPENSES_LOAN_INTEREST,       ///< Interest payments over the loan.
	EXPENSES_OTHER,               ///< Other expenses.
	EXPENSES_END,                 ///< Number of expense types.
	INVALID_EXPENSES      = 0xFF, ///< Invalid expense type.
};

/**
 * Data type for storage of Money for each #ExpensesType category.
 */
using Expenses = std::array<Money, EXPENSES_END>;

/**
 * Categories of a price bases.
 */
enum PriceCategory : uint8_t {
	PCAT_NONE,         ///< Not affected by difficulty settings
	PCAT_RUNNING,      ///< Price is affected by "vehicle running cost" difficulty setting
	PCAT_CONSTRUCTION, ///< Price is affected by "construction cost" difficulty setting
};

/** The "steps" in loan size, in British Pounds! */
static const int LOAN_INTERVAL = 10000;
/** The size of loan for a new company, in British Pounds! */
static const int64_t INITIAL_LOAN = 100000;
/** The max amount possible to configure for a max loan of a company. */
static const int64_t MAX_LOAN_LIMIT = 2000000000;

/**
 * Maximum inflation (including fractional part) without causing overflows in int64_t price computations.
 * This allows for 32 bit base prices (21 are currently needed).
 * Considering the sign bit and 16 fractional bits, there are 15 bits left.
 * 170 years of 4% inflation result in a inflation of about 822, so 10 bits are actually enough.
 * Note that NewGRF multipliers share the 16 fractional bits.
 * @see MAX_PRICE_MODIFIER
 */
static const uint64_t MAX_INFLATION = (1ull << (63 - 32)) - 1;

/**
 * Maximum NewGRF price modifiers.
 * Increasing base prices by factor 65536 should be enough.
 * @see MAX_INFLATION
 */
static const int MIN_PRICE_MODIFIER = -8;
static const int MAX_PRICE_MODIFIER = 16;
static const int INVALID_PRICE_MODIFIER = MIN_PRICE_MODIFIER - 1;

/** Multiplier for how many regular track bits a tunnel/bridge counts. */
static const uint TUNNELBRIDGE_TRACKBIT_FACTOR = 4;
/** Multiplier for how many regular track bits a level crossing counts. */
static const uint LEVELCROSSING_TRACKBIT_FACTOR = 2;
/** Multiplier for how many regular track bits a road depot counts. */
static const uint ROAD_DEPOT_TRACKBIT_FACTOR = 2;
/** Multiplier for how many regular track bits a bay stop counts. */
static const uint ROAD_STOP_TRACKBIT_FACTOR = 2;
/** Multiplier for how many regular tiles a lock counts. */
static const uint LOCK_DEPOT_TILE_FACTOR = 2;

struct CargoPayment;
using CargoPaymentID = PoolID<uint32_t, struct CargoPaymentIDTag, 0xFF000, 0xFFFFF>;

#endif /* ECONOMY_TYPE_H */
