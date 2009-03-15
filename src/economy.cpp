/* $Id$ */

/** @file economy.cpp Handling of the economy. */

#include "stdafx.h"
#include "openttd.h"
#include "tile_cmd.h"
#include "company_func.h"
#include "command_func.h"
#include "industry_map.h"
#include "town.h"
#include "news_func.h"
#include "network/network.h"
#include "network/network_func.h"
#include "vehicle_gui.h"
#include "ai/ai.hpp"
#include "aircraft.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "newgrf_industries.h"
#include "newgrf_industrytiles.h"
#include "newgrf_station.h"
#include "unmovable.h"
#include "group.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "gfx_func.h"
#include "autoreplace_func.h"
#include "company_gui.h"
#include "signs_base.h"

#include "table/strings.h"
#include "table/sprites.h"

/**
 * Multiply two integer values and shift the results to right.
 *
 * This function multiplies two integer values. The result is
 * shifted by the amount of shift to right.
 *
 * @param a The first integer
 * @param b The second integer
 * @param shift The amount to shift the value to right.
 * @return The shifted result
 */
static inline int32 BigMulS(const int32 a, const int32 b, const uint8 shift)
{
	return (int32)((int64)a * (int64)b >> shift);
}

/**
 * Multiply two unsigned integers and shift the results to right.
 *
 * This function multiplies two unsigned integers. The result is
 * shifted by the amount of shift to right.
 *
 * @param a The first unsigned integer
 * @param b The second unsigned integer
 * @param shift The amount to shift the value to right.
 * @return The shifted result
 */
static inline uint32 BigMulSU(const uint32 a, const uint32 b, const uint8 shift)
{
	return (uint32)((uint64)a * (uint64)b >> shift);
}

typedef SmallVector<Industry *, 16> SmallIndustryList;

/* Score info */
const ScoreInfo _score_info[] = {
	{ SCORE_VEHICLES,        120, 100 },
	{ SCORE_STATIONS,         80, 100 },
	{ SCORE_MIN_PROFIT,    10000, 100 },
	{ SCORE_MIN_INCOME,    50000,  50 },
	{ SCORE_MAX_INCOME,   100000, 100 },
	{ SCORE_DELIVERED,     40000, 400 },
	{ SCORE_CARGO,             8,  50 },
	{ SCORE_MONEY,      10000000,  50 },
	{ SCORE_LOAN,         250000,  50 },
	{ SCORE_TOTAL,             0,   0 }
};

int _score_part[MAX_COMPANIES][SCORE_END];
Economy _economy;
Subsidy _subsidies[MAX_COMPANIES];
Prices _price;
uint16 _price_frac[NUM_PRICES];
Money  _cargo_payment_rates[NUM_CARGO];
uint16 _cargo_payment_rates_frac[NUM_CARGO];
Money _additional_cash_required;

Money CalculateCompanyValue(const Company *c)
{
	Owner owner = c->index;
	Money value = 0;

	Station *st;
	uint num = 0;

	FOR_ALL_STATIONS(st) {
		if (st->owner == owner) num += CountBits(st->facilities);
	}

	value += num * _price.station_value * 25;

	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->owner != owner) continue;

		if (v->type == VEH_TRAIN ||
				v->type == VEH_ROAD ||
				(v->type == VEH_AIRCRAFT && IsNormalAircraft(v)) ||
				v->type == VEH_SHIP) {
			value += v->value * 3 >> 1;
		}
	}

	/* Add real money value */
	value -= c->current_loan;
	value += c->money;

	return max(value, (Money)1);
}

/** if update is set to true, the economy is updated with this score
 *  (also the house is updated, should only be true in the on-tick event)
 * @param update the economy with calculated score
 * @param c company been evaluated
 * @return actual score of this company
 * */
int UpdateCompanyRatingAndValue(Company *c, bool update)
{
	Owner owner = c->index;
	int score = 0;

	memset(_score_part[owner], 0, sizeof(_score_part[owner]));

	/* Count vehicles */
	{
		Vehicle *v;
		Money min_profit = 0;
		bool min_profit_first = true;
		uint num = 0;

		FOR_ALL_VEHICLES(v) {
			if (v->owner != owner) continue;
			if (IsCompanyBuildableVehicleType(v->type) && v->IsPrimaryVehicle()) {
				num++;
				if (v->age > 730) {
					/* Find the vehicle with the lowest amount of profit */
					if (min_profit_first || min_profit > v->profit_last_year) {
						min_profit = v->profit_last_year;
						min_profit_first = false;
					}
				}
			}
		}

		min_profit >>= 8; // remove the fract part

		_score_part[owner][SCORE_VEHICLES] = num;
		/* Don't allow negative min_profit to show */
		if (min_profit > 0)
			_score_part[owner][SCORE_MIN_PROFIT] = ClampToI32(min_profit);
	}

	/* Count stations */
	{
		uint num = 0;
		const Station *st;

		FOR_ALL_STATIONS(st) {
			if (st->owner == owner) num += CountBits(st->facilities);
		}
		_score_part[owner][SCORE_STATIONS] = num;
	}

	/* Generate statistics depending on recent income statistics */
	{
		int numec = min(c->num_valid_stat_ent, 12);
		if (numec != 0) {
			const CompanyEconomyEntry *cee = c->old_economy;
			Money min_income = cee->income + cee->expenses;
			Money max_income = cee->income + cee->expenses;

			do {
				min_income = min(min_income, cee->income + cee->expenses);
				max_income = max(max_income, cee->income + cee->expenses);
			} while (++cee, --numec);

			if (min_income > 0) {
				_score_part[owner][SCORE_MIN_INCOME] = ClampToI32(min_income);
			}

			_score_part[owner][SCORE_MAX_INCOME] = ClampToI32(max_income);
		}
	}

	/* Generate score depending on amount of transported cargo */
	{
		const CompanyEconomyEntry *cee;
		int numec;
		uint32 total_delivered;

		numec = min(c->num_valid_stat_ent, 4);
		if (numec != 0) {
			cee = c->old_economy;
			total_delivered = 0;
			do {
				total_delivered += cee->delivered_cargo;
			} while (++cee, --numec);

			_score_part[owner][SCORE_DELIVERED] = total_delivered;
		}
	}

	/* Generate score for variety of cargo */
	{
		uint num = CountBits(c->cargo_types);
		_score_part[owner][SCORE_CARGO] = num;
		if (update) c->cargo_types = 0;
	}

	/* Generate score for company's money */
	{
		if (c->money > 0) {
			_score_part[owner][SCORE_MONEY] = ClampToI32(c->money);
		}
	}

	/* Generate score for loan */
	{
		_score_part[owner][SCORE_LOAN] = ClampToI32(_score_info[SCORE_LOAN].needed - c->current_loan);
	}

	/* Now we calculate the score for each item.. */
	{
		int total_score = 0;
		int s;
		score = 0;
		for (ScoreID i = SCORE_BEGIN; i < SCORE_END; i++) {
			/* Skip the total */
			if (i == SCORE_TOTAL) continue;
			/*  Check the score */
			s = Clamp(_score_part[owner][i], 0, _score_info[i].needed) * _score_info[i].score / _score_info[i].needed;
			score += s;
			total_score += _score_info[i].score;
		}

		_score_part[owner][SCORE_TOTAL] = score;

		/*  We always want the score scaled to SCORE_MAX (1000) */
		if (total_score != SCORE_MAX) score = score * SCORE_MAX / total_score;
	}

	if (update) {
		c->old_economy[0].performance_history = score;
		UpdateCompanyHQ(c, score);
		c->old_economy[0].company_value = CalculateCompanyValue(c);
	}

	InvalidateWindow(WC_PERFORMANCE_DETAIL, 0);
	return score;
}

/*  use INVALID_OWNER as new_owner to delete the company. */
void ChangeOwnershipOfCompanyItems(Owner old_owner, Owner new_owner)
{
	Town *t;
	CompanyID old = _current_company;

	assert(old_owner != new_owner);

	{
		Company *c;
		uint i;

		/* See if the old_owner had shares in other companies */
		_current_company = old_owner;
		FOR_ALL_COMPANIES(c) {
			for (i = 0; i < 4; i++) {
				if (c->share_owners[i] == old_owner) {
					/* Sell his shares */
					CommandCost res = DoCommand(0, c->index, 0, DC_EXEC, CMD_SELL_SHARE_IN_COMPANY);
					/* Because we are in a DoCommand, we can't just execute an other one and
					 *  expect the money to be removed. We need to do it ourself! */
					SubtractMoneyFromCompany(res);
				}
			}
		}

		/* Sell all the shares that people have on this company */
		c = GetCompany(old_owner);
		for (i = 0; i < 4; i++) {
			_current_company = c->share_owners[i];
			if (_current_company != INVALID_OWNER) {
				/* Sell the shares */
				CommandCost res = DoCommand(0, old_owner, 0, DC_EXEC, CMD_SELL_SHARE_IN_COMPANY);
				/* Because we are in a DoCommand, we can't just execute an other one and
				 *  expect the money to be removed. We need to do it ourself! */
				SubtractMoneyFromCompany(res);
			}
		}
	}

	_current_company = old_owner;

	/* Temporarily increase the company's money, to be sure that
	 * removing his/her property doesn't fail because of lack of money.
	 * Not too drastically though, because it could overflow */
	if (new_owner == INVALID_OWNER) {
		GetCompany(old_owner)->money = UINT64_MAX >> 2; // jackpot ;p
	}

	if (new_owner == INVALID_OWNER) {
		Subsidy *s;

		for (s = _subsidies; s != endof(_subsidies); s++) {
			if (s->cargo_type != CT_INVALID && s->age >= 12) {
				if (GetStation(s->to)->owner == old_owner) s->cargo_type = CT_INVALID;
			}
		}
	}

	/* Take care of rating in towns */
	FOR_ALL_TOWNS(t) {
		/* If a company takes over, give the ratings to that company. */
		if (new_owner != INVALID_OWNER) {
			if (HasBit(t->have_ratings, old_owner)) {
				if (HasBit(t->have_ratings, new_owner)) {
					/* use max of the two ratings. */
					t->ratings[new_owner] = max(t->ratings[new_owner], t->ratings[old_owner]);
				} else {
					SetBit(t->have_ratings, new_owner);
					t->ratings[new_owner] = t->ratings[old_owner];
				}
			}
		}

		/* Reset the ratings for the old owner */
		t->ratings[old_owner] = RATING_INITIAL;
		ClrBit(t->have_ratings, old_owner);
	}

	{
		FreeUnitIDGenerator unitidgen[] = {
			FreeUnitIDGenerator(VEH_TRAIN, new_owner), FreeUnitIDGenerator(VEH_ROAD,     new_owner),
			FreeUnitIDGenerator(VEH_SHIP,  new_owner), FreeUnitIDGenerator(VEH_AIRCRAFT, new_owner)
		};

		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->owner == old_owner && IsCompanyBuildableVehicleType(v->type)) {
				if (new_owner == INVALID_OWNER) {
					if (v->Previous() == NULL) delete v;
				} else {
					v->owner = new_owner;
					v->colourmap = PAL_NONE;
					if (IsEngineCountable(v)) GetCompany(new_owner)->num_engines[v->engine_type]++;
					if (v->IsPrimaryVehicle()) v->unitnumber = unitidgen[v->type].NextID();
				}
			}
		}
	}

	/*  Change ownership of tiles */
	{
		TileIndex tile = 0;
		do {
			ChangeTileOwner(tile, old_owner, new_owner);
		} while (++tile != MapSize());

		if (new_owner != INVALID_OWNER) {
			/* Update all signals because there can be new segment that was owned by two companies
			 * and signals were not propagated
			 * Similiar with crossings - it is needed to bar crossings that weren't before
			 * because of different owner of crossing and approaching train */
			tile = 0;

			do {
				if (IsTileType(tile, MP_RAILWAY) && IsTileOwner(tile, new_owner) && HasSignals(tile)) {
					TrackBits tracks = GetTrackBits(tile);
					do { // there may be two tracks with signals for TRACK_BIT_HORZ and TRACK_BIT_VERT
						Track track = RemoveFirstTrack(&tracks);
						if (HasSignalOnTrack(tile, track)) AddTrackToSignalBuffer(tile, track, new_owner);
					} while (tracks != TRACK_BIT_NONE);
				} else if (IsLevelCrossingTile(tile) && IsTileOwner(tile, new_owner)) {
					UpdateLevelCrossing(tile);
				}
			} while (++tile != MapSize());
		}

		/* update signals in buffer */
		UpdateSignalsInBuffer();
	}

	/* convert owner of stations (including deleted ones, but excluding buoys) */
	Station *st;
	FOR_ALL_STATIONS(st) {
		if (st->owner == old_owner) {
			/* if a company goes bankrupt, set owner to OWNER_NONE so the sign doesn't disappear immediately
			 * also, drawing station window would cause reading invalid company's colour */
			st->owner = new_owner == INVALID_OWNER ? OWNER_NONE : new_owner;
		}
	}

	/* do the same for waypoints (we need to do this here so deleted waypoints are converted too) */
	Waypoint *wp;
	FOR_ALL_WAYPOINTS(wp) {
		if (wp->owner == old_owner) {
			wp->owner = new_owner == INVALID_OWNER ? OWNER_NONE : new_owner;
		}
	}

	/* In all cases clear replace engine rules.
	 * Even if it was copied, it could interfere with new owner's rules */
	RemoveAllEngineReplacementForCompany(GetCompany(old_owner));

	if (new_owner == INVALID_OWNER) {
		RemoveAllGroupsForCompany(old_owner);
	} else {
		Group *g;
		FOR_ALL_GROUPS(g) {
			if (g->owner == old_owner) g->owner = new_owner;
		}
	}

	Sign *si;
	FOR_ALL_SIGNS(si) {
		if (si->owner == old_owner) si->owner = new_owner == INVALID_OWNER ? OWNER_NONE : new_owner;
	}

	/* Change colour of existing windows */
	if (new_owner != INVALID_OWNER) ChangeWindowOwner(old_owner, new_owner);

	_current_company = old;

	MarkWholeScreenDirty();
}

static void ChangeNetworkOwner(Owner current_owner, Owner new_owner)
{
#ifdef ENABLE_NETWORK
	if (!_networking) return;

	if (current_owner == _local_company) {
		_network_playas = new_owner;
		SetLocalCompany(new_owner);
	}

	if (!_network_server) return;

	NetworkServerChangeOwner(current_owner, new_owner);
#endif /* ENABLE_NETWORK */
}

static void CompanyCheckBankrupt(Company *c)
{
	/*  If the company has money again, it does not go bankrupt */
	if (c->money >= 0) {
		c->quarters_of_bankrupcy = 0;
		return;
	}

	c->quarters_of_bankrupcy++;

	CompanyNewsInformation *cni = MallocT<CompanyNewsInformation>(1);
	cni->FillData(c);

	switch (c->quarters_of_bankrupcy) {
		case 0:
		case 1:
			free(cni);
			break;

		case 2:
			SetDParam(0, STR_7056_TRANSPORT_COMPANY_IN_TROUBLE);
			SetDParam(1, STR_7057_WILL_BE_SOLD_OFF_OR_DECLARED);
			SetDParamStr(2, cni->company_name);
			AddNewsItem(STR_02B6, NS_COMPANY_TROUBLE, 0, 0, cni);
			AI::BroadcastNewEvent(new AIEventCompanyInTrouble(c->index));
			break;
		case 3: {
			/* XXX - In multiplayer, should we ask other companies if it wants to take
		          over when it is a human company? -- TrueLight */
			if (IsHumanCompany(c->index)) {
				SetDParam(0, STR_7056_TRANSPORT_COMPANY_IN_TROUBLE);
				SetDParam(1, STR_7057_WILL_BE_SOLD_OFF_OR_DECLARED);
				SetDParamStr(2, cni->company_name);
				AddNewsItem(STR_02B6, NS_COMPANY_TROUBLE, 0, 0, cni);
				break;
			}

			/* Check if the company has any value.. if not, declare it bankrupt
			 *  right now */
			Money val = CalculateCompanyValue(c);
			if (val > 0) {
				c->bankrupt_value = val;
				c->bankrupt_asked = 1 << c->index; // Don't ask the owner
				c->bankrupt_timeout = 0;
				free(cni);
				break;
			}
			/* Else, falltrue to case 4... */
		}
		default:
		case 4:
			if (!_networking && _local_company == c->index) {
				/* If we are in offline mode, leave the company playing. Eg. there
				 * is no THE-END, otherwise mark the client as spectator to make sure
				 * he/she is no long in control of this company. However... when you
				 * join another company (cheat) the "unowned" company can bankrupt. */
				c->bankrupt_asked = MAX_UVALUE(CompanyMask);
				c->bankrupt_timeout = 0x456;
				break;
			}

			/* Close everything the owner has open */
			DeleteCompanyWindows(c->index);

			/* Show bankrupt news */
			SetDParam(0, STR_705C_BANKRUPT);
			SetDParam(1, STR_705D_HAS_BEEN_CLOSED_DOWN_BY);
			SetDParamStr(2, cni->company_name);
			AddNewsItem(STR_02B6, NS_COMPANY_BANKRUPT, 0, 0, cni);

			/* Remove the company */
			ChangeNetworkOwner(c->index, COMPANY_SPECTATOR);
			ChangeOwnershipOfCompanyItems(c->index, INVALID_OWNER);

			if (!IsHumanCompany(c->index)) AI::Stop(c->index);

			CompanyID c_index = c->index;
			delete c;
			AI::BroadcastNewEvent(new AIEventCompanyBankrupt(c_index));
	}
}

static void CompaniesGenStatistics()
{
	Station *st;
	Company *c;

	FOR_ALL_STATIONS(st) {
		_current_company = st->owner;
		CommandCost cost(EXPENSES_PROPERTY, _price.station_value >> 1);
		SubtractMoneyFromCompany(cost);
	}

	if (!HasBit(1 << 0 | 1 << 3 | 1 << 6 | 1 << 9, _cur_month))
		return;

	FOR_ALL_COMPANIES(c) {
		memmove(&c->old_economy[1], &c->old_economy[0], sizeof(c->old_economy) - sizeof(c->old_economy[0]));
		c->old_economy[0] = c->cur_economy;
		memset(&c->cur_economy, 0, sizeof(c->cur_economy));

		if (c->num_valid_stat_ent != 24) c->num_valid_stat_ent++;

		UpdateCompanyRatingAndValue(c, true);
		CompanyCheckBankrupt(c);

		if (c->block_preview != 0) c->block_preview--;
	}

	InvalidateWindow(WC_INCOME_GRAPH, 0);
	InvalidateWindow(WC_OPERATING_PROFIT, 0);
	InvalidateWindow(WC_DELIVERED_CARGO, 0);
	InvalidateWindow(WC_PERFORMANCE_HISTORY, 0);
	InvalidateWindow(WC_COMPANY_VALUE, 0);
	InvalidateWindow(WC_COMPANY_LEAGUE, 0);
}

static void AddSingleInflation(Money *value, uint16 *frac, int32 amt)
{
	/* Is it safe to add inflation ? */
	if ((INT64_MAX / amt) < (*value + 1)) {
		*value = INT64_MAX / amt;
		*frac = 0;
	} else {
		int64 tmp = (int64)*value * amt + *frac;
		*frac   = GB(tmp, 0, 16);
		*value += tmp >> 16;
	}
}

static void AddInflation(bool check_year = true)
{
	/* The cargo payment inflation differs from the normal inflation, so the
	 * relative amount of money you make with a transport decreases slowly over
	 * the 170 years. After a few hundred years we reach a level in which the
	 * games will become unplayable as the maximum income will be less than
	 * the minimum running cost.
	 *
	 * Furthermore there are a lot of inflation related overflows all over the
	 * place. Solving them is hardly possible because inflation will always
	 * reach the overflow threshold some day. So we'll just perform the
	 * inflation mechanism during the first 170 years (the amount of years that
	 * one had in the original TTD) and stop doing the inflation after that
	 * because it only causes problems that can't be solved nicely and the
	 * inflation doesn't add anything after that either; it even makes playing
	 * it impossible due to the diverging cost and income rates.
	 */
	if (check_year && (_cur_year - _settings_game.game_creation.starting_year) >= (ORIGINAL_MAX_YEAR - ORIGINAL_BASE_YEAR)) return;

	/* Approximation for (100 + infl_amount)% ** (1 / 12) - 100%
	 * scaled by 65536
	 * 12 -> months per year
	 * This is only a good approxiamtion for small values
	 */
	int32 inf = _economy.infl_amount * 54;

	for (uint i = 0; i != NUM_PRICES; i++) {
		AddSingleInflation((Money*)&_price + i, _price_frac + i, inf);
	}

	AddSingleInflation(&_economy.max_loan_unround, &_economy.max_loan_unround_fract, inf);

	if (_economy.max_loan + 50000 <= _economy.max_loan_unround) _economy.max_loan += 50000;

	inf = _economy.infl_amount_pr * 54;
	for (CargoID i = 0; i < NUM_CARGO; i++) {
		AddSingleInflation(
			(Money*)_cargo_payment_rates + i,
			_cargo_payment_rates_frac + i,
			inf
		);
	}

	InvalidateWindowClasses(WC_BUILD_VEHICLE);
	InvalidateWindowClasses(WC_REPLACE_VEHICLE);
	InvalidateWindowClasses(WC_VEHICLE_DETAILS);
	InvalidateWindow(WC_PAYMENT_RATES, 0);
}

static void CompaniesPayInterest()
{
	const Company *c;

	FOR_ALL_COMPANIES(c) {
		_current_company = c->index;

		/* Over a year the paid interest should be "loan * interest percentage",
		 * but... as that number is likely not dividable by 12 (pay each month),
		 * one needs to account for that in the monthly fee calculations.
		 * To easily calculate what one should pay "this" month, you calculate
		 * what (total) should have been paid up to this month and you substract
		 * whatever has been paid in the previous months. This will mean one month
		 * it'll be a bit more and the other it'll be a bit less than the average
		 * monthly fee, but on average it will be exact. */
		Money yearly_fee = c->current_loan * _economy.interest_rate / 100;
		Money up_to_previous_month = yearly_fee * _cur_month / 12;
		Money up_to_this_month = yearly_fee * (_cur_month + 1) / 12;

		SubtractMoneyFromCompany(CommandCost(EXPENSES_LOAN_INT, up_to_this_month - up_to_previous_month));

		SubtractMoneyFromCompany(CommandCost(EXPENSES_OTHER, _price.station_value >> 2));
	}
}

static void HandleEconomyFluctuations()
{
	if (_settings_game.difficulty.economy == 0) return;

	if (--_economy.fluct == 0) {
		_economy.fluct = -(int)GB(Random(), 0, 2);
		AddNewsItem(STR_7073_WORLD_RECESSION_FINANCIAL, NS_ECONOMY, 0, 0);
	} else if (_economy.fluct == -12) {
		_economy.fluct = GB(Random(), 0, 8) + 312;
		AddNewsItem(STR_7074_RECESSION_OVER_UPTURN_IN, NS_ECONOMY, 0, 0);
	}
}

static byte _price_category[NUM_PRICES] = {
	0, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 1, 1, 1, 1, 1, 1,
	2,
};

static const Money _price_base[NUM_PRICES] = {
	    100, ///< station_value
	    100, ///< build_rail
	     95, ///< build_road
	     65, ///< build_signals
	    275, ///< build_bridge
	    600, ///< build_train_depot
	    500, ///< build_road_depot
	    700, ///< build_ship_depot
	    450, ///< build_tunnel
	    200, ///< train_station_track
	    180, ///< train_station_length
	    600, ///< build_airport
	    200, ///< build_bus_station
	    200, ///< build_truck_station
	    350, ///< build_dock
	 400000, ///< build_railvehicle
	   2000, ///< build_railwagon
	 700000, ///< aircraft_base
	  14000, ///< roadveh_base
	  65000, ///< ship_base
	     20, ///< build_trees
	    250, ///< terraform
	     20, ///< clear_grass
	     40, ///< clear_roughland
	    200, ///< clear_rocks
	    500, ///< clear_fields
	     20, ///< remove_trees
	    -70, ///< remove_rail
	     10, ///< remove_signals
	     50, ///< clear_bridge
	     80, ///< remove_train_depot
	     80, ///< remove_road_depot
	     90, ///< remove_ship_depot
	     30, ///< clear_tunnel
	  10000, ///< clear_water
	     50, ///< remove_rail_station
	     30, ///< remove_airport
	     50, ///< remove_bus_station
	     50, ///< remove_truck_station
	     55, ///< remove_dock
	   1600, ///< remove_house
	     40, ///< remove_road
	   5600, ///< running_rail[0] steam
	   5200, ///< running_rail[1] diesel
	   4800, ///< running_rail[2] electric
	   9600, ///< aircraft_running
	   1600, ///< roadveh_running
	   5600, ///< ship_running
	1000000, ///< build_industry
};

static byte price_base_multiplier[NUM_PRICES];

/**
 * Reset changes to the price base multipliers.
 */
void ResetPriceBaseMultipliers()
{
	uint i;

	/* 8 means no multiplier. */
	for (i = 0; i < NUM_PRICES; i++)
		price_base_multiplier[i] = 8;
}

/**
 * Change a price base by the given factor.
 * The price base is altered by factors of two, with an offset of 8.
 * NewBaseCost = OldBaseCost * 2^(n-8)
 * @param price Index of price base to change.
 * @param factor Amount to change by.
 */
void SetPriceBaseMultiplier(uint price, byte factor)
{
	assert(price < NUM_PRICES);
	price_base_multiplier[price] = factor;
}

/**
 * Initialize the variables that will maintain the daily industry change system.
 * @param init_counter specifies if the counter is required to be initialized
 */
void StartupIndustryDailyChanges(bool init_counter)
{
	uint map_size = MapLogX() + MapLogY();
	/* After getting map size, it needs to be scaled appropriately and divided by 31,
	 * which stands for the days in a month.
	 * Using just 31 will make it so that a monthly reset (based on the real number of days of that month)
	 * would not be needed.
	 * Since it is based on "fractionnal parts", the leftover days will not make much of a difference
	 * on the overall total number of changes performed */
	_economy.industry_daily_increment = (1 << map_size) / 31;

	if (init_counter) {
		/* A new game or a savegame from an older version will require the counter to be initialized */
		_economy.industry_daily_change_counter = 0;
	}
}

void StartupEconomy()
{
	int i;

	assert(sizeof(_price) == NUM_PRICES * sizeof(Money));

	for (i = 0; i != NUM_PRICES; i++) {
		Money price = _price_base[i];
		if (_price_category[i] != 0) {
			uint mod = _price_category[i] == 1 ? _settings_game.difficulty.vehicle_costs : _settings_game.difficulty.construction_cost;
			if (mod < 1) {
				price = price * 3 >> 2;
			} else if (mod > 1) {
				price = price * 9 >> 3;
			}
		}
		if (price_base_multiplier[i] > 8) {
			price <<= price_base_multiplier[i] - 8;
		} else {
			price >>= 8 - price_base_multiplier[i];
		}
		((Money*)&_price)[i] = price;
		_price_frac[i] = 0;
	}

	_economy.interest_rate = _settings_game.difficulty.initial_interest;
	_economy.infl_amount = _settings_game.difficulty.initial_interest;
	_economy.infl_amount_pr = max(0, _settings_game.difficulty.initial_interest - 1);
	_economy.max_loan_unround = _economy.max_loan = _settings_game.difficulty.max_loan;
	_economy.fluct = GB(Random(), 0, 8) + 168;

	StartupIndustryDailyChanges(true); // As we are starting a new game, initialize the counter too

}

void ResetEconomy()
{
	/* Test if resetting the economy is needed. */
	bool needed = false;

	for (CargoID c = 0; c < NUM_CARGO; c++) {
		const CargoSpec *cs = GetCargo(c);
		if (!cs->IsValid()) continue;
		if (_cargo_payment_rates[c] == 0) {
			needed = true;
			break;
		}
	}

	if (!needed) return;

	/* Remember old unrounded maximum loan value. NewGRF has the ability
	 * to change all the other inflation affected base costs. */
	Money old_value = _economy.max_loan_unround;

	/* Reset the economy */
	StartupEconomy();
	InitializeLandscapeVariables(false);

	/* Reapply inflation, ignoring the year */
	while (old_value > _economy.max_loan_unround) {
		AddInflation(false);
	}
}

Money GetPriceByIndex(uint8 index)
{
	if (index > NUM_PRICES) return 0;

	return ((Money*)&_price)[index];
}


Pair SetupSubsidyDecodeParam(const Subsidy *s, bool mode)
{
	TileIndex tile;
	TileIndex tile2;
	Pair tp;

	/* if mode is false, use the singular form */
	const CargoSpec *cs = GetCargo(s->cargo_type);
	SetDParam(0, mode ? cs->name : cs->name_single);

	if (s->age < 12) {
		if (cs->town_effect != TE_PASSENGERS && cs->town_effect != TE_MAIL) {
			SetDParam(1, STR_INDUSTRY);
			SetDParam(2, s->from);
			tile = GetIndustry(s->from)->xy;

			if (cs->town_effect != TE_GOODS && cs->town_effect != TE_FOOD) {
				SetDParam(4, STR_INDUSTRY);
				SetDParam(5, s->to);
				tile2 = GetIndustry(s->to)->xy;
			} else {
				SetDParam(4, STR_TOWN);
				SetDParam(5, s->to);
				tile2 = GetTown(s->to)->xy;
			}
		} else {
			SetDParam(1, STR_TOWN);
			SetDParam(2, s->from);
			tile = GetTown(s->from)->xy;

			SetDParam(4, STR_TOWN);
			SetDParam(5, s->to);
			tile2 = GetTown(s->to)->xy;
		}
	} else {
		SetDParam(1, s->from);
		tile = GetStation(s->from)->xy;

		SetDParam(2, s->to);
		tile2 = GetStation(s->to)->xy;
	}

	tp.a = tile;
	tp.b = tile2;

	return tp;
}

void DeleteSubsidyWithTown(TownID index)
{
	Subsidy *s;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type != CT_INVALID && s->age < 12) {
			const CargoSpec *cs = GetCargo(s->cargo_type);
			if (((cs->town_effect == TE_PASSENGERS || cs->town_effect == TE_MAIL) && (index == s->from || index == s->to)) ||
				((cs->town_effect == TE_GOODS || cs->town_effect == TE_FOOD) && index == s->to)) {
				s->cargo_type = CT_INVALID;
			}
		}
	}
}

void DeleteSubsidyWithIndustry(IndustryID index)
{
	Subsidy *s;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type != CT_INVALID && s->age < 12) {
			const CargoSpec *cs = GetCargo(s->cargo_type);
			if (cs->town_effect != TE_PASSENGERS && cs->town_effect != TE_MAIL &&
				(index == s->from || (cs->town_effect != TE_GOODS && cs->town_effect != TE_FOOD && index == s->to))) {
				s->cargo_type = CT_INVALID;
			}
		}
	}
}

void DeleteSubsidyWithStation(StationID index)
{
	Subsidy *s;
	bool dirty = false;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type != CT_INVALID && s->age >= 12 &&
				(s->from == index || s->to == index)) {
			s->cargo_type = CT_INVALID;
			dirty = true;
		}
	}

	if (dirty)
		InvalidateWindow(WC_SUBSIDIES_LIST, 0);
}

struct FoundRoute {
	uint distance;
	CargoID cargo;
	void *from;
	void *to;
};

static void FindSubsidyPassengerRoute(FoundRoute *fr)
{
	Town *from, *to;

	fr->distance = UINT_MAX;

	fr->from = from = GetRandomTown();
	if (from == NULL || from->population < 400) return;

	fr->to = to = GetRandomTown();
	if (from == to || to == NULL || to->population < 400 || to->pct_pass_transported > 42)
		return;

	fr->distance = DistanceManhattan(from->xy, to->xy);
}

static void FindSubsidyCargoRoute(FoundRoute *fr)
{
	Industry *i;
	int trans, total;
	CargoID cargo;

	fr->distance = UINT_MAX;

	fr->from = i = GetRandomIndustry();
	if (i == NULL) return;

	/* Randomize cargo type */
	if (HasBit(Random(), 0) && i->produced_cargo[1] != CT_INVALID) {
		cargo = i->produced_cargo[1];
		trans = i->last_month_pct_transported[1];
		total = i->last_month_production[1];
	} else {
		cargo = i->produced_cargo[0];
		trans = i->last_month_pct_transported[0];
		total = i->last_month_production[0];
	}

	/* Quit if no production in this industry
	 * or if the cargo type is passengers
	 * or if the pct transported is already large enough */
	if (total == 0 || trans > 42 || cargo == CT_INVALID) return;

	const CargoSpec *cs = GetCargo(cargo);
	if (cs->town_effect == TE_PASSENGERS) return;

	fr->cargo = cargo;

	if (cs->town_effect == TE_GOODS || cs->town_effect == TE_FOOD) {
		/*  The destination is a town */
		Town *t = GetRandomTown();

		/* Only want big towns */
		if (t == NULL || t->population < 900) return;

		fr->distance = DistanceManhattan(i->xy, t->xy);
		fr->to = t;
	} else {
		/* The destination is an industry */
		Industry *i2 = GetRandomIndustry();

		/* The industry must accept the cargo */
		if (i2 == NULL || i == i2 ||
				(cargo != i2->accepts_cargo[0] &&
				cargo != i2->accepts_cargo[1] &&
				cargo != i2->accepts_cargo[2])) {
			return;
		}
		fr->distance = DistanceManhattan(i->xy, i2->xy);
		fr->to = i2;
	}
}

static bool CheckSubsidyDuplicate(Subsidy *s)
{
	const Subsidy *ss;

	for (ss = _subsidies; ss != endof(_subsidies); ss++) {
		if (s != ss &&
				ss->from == s->from &&
				ss->to == s->to &&
				ss->cargo_type == s->cargo_type) {
			s->cargo_type = CT_INVALID;
			return true;
		}
	}
	return false;
}


static void SubsidyMonthlyHandler()
{
	Subsidy *s;
	Pair pair;
	Station *st;
	uint n;
	FoundRoute fr;
	bool modified = false;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type == CT_INVALID) continue;

		if (s->age == 12 - 1) {
			pair = SetupSubsidyDecodeParam(s, 1);
			AddNewsItem(STR_202E_OFFER_OF_SUBSIDY_EXPIRED, NS_SUBSIDIES, pair.a, pair.b);
			s->cargo_type = CT_INVALID;
			modified = true;
			AI::BroadcastNewEvent(new AIEventSubsidyOfferExpired(s - _subsidies));
		} else if (s->age == 2 * 12 - 1) {
			st = GetStation(s->to);
			if (st->owner == _local_company) {
				pair = SetupSubsidyDecodeParam(s, 1);
				AddNewsItem(STR_202F_SUBSIDY_WITHDRAWN_SERVICE, NS_SUBSIDIES, pair.a, pair.b);
			}
			s->cargo_type = CT_INVALID;
			modified = true;
			AI::BroadcastNewEvent(new AIEventSubsidyExpired(s - _subsidies));
		} else {
			s->age++;
		}
	}

	/* 25% chance to go on */
	if (Chance16(1, 4)) {
		/*  Find a free slot*/
		s = _subsidies;
		while (s->cargo_type != CT_INVALID) {
			if (++s == endof(_subsidies))
				goto no_add;
		}

		n = 1000;
		do {
			FindSubsidyPassengerRoute(&fr);
			if (fr.distance <= 70) {
				s->cargo_type = CT_PASSENGERS;
				s->from = ((Town*)fr.from)->index;
				s->to = ((Town*)fr.to)->index;
				goto add_subsidy;
			}
			FindSubsidyCargoRoute(&fr);
			if (fr.distance <= 70) {
				s->cargo_type = fr.cargo;
				s->from = ((Industry*)fr.from)->index;
				{
					const CargoSpec *cs = GetCargo(fr.cargo);
					s->to = (cs->town_effect == TE_GOODS || cs->town_effect == TE_FOOD) ? ((Town*)fr.to)->index : ((Industry*)fr.to)->index;
				}
	add_subsidy:
				if (!CheckSubsidyDuplicate(s)) {
					s->age = 0;
					pair = SetupSubsidyDecodeParam(s, 0);
					AddNewsItem(STR_2030_SERVICE_SUBSIDY_OFFERED, NS_SUBSIDIES, pair.a, pair.b);
					AI::BroadcastNewEvent(new AIEventSubsidyOffer(s - _subsidies));
					modified = true;
					break;
				}
			}
		} while (n--);
	}
no_add:;
	if (modified)
		InvalidateWindow(WC_SUBSIDIES_LIST, 0);
}

Money GetTransportedGoodsIncome(uint num_pieces, uint dist, byte transit_days, CargoID cargo_type)
{
	const CargoSpec *cs = GetCargo(cargo_type);

	/* Use callback to calculate cargo profit, if available */
	if (HasBit(cs->callback_mask, CBM_CARGO_PROFIT_CALC)) {
		uint32 var18 = min(dist, 0xFFFF) | (min(num_pieces, 0xFF) << 16) | (transit_days << 24);
		uint16 callback = GetCargoCallback(CBID_CARGO_PROFIT_CALC, 0, var18, cs);
		if (callback != CALLBACK_FAILED) {
			int result = GB(callback, 0, 14);

			/* Simulate a 15 bit signed value */
			if (HasBit(callback, 14)) result = 0x4000 - result;

			/* "The result should be a signed multiplier that gets multiplied
			 * by the amount of cargo moved and the price factor, then gets
			 * divided by 8192." */
			return result * num_pieces * _cargo_payment_rates[cargo_type] / 8192;
		}
	}

	/* zero the distance (thus income) if it's the bank and very short transport. */
	if (_settings_game.game_creation.landscape == LT_TEMPERATE && cs->label == 'VALU' && dist < 10) return 0;


	static const int MIN_TIME_FACTOR = 31;
	static const int MAX_TIME_FACTOR = 255;

	const int days1 = cs->transit_days[0];
	const int days2 = cs->transit_days[1];
	const int days_over_days1 = max(   transit_days - days1, 0);
	const int days_over_days2 = max(days_over_days1 - days2, 0);

	/*
	 * The time factor is calculated based on the time it took
	 * (transit_days) compared two cargo-depending values. The
	 * range is divided into three parts:
	 *
	 *  - constant for fast transits
	 *  - linear decreasing with time with a slope of -1 for medium transports
	 *  - linear decreasing with time with a slope of -2 for slow transports
	 *
	 */
	const int time_factor = max(MAX_TIME_FACTOR - days_over_days1 - days_over_days2, MIN_TIME_FACTOR);

	return BigMulS(dist * time_factor * num_pieces, _cargo_payment_rates[cargo_type], 21);
}


struct FindIndustryToDeliverData {
	const Rect *rect;            ///< Station acceptance rectangle
	CargoID cargo_type;          ///< Cargo type that was delivered

	Industry *ind;               ///< Returns found industry
	const IndustrySpec *indspec; ///< Spec of ind
	uint cargo_index;            ///< Index of cargo_type in acceptance list of ind
};

static bool FindIndustryToDeliver(TileIndex ind_tile, void *user_data)
{
	FindIndustryToDeliverData *callback_data = (FindIndustryToDeliverData *)user_data;
	const Rect *rect = callback_data->rect;
	CargoID cargo_type = callback_data->cargo_type;

	/* Only process industry tiles */
	if (!IsTileType(ind_tile, MP_INDUSTRY)) return false;

	/* Only process tiles in the station acceptance rectangle */
	int x = TileX(ind_tile);
	int y = TileY(ind_tile);
	if (x < rect->left || x > rect->right || y < rect->top || y > rect->bottom) return false;

	Industry *ind = GetIndustryByTile(ind_tile);
	const IndustrySpec *indspec = GetIndustrySpec(ind->type);

	uint cargo_index;
	for (cargo_index = 0; cargo_index < lengthof(ind->accepts_cargo); cargo_index++) {
		if (cargo_type == ind->accepts_cargo[cargo_index]) break;
	}
	/* Check if matching cargo has been found */
	if (cargo_index >= lengthof(ind->accepts_cargo)) return false;

	/* Check if industry temporarly refuses acceptance */
	if (HasBit(indspec->callback_flags, CBM_IND_REFUSE_CARGO)) {
		uint16 res = GetIndustryCallback(CBID_INDUSTRY_REFUSE_CARGO, 0, GetReverseCargoTranslation(cargo_type, indspec->grf_prop.grffile), ind, ind->type, ind->xy);
		if (res == 0) return false;
	}

	/* Found industry accepting the cargo */
	callback_data->ind = ind;
	callback_data->indspec = indspec;
	callback_data->cargo_index = cargo_index;
	return true;
}

/**
 * Transfer goods from station to industry.
 * All cargo is delivered to the nearest (Manhattan) industry to the station sign, which is inside the acceptance rectangle and actually accepts the cargo.
 * @param st The station that accepted the cargo
 * @param cargo_type Type of cargo delivered
 * @param nun_pieces Amount of cargo delivered
 * @param industry_set The destination industry will be inserted into this set
 */
static void DeliverGoodsToIndustry(const Station *st, CargoID cargo_type, int num_pieces, SmallIndustryList *industry_set)
{
	if (st->rect.IsEmpty()) return;

	/* Compute acceptance rectangle */
	int catchment_radius = st->GetCatchmentRadius();
	Rect rect = {
		max<int>(st->rect.left   - catchment_radius, 0),
		max<int>(st->rect.top    - catchment_radius, 0),
		min<int>(st->rect.right  + catchment_radius, MapMaxX()),
		min<int>(st->rect.bottom + catchment_radius, MapMaxY())
	};

	/* Compute maximum extent of acceptance rectangle wrt. station sign */
	TileIndex start_tile = st->xy;
	uint max_radius = max(
		max(DistanceManhattan(start_tile, TileXY(rect.left , rect.top)), DistanceManhattan(start_tile, TileXY(rect.left , rect.bottom))),
		max(DistanceManhattan(start_tile, TileXY(rect.right, rect.top)), DistanceManhattan(start_tile, TileXY(rect.right, rect.bottom)))
	);

	FindIndustryToDeliverData callback_data;
	callback_data.rect = &rect;
	callback_data.cargo_type = cargo_type;
	callback_data.ind = NULL;
	callback_data.indspec = NULL;
	callback_data.cargo_index = 0;

	/* Find the nearest industrytile to the station sign inside the catchment area, whose industry accepts the cargo.
	 * This fails in three cases:
	 *  1) The station accepts the cargo because there are enough houses around it accepting the cargo.
	 *  2) The industries in the catchment area temporarily reject the cargo, and the daily station loop has not yet updated station acceptance.
	 *  3) The results of callbacks CBID_INDUSTRY_REFUSE_CARGO and CBID_INDTILE_CARGO_ACCEPTANCE are inconsistent. (documented behaviour)
	 */
	if (CircularTileSearch(&start_tile, 2 * max_radius + 1, FindIndustryToDeliver, &callback_data)) {
		Industry *best = callback_data.ind;
		uint accepted_cargo_index = callback_data.cargo_index;
		assert(best != NULL);

		/* Insert the industry into industry_set, if not yet contained */
		if (industry_set != NULL) industry_set->Include(best);

		best->incoming_cargo_waiting[accepted_cargo_index] = min(num_pieces + best->incoming_cargo_waiting[accepted_cargo_index], 0xFFFF);
	}
}

static bool CheckSubsidised(Station *from, Station *to, CargoID cargo_type)
{
	Subsidy *s;
	TileIndex xy;
	Pair pair;

	/* check if there is an already existing subsidy that applies to us */
	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type == cargo_type &&
				s->age >= 12 &&
				s->from == from->index &&
				s->to == to->index) {
			return true;
		}
	}

	/* check if there's a new subsidy that applies.. */
	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type == cargo_type && s->age < 12) {
			/* Check distance from source */
			const CargoSpec *cs = GetCargo(cargo_type);
			if (cs->town_effect == TE_PASSENGERS || cs->town_effect == TE_MAIL) {
				xy = GetTown(s->from)->xy;
			} else {
				xy = GetIndustry(s->from)->xy;
			}
			if (DistanceMax(xy, from->xy) > 9) continue;

			/* Check distance from dest */
			switch (cs->town_effect) {
				case TE_PASSENGERS:
				case TE_MAIL:
				case TE_GOODS:
				case TE_FOOD:
					xy = GetTown(s->to)->xy;
					break;

				default:
					xy = GetIndustry(s->to)->xy;
					break;
			}
			if (DistanceMax(xy, to->xy) > 9) continue;

			/* Found a subsidy, change the values to indicate that it's in use */
			s->age = 12;
			s->from = from->index;
			s->to = to->index;

			/* Add a news item */
			pair = SetupSubsidyDecodeParam(s, 0);
			InjectDParam(1);

			SetDParam(0, _current_company);
			AddNewsItem(
				STR_2031_SERVICE_SUBSIDY_AWARDED + _settings_game.difficulty.subsidy_multiplier,
				NS_SUBSIDIES,
				pair.a, pair.b
			);
			AI::BroadcastNewEvent(new AIEventSubsidyAwarded(s - _subsidies));

			InvalidateWindow(WC_SUBSIDIES_LIST, 0);
			return true;
		}
	}
	return false;
}

/**
 * Delivers goods to industries/towns and calculates the payment
 * @param num_pieces amount of cargo delivered
 * @param source Originstation of the cargo
 * @param dest Station the cargo has been unloaded
 * @param source_tile The origin of the cargo for distance calculation
 * @param days_in_transit Travel time
 * @param industry_set The delivered industry will be inserted into this set, if not yet contained
 * The cargo is just added to the stockpile of the industry. It is due to the caller to trigger the industry's production machinery
 */
static Money DeliverGoods(int num_pieces, CargoID cargo_type, StationID source, StationID dest, TileIndex source_tile, byte days_in_transit, SmallIndustryList *industry_set)
{
	bool subsidised;
	Station *s_from, *s_to;
	Money profit;

	assert(num_pieces > 0);

	/* Update company statistics */
	{
		Company *c = GetCompany(_current_company);
		c->cur_economy.delivered_cargo += num_pieces;
		SetBit(c->cargo_types, cargo_type);
	}

	/* Get station pointers. */
	s_from = GetStation(source);
	s_to = GetStation(dest);

	/* Check if a subsidy applies. */
	subsidised = CheckSubsidised(s_from, s_to, cargo_type);

	/* Increase town's counter for some special goods types */
	const CargoSpec *cs = GetCargo(cargo_type);
	if (cs->town_effect == TE_FOOD) s_to->town->new_act_food += num_pieces;
	if (cs->town_effect == TE_WATER) s_to->town->new_act_water += num_pieces;

	/* Give the goods to the industry. */
	DeliverGoodsToIndustry(s_to, cargo_type, num_pieces, industry_set);

	/* Determine profit */
	profit = GetTransportedGoodsIncome(num_pieces, DistanceManhattan(source_tile, s_to->xy), days_in_transit, cargo_type);

	/* Modify profit if a subsidy is in effect */
	if (subsidised) {
		switch (_settings_game.difficulty.subsidy_multiplier) {
			case 0:  profit += profit >> 1; break;
			case 1:  profit *= 2; break;
			case 2:  profit *= 3; break;
			default: profit *= 4; break;
		}
	}

	return profit;
}

/**
 * Inform the industry about just delivered cargo
 * DeliverGoodsToIndustry() silently incremented incoming_cargo_waiting, now it is time to do something with the new cargo.
 * @param i The industry to process
 */
static void TriggerIndustryProduction(Industry *i)
{
	const IndustrySpec *indspec = GetIndustrySpec(i->type);
	uint16 callback = indspec->callback_flags;

	i->was_cargo_delivered = true;
	i->last_cargo_accepted_at = _date;

	if (HasBit(callback, CBM_IND_PRODUCTION_CARGO_ARRIVAL) || HasBit(callback, CBM_IND_PRODUCTION_256_TICKS)) {
		if (HasBit(callback, CBM_IND_PRODUCTION_CARGO_ARRIVAL)) {
			IndustryProductionCallback(i, 0);
		} else {
			InvalidateWindow(WC_INDUSTRY_VIEW, i->index);
		}
	} else {
		for (uint cargo_index = 0; cargo_index < lengthof(i->incoming_cargo_waiting); cargo_index++) {
			uint cargo_waiting = i->incoming_cargo_waiting[cargo_index];
			if (cargo_waiting == 0) continue;

			i->produced_cargo_waiting[0] = min(i->produced_cargo_waiting[0] + (cargo_waiting * indspec->input_cargo_multiplier[cargo_index][0] / 256), 0xFFFF);
			i->produced_cargo_waiting[1] = min(i->produced_cargo_waiting[1] + (cargo_waiting * indspec->input_cargo_multiplier[cargo_index][1] / 256), 0xFFFF);

			i->incoming_cargo_waiting[cargo_index] = 0;
		}
	}

	TriggerIndustry(i, INDUSTRY_TRIGGER_RECEIVED_CARGO);
	StartStopIndustryTileAnimation(i, IAT_INDUSTRY_RECEIVED_CARGO);
}

/**
 * Performs the vehicle payment _and_ marks the vehicle to be unloaded.
 * @param front_v the vehicle to be unloaded
 */
void VehiclePayment(Vehicle *front_v)
{
	int result = 0;

	Money vehicle_profit = 0; // Money paid to the train
	Money route_profit   = 0; // The grand total amount for the route. A-D of transfer chain A-B-C-D
	Money virtual_profit = 0; // The virtual profit for entire vehicle chain

	StationID last_visited = front_v->last_station_visited;
	Station *st = GetStation(last_visited);

	/* The owner of the train wants to be paid */
	CompanyID old_company = _current_company;
	_current_company = front_v->owner;

	/* At this moment loading cannot be finished */
	ClrBit(front_v->vehicle_flags, VF_LOADING_FINISHED);

	/* Start unloading in at the first possible moment */
	front_v->load_unload_time_rem = 1;

	/* Collect delivered industries */
	static SmallIndustryList industry_set;
	industry_set.Clear();

	for (Vehicle *v = front_v; v != NULL; v = v->Next()) {
		/* No cargo to unload */
		if (v->cargo_cap == 0 || v->cargo.Empty() || front_v->current_order.GetUnloadType() & OUFB_NO_UNLOAD) continue;

		/* All cargo has already been paid for, no need to pay again */
		if (!v->cargo.UnpaidCargo()) {
			SetBit(v->vehicle_flags, VF_CARGO_UNLOADING);
			continue;
		}

		GoodsEntry *ge = &st->goods[v->cargo_type];
		const CargoList::List *cargos = v->cargo.Packets();

		for (CargoList::List::const_iterator it = cargos->begin(); it != cargos->end(); it++) {
			CargoPacket *cp = *it;
			if (!cp->paid_for &&
					cp->source != last_visited &&
					HasBit(ge->acceptance_pickup, GoodsEntry::ACCEPTANCE) &&
					(front_v->current_order.GetUnloadType() & OUFB_TRANSFER) == 0) {
				/* Deliver goods to the station */
				st->time_since_unload = 0;

				/* handle end of route payment */
				Money profit = DeliverGoods(cp->count, v->cargo_type, cp->source, last_visited, cp->source_xy, cp->days_in_transit, &industry_set);
				cp->paid_for = true;
				route_profit   += profit; // display amount paid for final route delivery, A-D of a chain A-B-C-D
				vehicle_profit += profit - cp->feeder_share;                    // whole vehicle is not payed for transfers picked up earlier

				result |= 1;

				SetBit(v->vehicle_flags, VF_CARGO_UNLOADING);
			} else if (front_v->current_order.GetUnloadType() & (OUFB_UNLOAD | OUFB_TRANSFER)) {
				if (!cp->paid_for && (front_v->current_order.GetUnloadType() & OUFB_TRANSFER) != 0) {
					Money profit = GetTransportedGoodsIncome(
						cp->count,
						/* pay transfer vehicle for only the part of transfer it has done: ie. cargo_loaded_at_xy to here */
						DistanceManhattan(cp->loaded_at_xy, GetStation(last_visited)->xy),
						cp->days_in_transit,
						v->cargo_type);

					front_v->profit_this_year += profit << 8;
					virtual_profit   += profit; // accumulate transfer profits for whole vehicle
					cp->feeder_share += profit; // account for the (virtual) profit already made for the cargo packet
					cp->paid_for      = true;   // record that the cargo has been paid for to eliminate double counting
				}
				result |= 2;

				SetBit(v->vehicle_flags, VF_CARGO_UNLOADING);
			}
		}
		v->cargo.InvalidateCache();
	}

	/* Call the production machinery of industries only once for every vehicle chain */
	const Industry * const *isend = industry_set.End();
	for (Industry **iid = industry_set.Begin(); iid != isend; iid++) {
		TriggerIndustryProduction(*iid);
	}

	if (virtual_profit > 0) {
		ShowFeederIncomeAnimation(front_v->x_pos, front_v->y_pos, front_v->z_pos, virtual_profit);
	}

	if (route_profit != 0) {
		front_v->profit_this_year += vehicle_profit << 8;
		SubtractMoneyFromCompany(CommandCost(front_v->GetExpenseType(true), -route_profit));

		if (IsLocalCompany() && !PlayVehicleSound(front_v, VSE_LOAD_UNLOAD)) {
			SndPlayVehicleFx(SND_14_CASHTILL, front_v);
		}

		ShowCostOrIncomeAnimation(front_v->x_pos, front_v->y_pos, front_v->z_pos, -vehicle_profit);
	}

	_current_company = old_company;
}

/**
 * Loads/unload the vehicle if possible.
 * @param v the vehicle to be (un)loaded
 * @param cargo_left the amount of each cargo type that is
 *                   virtually left on the platform to be
 *                   picked up by another vehicle when all
 *                   previous vehicles have loaded.
 */
static void LoadUnloadVehicle(Vehicle *v, int *cargo_left)
{
	assert(v->current_order.IsType(OT_LOADING));

	/* We have not waited enough time till the next round of loading/unloading */
	if (--v->load_unload_time_rem != 0) {
		if (_settings_game.order.improved_load && (v->current_order.GetLoadType() & OLFB_FULL_LOAD)) {
			/* 'Reserve' this cargo for this vehicle, because we were first. */
			for (; v != NULL; v = v->Next()) {
				int cap_left = v->cargo_cap - v->cargo.Count();
				if (cap_left > 0) cargo_left[v->cargo_type] -= cap_left;
			}
		}
		return;
	}

	StationID last_visited = v->last_station_visited;
	Station *st = GetStation(last_visited);

	if (v->type == VEH_TRAIN && (!IsTileType(v->tile, MP_STATION) || GetStationIndex(v->tile) != st->index)) {
		/* The train reversed in the station. Take the "easy" way
		 * out and let the train just leave as it always did. */
		SetBit(v->vehicle_flags, VF_LOADING_FINISHED);
		return;
	}

	int unloading_time = 0;
	Vehicle *u = v;
	int result = 0;

	bool completely_emptied = true;
	bool anything_unloaded = false;
	bool anything_loaded   = false;
	uint32 cargo_not_full  = 0;
	uint32 cargo_full      = 0;

	v->cur_speed = 0;

	for (; v != NULL; v = v->Next()) {
		if (v->cargo_cap == 0) continue;

		byte load_amount = EngInfo(v->engine_type)->load_amount;

		/* The default loadamount for mail is 1/4 of the load amount for passengers */
		if (v->type == VEH_AIRCRAFT && !IsNormalAircraft(v)) load_amount = (load_amount + 3) / 4;

		if (_settings_game.order.gradual_loading && HasBit(EngInfo(v->engine_type)->callbackmask, CBM_VEHICLE_LOAD_AMOUNT)) {
			uint16 cb_load_amount = GetVehicleCallback(CBID_VEHICLE_LOAD_AMOUNT, 0, 0, v->engine_type, v);
			if (cb_load_amount != CALLBACK_FAILED && GB(cb_load_amount, 0, 8) != 0) load_amount = GB(cb_load_amount, 0, 8);
		}

		GoodsEntry *ge = &st->goods[v->cargo_type];

		if (HasBit(v->vehicle_flags, VF_CARGO_UNLOADING) && (u->current_order.GetUnloadType() & OUFB_NO_UNLOAD) == 0) {
			uint cargo_count = v->cargo.Count();
			uint amount_unloaded = _settings_game.order.gradual_loading ? min(cargo_count, load_amount) : cargo_count;
			bool remaining = false; // Are there cargo entities in this vehicle that can still be unloaded here?
			bool accepted  = false; // Is the cargo accepted by the station?

			if (HasBit(ge->acceptance_pickup, GoodsEntry::ACCEPTANCE) && !(u->current_order.GetUnloadType() & OUFB_TRANSFER)) {
				/* The cargo has reached it's final destination, the packets may now be destroyed */
				remaining = v->cargo.MoveTo(NULL, amount_unloaded, CargoList::MTA_FINAL_DELIVERY, last_visited);

				result |= 1;
				accepted = true;
			}

			/* The !accepted || v->cargo.Count == cargo_count clause is there
			 * to make it possible to force unload vehicles at the station where
			 * they were loaded, but to not force unload the vehicle when the
			 * station is still accepting the cargo in the vehicle. It doesn't
			 * accept cargo that was loaded at the same station. */
			if (u->current_order.GetUnloadType() & (OUFB_UNLOAD | OUFB_TRANSFER) && (!accepted || v->cargo.Count() == cargo_count)) {
				remaining = v->cargo.MoveTo(&ge->cargo, amount_unloaded);
				SetBit(ge->acceptance_pickup, GoodsEntry::PICKUP);

				result |= 2;
			} else if (!accepted) {
				/* The order changed while unloading (unset unload/transfer) or the
				 * station does not accept goods anymore. */
				ClrBit(v->vehicle_flags, VF_CARGO_UNLOADING);
				continue;
			}

			/* Deliver goods to the station */
			st->time_since_unload = 0;

			unloading_time += amount_unloaded;

			anything_unloaded = true;
			if (_settings_game.order.gradual_loading && remaining) {
				completely_emptied = false;
			} else {
				/* We have finished unloading (cargo count == 0) */
				ClrBit(v->vehicle_flags, VF_CARGO_UNLOADING);
			}

			continue;
		}

		/* Do not pick up goods when we have no-load set. */
		if (u->current_order.GetLoadType() & OLFB_NO_LOAD) continue;

		/* update stats */
		int t;
		switch (u->type) {
			case VEH_TRAIN: t = u->u.rail.cached_max_speed; break;
			case VEH_ROAD:  t = u->max_speed / 2;           break;
			default:        t = u->max_speed;               break;
		}

		/* if last speed is 0, we treat that as if no vehicle has ever visited the station. */
		ge->last_speed = min(t, 255);
		ge->last_age = _cur_year - u->build_year;
		ge->days_since_pickup = 0;

		/* If there's goods waiting at the station, and the vehicle
		 * has capacity for it, load it on the vehicle. */
		int cap_left = v->cargo_cap - v->cargo.Count();
		if (!ge->cargo.Empty() && cap_left > 0) {
			uint cap = cap_left;
			uint count = ge->cargo.Count();

			/* Skip loading this vehicle if another train/vehicle is already handling
			 * the same cargo type at this station */
			if (_settings_game.order.improved_load && cargo_left[v->cargo_type] <= 0) {
				SetBit(cargo_not_full, v->cargo_type);
				continue;
			}

			if (cap > count) cap = count;
			if (_settings_game.order.gradual_loading) cap = min(cap, load_amount);
			if (_settings_game.order.improved_load) {
				/* Don't load stuff that is already 'reserved' for other vehicles */
				cap = min((uint)cargo_left[v->cargo_type], cap);
				cargo_left[v->cargo_type] -= cap;
			}

			if (v->cargo.Empty()) TriggerVehicle(v, VEHICLE_TRIGGER_NEW_CARGO);

			/* TODO: Regarding this, when we do gradual loading, we
			 * should first unload all vehicles and then start
			 * loading them. Since this will cause
			 * VEHICLE_TRIGGER_EMPTY to be called at the time when
			 * the whole vehicle chain is really totally empty, the
			 * completely_emptied assignment can then be safely
			 * removed; that's how TTDPatch behaves too. --pasky */
			completely_emptied = false;
			anything_loaded = true;

			ge->cargo.MoveTo(&v->cargo, cap, CargoList::MTA_CARGO_LOAD, st->xy);

			st->time_since_load = 0;
			st->last_vehicle_type = v->type;

			StationAnimationTrigger(st, st->xy, STAT_ANIM_CARGO_TAKEN, v->cargo_type);

			unloading_time += cap;

			result |= 2;
		}

		if (v->cargo.Count() >= v->cargo_cap) {
			SetBit(cargo_full, v->cargo_type);
		} else {
			SetBit(cargo_not_full, v->cargo_type);
		}
	}

	/* Only set completly_emptied, if we just unloaded all remaining cargo */
	completely_emptied &= anything_unloaded;

	/* We update these variables here, so gradual loading still fills
	 * all wagons at the same time instead of using the same 'improved'
	 * loading algorithm for the wagons (only fill wagon when there is
	 * enough to fill the previous wagons) */
	if (_settings_game.order.improved_load && (u->current_order.GetLoadType() & OLFB_FULL_LOAD)) {
		/* Update left cargo */
		for (v = u; v != NULL; v = v->Next()) {
			int cap_left = v->cargo_cap - v->cargo.Count();
			if (cap_left > 0) cargo_left[v->cargo_type] -= cap_left;
		}
	}

	v = u;

	if (anything_loaded || anything_unloaded) {
		if (_settings_game.order.gradual_loading) {
			/* The time it takes to load one 'slice' of cargo or passengers depends
			 * on the vehicle type - the values here are those found in TTDPatch */
			const uint gradual_loading_wait_time[] = { 40, 20, 10, 20 };

			unloading_time = gradual_loading_wait_time[v->type];
		}
	} else {
		bool finished_loading = true;
		if (v->current_order.GetLoadType() & OLFB_FULL_LOAD) {
			if (v->current_order.GetLoadType() == OLF_FULL_LOAD_ANY) {
				/* if the aircraft carries passengers and is NOT full, then
				 * continue loading, no matter how much mail is in */
				if ((v->type == VEH_AIRCRAFT && IsCargoInClass(v->cargo_type, CC_PASSENGERS) && v->cargo_cap > v->cargo.Count()) ||
						(cargo_not_full && (cargo_full & ~cargo_not_full) == 0)) { // There are stull non-full cargos
					finished_loading = false;
				}
			} else if (cargo_not_full != 0) {
				finished_loading = false;
			}
		}
		unloading_time = 20;

		SB(v->vehicle_flags, VF_LOADING_FINISHED, 1, finished_loading);
	}

	if (v->type == VEH_TRAIN) {
		/* Each platform tile is worth 2 rail vehicles. */
		int overhang = v->u.rail.cached_total_length - st->GetPlatformLength(v->tile) * TILE_SIZE;
		if (overhang > 0) {
			unloading_time <<= 1;
			unloading_time += (overhang * unloading_time) / 8;
		}
	}

	/* Calculate the loading indicator fill percent and display
	 * In the Game Menu do not display indicators
	 * If _settings_client.gui.loading_indicators == 2, show indicators (bool can be promoted to int as 0 or 1 - results in 2 > 0,1 )
	 * if _settings_client.gui.loading_indicators == 1, _local_company must be the owner or must be a spectator to show ind., so 1 > 0
	 * if _settings_client.gui.loading_indicators == 0, do not display indicators ... 0 is never greater than anything
	 */
	if (_game_mode != GM_MENU && (_settings_client.gui.loading_indicators > (uint)(v->owner != _local_company && _local_company != COMPANY_SPECTATOR))) {
		StringID percent_up_down = STR_NULL;
		int percent = CalcPercentVehicleFilled(v, &percent_up_down);
		if (v->fill_percent_te_id == INVALID_TE_ID) {
			v->fill_percent_te_id = ShowFillingPercent(v->x_pos, v->y_pos, v->z_pos + 20, percent, percent_up_down);
		} else {
			UpdateFillingPercent(v->fill_percent_te_id, percent, percent_up_down);
		}
	}

	v->load_unload_time_rem = unloading_time;

	if (completely_emptied) {
		TriggerVehicle(v, VEHICLE_TRIGGER_EMPTY);
	}

	if (result != 0) {
		InvalidateWindow(GetWindowClassForVehicleType(v->type), v->owner);
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

		st->MarkTilesDirty(true);
		v->MarkDirty();

		if (result & 2) InvalidateWindow(WC_STATION_VIEW, last_visited);
	}
}

/**
 * Load/unload the vehicles in this station according to the order
 * they entered.
 * @param st the station to do the loading/unloading for
 */
void LoadUnloadStation(Station *st)
{
	int cargo_left[NUM_CARGO];

	for (uint i = 0; i < NUM_CARGO; i++) cargo_left[i] = st->goods[i].cargo.Count();

	std::list<Vehicle *>::iterator iter;
	for (iter = st->loading_vehicles.begin(); iter != st->loading_vehicles.end(); ++iter) {
		Vehicle *v = *iter;
		if (!(v->vehstatus & (VS_STOPPED | VS_CRASHED))) LoadUnloadVehicle(v, cargo_left);
	}
}

void CompaniesMonthlyLoop()
{
	CompaniesGenStatistics();
	if (_settings_game.economy.inflation) AddInflation();
	CompaniesPayInterest();
	/* Reset the _current_company flag */
	_current_company = OWNER_NONE;
	HandleEconomyFluctuations();
	SubsidyMonthlyHandler();
}

static void DoAcquireCompany(Company *c)
{
	Company *owner;
	int i;
	Money value;
	CompanyID ci = c->index;

	CompanyNewsInformation *cni = MallocT<CompanyNewsInformation>(1);
	cni->FillData(c, GetCompany(_current_company));

	SetDParam(0, STR_7059_TRANSPORT_COMPANY_MERGER);
	SetDParam(1, c->bankrupt_value == 0 ? STR_707F_HAS_BEEN_TAKEN_OVER_BY : STR_705A_HAS_BEEN_SOLD_TO_FOR);
	SetDParamStr(2, cni->company_name);
	SetDParamStr(3, cni->other_company_name);
	SetDParam(4, c->bankrupt_value);
	AddNewsItem(STR_02B6, NS_COMPANY_MERGER, 0, 0, cni);
	AI::BroadcastNewEvent(new AIEventCompanyMerger(ci, _current_company));

	/* original code does this a little bit differently */
	ChangeNetworkOwner(ci, _current_company);
	ChangeOwnershipOfCompanyItems(ci, _current_company);

	if (c->bankrupt_value == 0) {
		owner = GetCompany(_current_company);
		owner->current_loan += c->current_loan;
	}

	value = CalculateCompanyValue(c) >> 2;
	CompanyID old_company = _current_company;
	for (i = 0; i != 4; i++) {
		if (c->share_owners[i] != COMPANY_SPECTATOR) {
			_current_company = c->share_owners[i];
			SubtractMoneyFromCompany(CommandCost(EXPENSES_OTHER, -value));
		}
	}
	_current_company = old_company;

	if (!IsHumanCompany(c->index)) AI::Stop(c->index);

	DeleteCompanyWindows(ci);
	InvalidateWindowClassesData(WC_TRAINS_LIST, 0);
	InvalidateWindowClassesData(WC_SHIPS_LIST, 0);
	InvalidateWindowClassesData(WC_ROADVEH_LIST, 0);
	InvalidateWindowClassesData(WC_AIRCRAFT_LIST, 0);

	delete c;
}

extern int GetAmountOwnedBy(const Company *c, Owner owner);

/** Acquire shares in an opposing company.
 * @param tile unused
 * @param flags type of operation
 * @param p1 company to buy the shares from
 * @param p2 unused
 */
CommandCost CmdBuyShareInCompany(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CommandCost cost(EXPENSES_OTHER);

	/* Check if buying shares is allowed (protection against modified clients)
	 * Cannot buy own shares */
	if (!IsValidCompanyID((CompanyID)p1) || !_settings_game.economy.allow_shares || _current_company == (CompanyID)p1) return CMD_ERROR;

	Company *c = GetCompany((CompanyID)p1);

	/* Protect new companies from hostile takeovers */
	if (_cur_year - c->inaugurated_year < 6) return_cmd_error(STR_PROTECTED);

	/* Those lines are here for network-protection (clients can be slow) */
	if (GetAmountOwnedBy(c, COMPANY_SPECTATOR) == 0) return cost;

	/* We can not buy out a real company (temporarily). TODO: well, enable it obviously */
	if (GetAmountOwnedBy(c, COMPANY_SPECTATOR) == 1 && !c->is_ai) return cost;

	cost.AddCost(CalculateCompanyValue(c) >> 2);
	if (flags & DC_EXEC) {
		OwnerByte *b = c->share_owners;
		int i;

		while (*b != COMPANY_SPECTATOR) b++; // share owners is guaranteed to contain at least one COMPANY_SPECTATOR
		*b = _current_company;

		for (i = 0; c->share_owners[i] == _current_company;) {
			if (++i == 4) {
				c->bankrupt_value = 0;
				DoAcquireCompany(c);
				break;
			}
		}
		InvalidateWindow(WC_COMPANY, p1);
	}
	return cost;
}

/** Sell shares in an opposing company.
 * @param tile unused
 * @param flags type of operation
 * @param p1 company to sell the shares from
 * @param p2 unused
 */
CommandCost CmdSellShareInCompany(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	/* Check if selling shares is allowed (protection against modified clients)
	 * Cannot sell own shares */
	if (!IsValidCompanyID((CompanyID)p1) || !_settings_game.economy.allow_shares || _current_company == (CompanyID)p1) return CMD_ERROR;

	Company *c = GetCompany((CompanyID)p1);

	/* Those lines are here for network-protection (clients can be slow) */
	if (GetAmountOwnedBy(c, _current_company) == 0) return CommandCost();

	/* adjust it a little to make it less profitable to sell and buy */
	Money cost = CalculateCompanyValue(c) >> 2;
	cost = -(cost - (cost >> 7));

	if (flags & DC_EXEC) {
		OwnerByte *b = c->share_owners;
		while (*b != _current_company) b++; // share owners is guaranteed to contain company
		*b = COMPANY_SPECTATOR;
		InvalidateWindow(WC_COMPANY, p1);
	}
	return CommandCost(EXPENSES_OTHER, cost);
}

/** Buy up another company.
 * When a competing company is gone bankrupt you get the chance to purchase
 * that company.
 * @todo currently this only works for AI companies
 * @param tile unused
 * @param flags type of operation
 * @param p1 company to buy up
 * @param p2 unused
 */
CommandCost CmdBuyCompany(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CompanyID cid = (CompanyID)p1;

	/* Disable takeovers in multiplayer games */
	if (!IsValidCompanyID(cid) || _networking) return CMD_ERROR;

	/* Do not allow companies to take over themselves */
	if (cid == _current_company) return CMD_ERROR;

	Company *c = GetCompany(cid);

	if (!c->is_ai) return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoAcquireCompany(c);
	}
	return CommandCost(EXPENSES_OTHER, c->bankrupt_value);
}
