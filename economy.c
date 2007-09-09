/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "functions.h"
#include "strings.h" // XXX InjectDParam()
#include "table/strings.h"
#include "table/sprites.h"
#include "map.h"
#include "news.h"
#include "player.h"
#include "station.h"
#include "vehicle.h"
#include "window.h"
#include "gfx.h"
#include "command.h"
#include "saveload.h"
#include "economy.h"
#include "industry.h"
#include "town.h"
#include "network.h"
#include "sound.h"
#include "engine.h"
#include "network_data.h"
#include "variables.h"
#include "vehicle_gui.h"
#include "ai/ai.h"
#include "train.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "newgrf_callbacks.h"
#include "unmovable.h"
#include "date.h"

// Score info
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

int _score_part[MAX_PLAYERS][NUM_SCORE];

int64 CalculateCompanyValue(const Player* p)
{
	PlayerID owner = p->index;
	int64 value;

	{
		Station *st;
		uint num = 0;

		FOR_ALL_STATIONS(st) {
			if (st->owner == owner) {
				uint facil = st->facilities;
				do num += (facil&1); while (facil >>= 1);
			}
		}

		value = num * _price.station_value * 25;
	}

	{
		Vehicle *v;

		FOR_ALL_VEHICLES(v) {
			if (v->owner != owner) continue;

			if (v->type == VEH_Train ||
					v->type == VEH_Road ||
					(v->type == VEH_Aircraft && v->subtype<=2) ||
					v->type == VEH_Ship) {
				value += v->value * 3 >> 1;
			}
		}
	}

	value += p->money64 - p->current_loan; // add real money value

	return max64(value, 1);
}

// if update is set to true, the economy is updated with this score
//  (also the house is updated, should only be true in the on-tick event)
int UpdateCompanyRatingAndValue(Player *p, bool update)
{
	byte owner = p->index;
	int score = 0;

	memset(_score_part[owner], 0, sizeof(_score_part[owner]));

/* Count vehicles */
	{
		Vehicle *v;
		int32 min_profit = 0;
		bool min_profit_first = true;
		uint num = 0;

		FOR_ALL_VEHICLES(v) {
			if (v->owner != owner) continue;
			if ((v->type == VEH_Train && IsFrontEngine(v)) ||
					 v->type == VEH_Road ||
					(v->type == VEH_Aircraft && v->subtype <= 2) ||
					 v->type == VEH_Ship) {
				num++;
				if (v->age > 730) {
					/* Find the vehicle with the lowest amount of profit */
					if (min_profit_first == true) {
						min_profit = v->profit_last_year;
						min_profit_first = false;
					} else if (min_profit > v->profit_last_year) {
						min_profit = v->profit_last_year;
					}
				}
			}
		}

		_score_part[owner][SCORE_VEHICLES] = num;
		/* Don't allow negative min_profit to show */
		if (min_profit > 0)
			_score_part[owner][SCORE_MIN_PROFIT] = min_profit;
	}

/* Count stations */
	{
		uint num = 0;
		const Station* st;

		FOR_ALL_STATIONS(st) {
			if (st->owner == owner) {
				int facil = st->facilities;
				do num += facil&1; while (facil>>=1);
			}
		}
		_score_part[owner][SCORE_STATIONS] = num;
	}

/* Generate statistics depending on recent income statistics */
	{
		const PlayerEconomyEntry* pee;
		int numec;
		int32 min_income;
		int32 max_income;

		numec = min(p->num_valid_stat_ent, 12);
		if (numec != 0) {
			min_income = 0x7FFFFFFF;
			max_income = 0;
			pee = p->old_economy;
			do {
				min_income = min(min_income, pee->income + pee->expenses);
				max_income = max(max_income, pee->income + pee->expenses);
			} while (++pee,--numec);

			if (min_income > 0)
				_score_part[owner][SCORE_MIN_INCOME] = min_income;

			_score_part[owner][SCORE_MAX_INCOME] = max_income;
		}
	}

/* Generate score depending on amount of transported cargo */
	{
		const PlayerEconomyEntry* pee;
		int numec;
		uint32 total_delivered;

		numec = min(p->num_valid_stat_ent, 4);
		if (numec != 0) {
			pee = p->old_economy;
			total_delivered = 0;
			do {
				total_delivered += pee->delivered_cargo;
			} while (++pee,--numec);

			_score_part[owner][SCORE_DELIVERED] = total_delivered;
		}
	}

/* Generate score for variety of cargo */
	{
		uint cargo = p->cargo_types;
		uint num = 0;
		do num += cargo&1; while (cargo>>=1);
		_score_part[owner][SCORE_CARGO] = num;
		if (update) p->cargo_types = 0;
	}

/* Generate score for player money */
	{
		int32 money = p->player_money;
		if (money > 0) {
			_score_part[owner][SCORE_MONEY] = money;
		}
	}

/* Generate score for loan */
	{
		_score_part[owner][SCORE_LOAN] = _score_info[SCORE_LOAN].needed - p->current_loan;
	}

	// Now we calculate the score for each item..
	{
		int i;
		int total_score = 0;
		int s;
		score = 0;
		for (i = 0; i < NUM_SCORE; i++) {
			// Skip the total
			if (i == SCORE_TOTAL) continue;
			// Check the score
			s = clamp(_score_part[owner][i], 0, _score_info[i].needed) * _score_info[i].score / _score_info[i].needed;
			score += s;
			total_score += _score_info[i].score;
		}

		_score_part[owner][SCORE_TOTAL] = score;

		// We always want the score scaled to SCORE_MAX (1000)
		if (total_score != SCORE_MAX) score = score * SCORE_MAX / total_score;
	}

	if (update) {
		p->old_economy[0].performance_history = score;
		UpdateCompanyHQ(p, score);
		p->old_economy[0].company_value = CalculateCompanyValue(p);
	}

	InvalidateWindow(WC_PERFORMANCE_DETAIL, 0);
	return score;
}

// use PLAYER_SPECTATOR as new_player to delete the player.
void ChangeOwnershipOfPlayerItems(PlayerID old_player, PlayerID new_player)
{
	Town *t;
	PlayerID old = _current_player;

	assert(old_player != new_player);

	{
		Player *p;
		uint i;

		/* See if the old_player had shares in other companies */
		_current_player = old_player;
		FOR_ALL_PLAYERS(p) {
			for (i = 0; i < 4; i++) {
				if (p->share_owners[i] == old_player) {
					/* Sell his shares */
					int32 res = DoCommand(0, p->index, 0, DC_EXEC, CMD_SELL_SHARE_IN_COMPANY);
					/* Because we are in a DoCommand, we can't just execute an other one and
					 *  expect the money to be removed. We need to do it ourself! */
					SubtractMoneyFromPlayer(res);
				}
			}
		}

		/* Sell all the shares that people have on this company */
		p = GetPlayer(old_player);
		for (i = 0; i < 4; i++) {
			_current_player = p->share_owners[i];
			if (_current_player != PLAYER_SPECTATOR) {
				/* Sell the shares */
				int32 res = DoCommand(0, old_player, 0, DC_EXEC, CMD_SELL_SHARE_IN_COMPANY);
				/* Because we are in a DoCommand, we can't just execute an other one and
				 *  expect the money to be removed. We need to do it ourself! */
				SubtractMoneyFromPlayer(res);
			}
		}
	}

	_current_player = old_player;

	/* Temporarily increase the player's money, to be sure that
	 * removing his/her property doesn't fail because of lack of money.
	 * Not too drastically though, because it could overflow */
	if (new_player == PLAYER_SPECTATOR) {
		GetPlayer(old_player)->money64 = MAX_UVALUE(uint64) >>2; // jackpot ;p
		UpdatePlayerMoney32(GetPlayer(old_player));
	}

	if (new_player == PLAYER_SPECTATOR) {
		Subsidy *s;

		for (s = _subsidies; s != endof(_subsidies); s++) {
			if (s->cargo_type != CT_INVALID && s->age >= 12) {
				if (GetStation(s->to)->owner == old_player) s->cargo_type = CT_INVALID;
			}
		}
	}

	/* Take care of rating in towns */
	FOR_ALL_TOWNS(t) {
		/* If a player takes over, give the ratings to that player. */
		if (new_player != PLAYER_SPECTATOR) {
			if (HASBIT(t->have_ratings, old_player)) {
				if (HASBIT(t->have_ratings, new_player)) {
					// use max of the two ratings.
					t->ratings[new_player] = max(t->ratings[new_player], t->ratings[old_player]);
				} else {
					SETBIT(t->have_ratings, new_player);
					t->ratings[new_player] = t->ratings[old_player];
				}
			}
		}

		/* Reset the ratings for the old player */
		t->ratings[old_player] = 500;
		CLRBIT(t->have_ratings, old_player);
	}

	{
		int num_train = 0;
		int num_road = 0;
		int num_ship = 0;
		int num_aircraft = 0;
		Vehicle *v;

		// Determine Ids for the new vehicles
		FOR_ALL_VEHICLES(v) {
			if (v->owner == new_player) {
				switch (v->type) {
					case VEH_Train:    if (IsFrontEngine(v)) num_train++; break;
					case VEH_Road:     num_road++; break;
					case VEH_Ship:     num_ship++; break;
					case VEH_Aircraft: if (v->subtype <= 2) num_aircraft++; break;
					default: break;
				}
			}
		}

		FOR_ALL_VEHICLES(v) {
			if (v->owner == old_player && IS_BYTE_INSIDE(v->type, VEH_Train, VEH_Aircraft + 1)) {
				if (new_player == PLAYER_SPECTATOR) {
					DeleteWindowById(WC_VEHICLE_VIEW, v->index);
					DeleteWindowById(WC_VEHICLE_DETAILS, v->index);
					DeleteWindowById(WC_VEHICLE_ORDERS, v->index);
					DeleteVehicle(v);
				} else {
					v->owner = new_player;
					if (IsEngineCountable(v)) GetPlayer(new_player)->num_engines[v->engine_type]++;
					switch (v->type) {
						case VEH_Train:    if (IsFrontEngine(v)) v->unitnumber = ++num_train; break;
						case VEH_Road:     v->unitnumber = ++num_road; break;
						case VEH_Ship:     v->unitnumber = ++num_ship; break;
						case VEH_Aircraft: if (v->subtype <= 2) v->unitnumber = ++num_aircraft; break;
					}
				}
			}
		}
	}

	// Change ownership of tiles
	{
		TileIndex tile = 0;
		do {
			ChangeTileOwner(tile, old_player, new_player);
		} while (++tile != MapSize());
	}

	/* Change color of existing windows */
	if (new_player != PLAYER_SPECTATOR) ChangeWindowOwner(old_player, new_player);

	_current_player = old;

	MarkWholeScreenDirty();
}

static void ChangeNetworkOwner(PlayerID current_player, PlayerID new_player)
{
#ifdef ENABLE_NETWORK
	NetworkClientState *cs;
	NetworkClientInfo *ci;
	if (!_networking) return;

	if (current_player == _local_player) {
		_network_playas = new_player;
		SetLocalPlayer(new_player);
	}

	if (!_network_server) return;

	/* The server has to handle all administrative issues, for example
	* updating and notifying all clients of what has happened */
	ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);

	/* The server has just changed from player */
	if (current_player == ci->client_playas) {
		ci->client_playas = new_player;
		NetworkUpdateClientInfo(NETWORK_SERVER_INDEX);
	}

	/* Find all clients that were in control of this company, and mark them as new_player */
	FOR_ALL_CLIENTS(cs) {
		ci = DEREF_CLIENT_INFO(cs);
		if (current_player == ci->client_playas) {
			ci->client_playas = new_player;
			NetworkUpdateClientInfo(ci->client_index);
		}
	}
#endif /* ENABLE_NETWORK */
}

static void PlayersCheckBankrupt(Player *p)
{
	PlayerID owner;
	int64 val;

	// If the player has money again, it does not go bankrupt
	if (p->player_money >= 0) {
		p->quarters_of_bankrupcy = 0;
		return;
	}

	p->quarters_of_bankrupcy++;

	owner = p->index;

	switch (p->quarters_of_bankrupcy) {
		case 2:
			AddNewsItem( (StringID)(owner | NB_BTROUBLE),
				NEWS_FLAGS(NM_CALLBACK, 0, NT_COMPANY_INFO, DNC_BANKRUPCY),0,0);
			break;
		case 3: {
			/* XXX - In multiplayer, should we ask other players if it wants to take
		          over when it is a human company? -- TrueLight */
			if (IsHumanPlayer(owner)) {
				AddNewsItem( (StringID)(owner | NB_BTROUBLE),
					NEWS_FLAGS(NM_CALLBACK, 0, NT_COMPANY_INFO, DNC_BANKRUPCY),0,0);
				break;
			}

			// Check if the company has any value.. if not, declare it bankrupt
			//  right now
			val = CalculateCompanyValue(p);
			if (val > 0) {
				p->bankrupt_value = val;
				p->bankrupt_asked = 1 << owner; // Don't ask the owner
				p->bankrupt_timeout = 0;
				break;
			}
			// Else, falltrue to case 4...
		}
		case 4: {
			// Close everything the owner has open
			DeletePlayerWindows(owner);

//		Show bankrupt news
			SetDParam(0, p->name_1);
			SetDParam(1, p->name_2);
			AddNewsItem( (StringID)(owner | NB_BBANKRUPT), NEWS_FLAGS(NM_CALLBACK, 0, NT_COMPANY_INFO, DNC_BANKRUPCY),0,0);

			if (IsHumanPlayer(owner)) {
				/* XXX - If we are in offline mode, leave the player playing. Eg. there
				 * is no THE-END, otherwise mark the player as spectator to make sure
				 * he/she is no long in control of this company */
				if (!_networking) {
					p->bankrupt_asked = 0xFF;
					p->bankrupt_timeout = 0x456;
					break;
				}

				ChangeNetworkOwner(owner, PLAYER_SPECTATOR);
			}

			/* Remove the player */
			ChangeOwnershipOfPlayerItems(owner, PLAYER_SPECTATOR);
			/* Register the player as not-active */
			p->is_active = false;

			if (!IsHumanPlayer(owner) && (!_networking || _network_server) && _ai.enabled)
				AI_PlayerDied(owner);
		}
	}
}

void DrawNewsBankrupcy(Window *w)
{
	Player *p;

	DrawNewsBorder(w);

	p = GetPlayer(GB(WP(w,news_d).ni->string_id, 0, 4));
	DrawPlayerFace(p->face, p->player_color, 2, 23);
	GfxFillRect(3, 23, 3+91, 23+118, 0x323 | USE_COLORTABLE);

	SetDParam(0, p->president_name_1);
	SetDParam(1, p->president_name_2);

	DrawStringMultiCenter(49, 148, STR_7058_PRESIDENT, 94);

	switch (WP(w,news_d).ni->string_id & 0xF0) {
	case NB_BTROUBLE:
		DrawStringCentered(w->width>>1, 1, STR_7056_TRANSPORT_COMPANY_IN_TROUBLE, 0);

		SetDParam(0, p->name_1);
		SetDParam(1, p->name_2);

		DrawStringMultiCenter(
			((w->width - 101) >> 1) + 98,
			90,
			STR_7057_WILL_BE_SOLD_OFF_OR_DECLARED,
			w->width - 101);
		break;

	case NB_BMERGER: {
		int32 price;

		DrawStringCentered(w->width>>1, 1, STR_7059_TRANSPORT_COMPANY_MERGER, 0);
		COPY_IN_DPARAM(0,WP(w,news_d).ni->params, 2);
		SetDParam(2, p->name_1);
		SetDParam(3, p->name_2);
		price = WP(w,news_d).ni->params[2];
		SetDParam(4, price);
		DrawStringMultiCenter(
			((w->width - 101) >> 1) + 98,
			90,
			price==0 ? STR_707F_HAS_BEEN_TAKEN_OVER_BY : STR_705A_HAS_BEEN_SOLD_TO_FOR,
			w->width - 101);
		break;
	}

	case NB_BBANKRUPT:
		DrawStringCentered(w->width>>1, 1, STR_705C_BANKRUPT, 0);
		COPY_IN_DPARAM(0,WP(w,news_d).ni->params, 2);
		DrawStringMultiCenter(
			((w->width - 101) >> 1) + 98,
			90,
			STR_705D_HAS_BEEN_CLOSED_DOWN_BY,
			w->width - 101);
		break;

	case NB_BNEWCOMPANY:
		DrawStringCentered(w->width>>1, 1, STR_705E_NEW_TRANSPORT_COMPANY_LAUNCHED, 0);
		SetDParam(0, p->name_1);
		SetDParam(1, p->name_2);
		COPY_IN_DPARAM(2,WP(w,news_d).ni->params, 2);
		DrawStringMultiCenter(
			((w->width - 101) >> 1) + 98,
			90,
			STR_705F_STARTS_CONSTRUCTION_NEAR,
			w->width - 101);
		break;

	default:
		NOT_REACHED();
	}
}

StringID GetNewsStringBankrupcy(const NewsItem *ni)
{
	const Player *p = GetPlayer(GB(ni->string_id, 0, 4));

	switch (ni->string_id & 0xF0) {
	case NB_BTROUBLE:
		SetDParam(0, STR_7056_TRANSPORT_COMPANY_IN_TROUBLE);
		SetDParam(1, STR_7057_WILL_BE_SOLD_OFF_OR_DECLARED);
		SetDParam(2, p->name_1);
		SetDParam(3, p->name_2);
		return STR_02B6;
	case NB_BMERGER:
		SetDParam(0, STR_7059_TRANSPORT_COMPANY_MERGER);
		SetDParam(1, STR_705A_HAS_BEEN_SOLD_TO_FOR);
		COPY_IN_DPARAM(2,ni->params, 2);
		SetDParam(4, p->name_1);
		SetDParam(5, p->name_2);
		COPY_IN_DPARAM(6,ni->params + 2, 1);
		return STR_02B6;
	case NB_BBANKRUPT:
		SetDParam(0, STR_705C_BANKRUPT);
		SetDParam(1, STR_705D_HAS_BEEN_CLOSED_DOWN_BY);
		COPY_IN_DPARAM(2,ni->params, 2);
		return STR_02B6;
	case NB_BNEWCOMPANY:
		SetDParam(0, STR_705E_NEW_TRANSPORT_COMPANY_LAUNCHED);
		SetDParam(1, STR_705F_STARTS_CONSTRUCTION_NEAR);
		SetDParam(2, p->name_1);
		SetDParam(3, p->name_2);
		COPY_IN_DPARAM(4,ni->params, 2);
		return STR_02B6;
	default:
		NOT_REACHED();
	}

	/* useless, but avoids compiler warning this way */
	return 0;
}

static void PlayersGenStatistics(void)
{
	Station *st;
	Player *p;

	FOR_ALL_STATIONS(st) {
		_current_player = st->owner;
		SET_EXPENSES_TYPE(EXPENSES_PROPERTY);
		SubtractMoneyFromPlayer(_price.station_value >> 1);
	}

	if (!HASBIT(1<<0|1<<3|1<<6|1<<9, _cur_month))
		return;

	FOR_ALL_PLAYERS(p) {
		if (p->is_active) {
			memmove(&p->old_economy[1], &p->old_economy[0], sizeof(p->old_economy) - sizeof(p->old_economy[0]));
			p->old_economy[0] = p->cur_economy;
			memset(&p->cur_economy, 0, sizeof(p->cur_economy));

			if (p->num_valid_stat_ent != 24) p->num_valid_stat_ent++;

			UpdateCompanyRatingAndValue(p, true);
			PlayersCheckBankrupt(p);

			if (p->block_preview != 0) p->block_preview--;
		}
	}

	InvalidateWindow(WC_INCOME_GRAPH, 0);
	InvalidateWindow(WC_OPERATING_PROFIT, 0);
	InvalidateWindow(WC_DELIVERED_CARGO, 0);
	InvalidateWindow(WC_PERFORMANCE_HISTORY, 0);
	InvalidateWindow(WC_COMPANY_VALUE, 0);
	InvalidateWindow(WC_COMPANY_LEAGUE, 0);
}

static void AddSingleInflation(int32 *value, uint16 *frac, int32 amt)
{
	int64 tmp;
	int32 low;
	tmp = BIGMULS(*value, amt);
	*frac = (uint16)(low = (uint16)tmp + *frac);
	*value += (int32)(tmp >> 16) + (low >> 16);
}

static void AddInflation(void)
{
	int i;
	int32 inf = _economy.infl_amount * 54;

	if ((_cur_year - _patches.starting_year) >= (ORIGINAL_MAX_YEAR - ORIGINAL_BASE_YEAR)) return;

	for (i = 0; i != NUM_PRICES; i++) {
		AddSingleInflation((int32*)&_price + i, _price_frac + i, inf);
	}

	_economy.max_loan_unround += BIGMULUS(_economy.max_loan_unround, inf, 16);

	if (_economy.max_loan + 50000 <= _economy.max_loan_unround)
		_economy.max_loan += 50000;

	inf = _economy.infl_amount_pr * 54;
	for (i = 0; i != NUM_CARGO; i++) {
		AddSingleInflation(
			(int32*)_cargo_payment_rates + i,
			_cargo_payment_rates_frac + i,
			inf
		);
	}

	InvalidateWindowClasses(WC_BUILD_VEHICLE);
	InvalidateWindowClasses(WC_REPLACE_VEHICLE);
	InvalidateWindowClasses(WC_VEHICLE_DETAILS);
	InvalidateWindow(WC_PAYMENT_RATES, 0);
}

static void PlayersPayInterest(void)
{
	const Player* p;
	int interest = _economy.interest_rate * 54;

	FOR_ALL_PLAYERS(p) {
		if (!p->is_active) continue;

		_current_player = p->index;
		SET_EXPENSES_TYPE(EXPENSES_LOAN_INT);

		SubtractMoneyFromPlayer(BIGMULUS(p->current_loan, interest, 16));

		SET_EXPENSES_TYPE(EXPENSES_OTHER);
		SubtractMoneyFromPlayer(_price.station_value >> 2);
	}
}

static void HandleEconomyFluctuations(void)
{
	if (_opt.diff.economy == 0) return;

	if (--_economy.fluct == 0) {
		_economy.fluct = -(int)GB(Random(), 0, 2);
		AddNewsItem(STR_7073_WORLD_RECESSION_FINANCIAL, NEWS_FLAGS(NM_NORMAL,0,NT_ECONOMY,0), 0, 0);
	} else if (_economy.fluct == -12) {
		_economy.fluct = GB(Random(), 0, 8) + 312;
		AddNewsItem(STR_7074_RECESSION_OVER_UPTURN_IN, NEWS_FLAGS(NM_NORMAL,0,NT_ECONOMY,0), 0, 0);
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

static const int32 _price_base[NUM_PRICES] = {
	    100, // station_value
	    100, // build_rail
	     95, // build_road
	     65, // build_signals
	    275, // build_bridge
	    600, // build_train_depot
	    500, // build_road_depot
	    700, // build_ship_depot
	    450, // build_tunnel
	    200, // train_station_track
	    180, // train_station_length
	    600, // build_airport
	    200, // build_bus_station
	    200, // build_truck_station
	    350, // build_dock
	 400000, // build_railvehicle
	   2000, // build_railwagon
	 700000, // aircraft_base
	  14000, // roadveh_base
	  65000, // ship_base
	     20, // build_trees
	    250, // terraform
	     20, // clear_1
	     40, // purchase_land
	    200, // clear_2
	    500, // clear_3
	     20, // remove_trees
	    -70, // remove_rail
	     10, // remove_signals
	     50, // clear_bridge
	     80, // remove_train_depot
	     80, // remove_road_depot
	     90, // remove_ship_depot
	     30, // clear_tunnel
	  10000, // clear_water
	     50, // remove_rail_station
	     30, // remove_airport
	     50, // remove_bus_station
	     50, // remove_truck_station
	     55, // remove_dock
	   1600, // remove_house
	     40, // remove_road
	   5600, // running_rail[0] railroad
	   5200, // running_rail[1] monorail
	   4800, // running_rail[2] maglev
	   9600, // aircraft_running
	   1600, // roadveh_running
	   5600, // ship_running
	1000000, // build_industry
};

static byte price_base_multiplier[NUM_PRICES];

/**
 * Reset changes to the price base multipliers.
 */
void ResetPriceBaseMultipliers(void)
{
	uint i;

	// 8 means no multiplier.
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

void StartupEconomy(void)
{
	int i;

	assert(sizeof(_price) == NUM_PRICES * sizeof(int32));

	for (i = 0; i != NUM_PRICES; i++) {
		int32 price = _price_base[i];
		if (_price_category[i] != 0) {
			uint mod = _price_category[i] == 1 ? _opt.diff.vehicle_costs : _opt.diff.construction_cost;
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
		((int32*)&_price)[i] = price;
		_price_frac[i] = 0;
	}

	_economy.interest_rate = _opt.diff.initial_interest;
	_economy.infl_amount = _opt.diff.initial_interest;
	_economy.infl_amount_pr = max(0, _opt.diff.initial_interest - 1);
	_economy.max_loan_unround = _economy.max_loan = _opt.diff.max_loan * 1000;
	_economy.fluct = GB(Random(), 0, 8) + 168;
}

Pair SetupSubsidyDecodeParam(const Subsidy* s, bool mode)
{
	TileIndex tile;
	TileIndex tile2;
	Pair tp;

	/* if mode is false, use the singular form */
	SetDParam(0, _cargoc.names_s[s->cargo_type] + (mode ? 0 : 32));

	if (s->age < 12) {
		if (s->cargo_type != CT_PASSENGERS && s->cargo_type != CT_MAIL) {
			SetDParam(1, STR_INDUSTRY);
			SetDParam(2, s->from);
			tile = GetIndustry(s->from)->xy;

			if (s->cargo_type != CT_GOODS && s->cargo_type != CT_FOOD) {
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
		if (s->cargo_type != CT_INVALID && s->age < 12 &&
				(((s->cargo_type == CT_PASSENGERS || s->cargo_type == CT_MAIL) && (index == s->from || index == s->to)) ||
				((s->cargo_type == CT_GOODS || s->cargo_type == CT_FOOD) && index == s->to))) {
			s->cargo_type = CT_INVALID;
		}
	}
}

void DeleteSubsidyWithIndustry(IndustryID index)
{
	Subsidy *s;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type != CT_INVALID && s->age < 12 &&
				s->cargo_type != CT_PASSENGERS && s->cargo_type != CT_MAIL &&
				(index == s->from || (s->cargo_type != CT_GOODS && s->cargo_type != CT_FOOD && index == s->to))) {
			s->cargo_type = CT_INVALID;
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

typedef struct FoundRoute {
	uint distance;
	CargoID cargo;
	void *from;
	void *to;
} FoundRoute;

static void FindSubsidyPassengerRoute(FoundRoute *fr)
{
	Town *from,*to;

	fr->distance = (uint)-1;

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

	fr->distance = (uint)-1;

	fr->from = i = GetRandomIndustry();
	if (i == NULL) return;

	// Randomize cargo type
	if (Random()&1 && i->produced_cargo[1] != CT_INVALID) {
		cargo = i->produced_cargo[1];
		trans = i->pct_transported[1];
		total = i->total_production[1];
	} else {
		cargo = i->produced_cargo[0];
		trans = i->pct_transported[0];
		total = i->total_production[0];
	}

	// Quit if no production in this industry
	//  or if the cargo type is passengers
	//  or if the pct transported is already large enough
	if (total == 0 || trans > 42 || cargo == CT_INVALID || cargo == CT_PASSENGERS)
		return;

	fr->cargo = cargo;

	if (cargo == CT_GOODS || cargo == CT_FOOD) {
		// The destination is a town
		Town *t = GetRandomTown();

		// Only want big towns
		if (t == NULL || t->population < 900) return;

		fr->distance = DistanceManhattan(i->xy, t->xy);
		fr->to = t;
	} else {
		// The destination is an industry
		Industry *i2 = GetRandomIndustry();

		// The industry must accept the cargo
		if (i == i2 || i == NULL ||
				(cargo != i2->accepts_cargo[0] &&
				cargo != i2->accepts_cargo[1] &&
				cargo != i2->accepts_cargo[2]))
			return;
		fr->distance = DistanceManhattan(i->xy, i2->xy);
		fr->to = i2;
	}
}

static bool CheckSubsidyDuplicate(Subsidy *s)
{
	const Subsidy* ss;

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


static void SubsidyMonthlyHandler(void)
{
	Subsidy *s;
	Pair pair;
	Station *st;
	uint n;
	FoundRoute fr;
	bool modified = false;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type == CT_INVALID) continue;

		if (s->age == 12-1) {
			pair = SetupSubsidyDecodeParam(s, 1);
			AddNewsItem(STR_202E_OFFER_OF_SUBSIDY_EXPIRED, NEWS_FLAGS(NM_NORMAL, NF_TILE, NT_SUBSIDIES, 0), pair.a, pair.b);
			s->cargo_type = CT_INVALID;
			modified = true;
		} else if (s->age == 2*12-1) {
			st = GetStation(s->to);
			if (st->owner == _local_player) {
				pair = SetupSubsidyDecodeParam(s, 1);
				AddNewsItem(STR_202F_SUBSIDY_WITHDRAWN_SERVICE, NEWS_FLAGS(NM_NORMAL, NF_TILE, NT_SUBSIDIES, 0), pair.a, pair.b);
			}
			s->cargo_type = CT_INVALID;
			modified = true;
		} else {
			s->age++;
		}
	}

	// 25% chance to go on
	if (CHANCE16(1,4)) {
		// Find a free slot
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
				s->to = (fr.cargo == CT_GOODS || fr.cargo == CT_FOOD) ? ((Town*)fr.to)->index : ((Industry*)fr.to)->index;
	add_subsidy:
				if (!CheckSubsidyDuplicate(s)) {
					s->age = 0;
					pair = SetupSubsidyDecodeParam(s, 0);
					AddNewsItem(STR_2030_SERVICE_SUBSIDY_OFFERED, NEWS_FLAGS(NM_NORMAL, NF_TILE, NT_SUBSIDIES, 0), pair.a, pair.b);
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

static const SaveLoad _subsidies_desc[] = {
	    SLE_VAR(Subsidy, cargo_type, SLE_UINT8),
	    SLE_VAR(Subsidy, age,        SLE_UINT8),
	SLE_CONDVAR(Subsidy, from,       SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVAR(Subsidy, from,       SLE_UINT16,                5, SL_MAX_VERSION),
	SLE_CONDVAR(Subsidy, to,         SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVAR(Subsidy, to,         SLE_UINT16,                5, SL_MAX_VERSION),
	SLE_END()
};

static void Save_SUBS(void)
{
	int i;
	Subsidy *s;

	for (i = 0; i != lengthof(_subsidies); i++) {
		s = &_subsidies[i];
		if (s->cargo_type != CT_INVALID) {
			SlSetArrayIndex(i);
			SlObject(s, _subsidies_desc);
		}
	}
}

static void Load_SUBS(void)
{
	int index;
	while ((index = SlIterateArray()) != -1)
		SlObject(&_subsidies[index], _subsidies_desc);
}

int32 GetTransportedGoodsIncome(uint num_pieces, uint dist, byte transit_days, CargoID cargo_type)
{
	CargoID cargo = cargo_type;
	byte f;

	/* zero the distance if it's the bank and very short transport. */
	if (_opt.landscape == LT_NORMAL && cargo == CT_VALUABLES && dist < 10)
		dist = 0;

	f = 255;
	if (transit_days > _cargoc.transit_days_1[cargo]) {
		transit_days -= _cargoc.transit_days_1[cargo];
		f -= transit_days;

		if (transit_days > _cargoc.transit_days_2[cargo]) {
			transit_days -= _cargoc.transit_days_2[cargo];

			if (f < transit_days) {
				f = 0;
			} else {
				f -= transit_days;
			}
		}
	}
	if (f < 31) f = 31;

	return BIGMULSS(dist * f * num_pieces, _cargo_payment_rates[cargo], 21);
}

static void DeliverGoodsToIndustry(TileIndex xy, CargoID cargo_type, int num_pieces)
{
	Industry* best = NULL;
	Industry* ind;
	uint u;

	// Check if there's an industry close to the station that accepts the cargo
	// XXX - Think of something better to
	//       1) Only deliver to industries which are withing the catchment radius
	//       2) Distribute between industries if more then one is present
	u = (_patches.station_spread + 8) * 2;
	FOR_ALL_INDUSTRIES(ind) {
		uint t;

		if (( cargo_type == ind->accepts_cargo[0] ||
					cargo_type == ind->accepts_cargo[1] ||
					cargo_type == ind->accepts_cargo[2]
				) &&
				ind->produced_cargo[0] != CT_INVALID &&
				ind->produced_cargo[0] != cargo_type &&
				(t = DistanceManhattan(ind->xy, xy)) < u) {
			u = t;
			best = ind;
		}
	}

	/* Found one? */
	if (best != NULL) {
		best->was_cargo_delivered = true;
		best->cargo_waiting[0] = min(best->cargo_waiting[0] + num_pieces, 0xFFFF);
	}
}

static bool CheckSubsidised(Station *from, Station *to, CargoID cargo_type)
{
	Subsidy *s;
	TileIndex xy;
	Pair pair;
	Player *p;

	// check if there is an already existing subsidy that applies to us
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
			if (cargo_type == CT_PASSENGERS || cargo_type == CT_MAIL) {
				xy = GetTown(s->from)->xy;
			} else {
				xy = (GetIndustry(s->from))->xy;
			}
			if (DistanceMax(xy, from->xy) > 9) continue;

			/* Check distance from dest */
			switch (cargo_type) {
				case CT_PASSENGERS:
				case CT_MAIL:
				case CT_GOODS:
				case CT_FOOD:
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
			InjectDParam(2);

			p = GetPlayer(_current_player);
			SetDParam(0, p->name_1);
			SetDParam(1, p->name_2);
			AddNewsItem(
				STR_2031_SERVICE_SUBSIDY_AWARDED + _opt.diff.subsidy_multiplier,
				NEWS_FLAGS(NM_NORMAL, NF_TILE, NT_SUBSIDIES, 0),
				pair.a, pair.b
			);

			InvalidateWindow(WC_SUBSIDIES_LIST, 0);
			return true;
		}
	}
	return false;
}

static int32 DeliverGoods(int num_pieces, CargoID cargo_type, StationID source, StationID dest, byte days_in_transit)
{
	bool subsidised;
	Station *s_from, *s_to;
	int32 profit;

	assert(num_pieces > 0);

	// Update player statistics
	{
		Player *p = GetPlayer(_current_player);
		p->cur_economy.delivered_cargo += num_pieces;
		SETBIT(p->cargo_types, cargo_type);
	}

	// Get station pointers.
	s_from = GetStation(source);
	s_to = GetStation(dest);

	// Check if a subsidy applies.
	subsidised = CheckSubsidised(s_from, s_to, cargo_type);

	// Increase town's counter for some special goods types
	if (cargo_type == CT_FOOD) s_to->town->new_act_food += num_pieces;
	if (cargo_type == CT_WATER)  s_to->town->new_act_water += num_pieces;

	// Give the goods to the industry.
	DeliverGoodsToIndustry(s_to->xy, cargo_type, num_pieces);

	// Determine profit
	profit = GetTransportedGoodsIncome(num_pieces, DistanceManhattan(s_from->xy, s_to->xy), days_in_transit, cargo_type);

	// Modify profit if a subsidy is in effect
	if (subsidised) {
		switch (_opt.diff.subsidy_multiplier) {
			case 0:  profit += profit >> 1; break;
			case 1:  profit *= 2; break;
			case 2:  profit *= 3; break;
			default: profit *= 4; break;
		}
	}

	return profit;
}

/*
 * Returns true if Vehicle v should wait loading because other vehicle is
 * already loading the same cargo type
 * v = vehicle to load, u = GetFirstInChain(v)
 */
static bool LoadWait(const Vehicle* v, const Vehicle* u)
{
	const Vehicle *w;
	const Vehicle *x;
	bool has_any_cargo = false;

	if (!(u->current_order.flags & OF_FULL_LOAD)) return false;

	for (w = u; w != NULL; w = w->next) {
		if (w->cargo_count != 0) {
			if (v->cargo_type == w->cargo_type &&
					u->last_station_visited == w->cargo_source) {
				return false;
			}
			has_any_cargo = true;
		}
	}

	FOR_ALL_VEHICLES(x) {
		if ((x->type != VEH_Train || IsFrontEngine(x)) && // for all locs
				u->last_station_visited == x->last_station_visited && // at the same station
				!(x->vehstatus & (VS_STOPPED | VS_CRASHED)) && // not stopped or crashed
				x->current_order.type == OT_LOADING && // loading
				u != x) { // not itself
			bool other_has_any_cargo = false;
			bool has_space_for_same_type = false;
			bool other_has_same_type = false;

			for (w = x; w != NULL; w = w->next) {
				if (w->cargo_count < w->cargo_cap && v->cargo_type == w->cargo_type) {
					has_space_for_same_type = true;
				}

				if (w->cargo_count != 0) {
					if (v->cargo_type == w->cargo_type &&
							u->last_station_visited == w->cargo_source) {
						other_has_same_type = true;
					}
					other_has_any_cargo = true;
				}
			}

			if (has_space_for_same_type) {
				if (other_has_same_type) return true;
				if (other_has_any_cargo && !has_any_cargo) return true;
			}
		}
	}

	return false;
}

int LoadUnloadVehicle(Vehicle *v, bool just_arrived)
{
	int profit = 0;
	int v_profit = 0; //virtual profit for feeder systems
	int v_profit_total = 0;
	int unloading_time = 20;
	Vehicle *u = v;
	int result = 0;
	StationID last_visited;
	Station *st;
	int t;
	uint count, cap;
	PlayerID old_player;
	bool completely_empty = true;
	byte load_amount;
	bool anything_loaded = false;

	assert(v->current_order.type == OT_LOADING);

	v->cur_speed = 0;

	/* Loading can only have finished when all the cargo has been unloaded, and
	 * there is nothing left to load. It's easier to clear this if the
	 * conditions haven't been met than attempting to check them all before
	 * enabling though. */
	SETBIT(v->load_status, LS_LOADING_FINISHED);

	old_player = _current_player;
	_current_player = v->owner;

	last_visited = v->last_station_visited;
	st = GetStation(last_visited);

	for (; v != NULL; v = v->next) {
		GoodsEntry* ge;
		load_amount = EngInfo(v->engine_type)->load_amount;
		if (_patches.gradual_loading) {
			uint16 cb_load_amount = GetVehicleCallback(CBID_VEHICLE_LOAD_AMOUNT, 0, 0, v->engine_type, v);
			if (cb_load_amount != CALLBACK_FAILED) load_amount = cb_load_amount & 0xFF;
		}

		if (v->cargo_cap == 0) continue;

		/* If the vehicle has just arrived, set it to unload. */
		if (just_arrived) SETBIT(v->load_status, LS_CARGO_UNLOADING);

		ge = &st->goods[v->cargo_type];
		count = GB(ge->waiting_acceptance, 0, 12);

		/* unload? */
		if (v->cargo_count != 0 && HASBIT(v->load_status, LS_CARGO_UNLOADING)) {
			uint16 amount_unloaded = _patches.gradual_loading ? min(v->cargo_count, load_amount) : v->cargo_count;

			CLRBIT(u->load_status, LS_LOADING_FINISHED);

			if (v->cargo_source != last_visited && ge->waiting_acceptance & 0x8000 && !(u->current_order.flags & OF_TRANSFER)) {
				// deliver goods to the station
				st->time_since_unload = 0;

				unloading_time += v->cargo_count; /* TTDBUG: bug in original TTD */
				if (just_arrived && !HASBIT(v->load_status, LS_CARGO_PAID_FOR)) {
					profit += DeliverGoods(v->cargo_count, v->cargo_type, v->cargo_source, last_visited, v->cargo_days);
					SETBIT(v->load_status, LS_CARGO_PAID_FOR);
				}
				result |= 1;
				v->cargo_count -= amount_unloaded;
				if (_patches.gradual_loading) continue;
			} else if (u->current_order.flags & (OF_UNLOAD | OF_TRANSFER)) {
				/* unload goods and let it wait at the station */
				st->time_since_unload = 0;
				if (just_arrived && (u->current_order.flags & OF_TRANSFER) && !HASBIT(v->load_status, LS_CARGO_PAID_FOR)) {
					v_profit = GetTransportedGoodsIncome(
						v->cargo_count,
						DistanceManhattan(GetStation(v->cargo_source)->xy, GetStation(last_visited)->xy),
						v->cargo_days,
						v->cargo_type) * 3 / 2;

					v_profit_total += v_profit;
					SETBIT(v->load_status, LS_CARGO_PAID_FOR);
				}

				unloading_time += v->cargo_count;
				t = GB(ge->waiting_acceptance, 0, 12);
				if (t == 0) {
					// No goods waiting at station
					ge->enroute_time = v->cargo_days;
					ge->enroute_from = v->cargo_source;
				} else {
					// Goods already waiting at station. Set counters to the worst value.
					if (v->cargo_days >= ge->enroute_time)
						ge->enroute_time = v->cargo_days;
					if (last_visited != ge->enroute_from)
						ge->enroute_from = v->cargo_source;
				}
				// Update amount of waiting cargo
				SB(ge->waiting_acceptance, 0, 12, min(amount_unloaded + t, 0xFFF));

				if (u->current_order.flags & OF_TRANSFER) {
					ge->feeder_profit += v_profit;
					u->profit_this_year += v_profit;
				}
				result |= 2;
				v->cargo_count -= amount_unloaded;
				if (_patches.gradual_loading) continue;
			}

			if (v->cargo_count != 0) completely_empty = false;
		}

		/* The vehicle must have been unloaded because it is either empty, or
		 * the UNLOADING bit is already clear in v->load_status. */
		CLRBIT(v->load_status, LS_CARGO_UNLOADING);
		CLRBIT(v->load_status, LS_CARGO_PAID_FOR);

		/* don't pick up goods that we unloaded */
		if (u->current_order.flags & OF_UNLOAD) continue;

		/* update stats */
		ge->days_since_pickup = 0;
		switch (u->type) {
			case VEH_Train: t = u->u.rail.cached_max_speed; break;
			case VEH_Road:  t = u->max_speed / 2;           break;
			default:        t = u->max_speed;               break;
		}

		// if last speed is 0, we treat that as if no vehicle has ever visited the station.
		ge->last_speed = min(t, 255);
		ge->last_age = _cur_year - u->build_year;

		// If there's goods waiting at the station, and the vehicle
		//  has capacity for it, load it on the vehicle.
		if (count != 0 &&
				(cap = v->cargo_cap - v->cargo_count) != 0) {
			int cargoshare;
			int feeder_profit_share;

			if (v->cargo_count == 0)
				TriggerVehicle(v, VEHICLE_TRIGGER_NEW_CARGO);

			/* Skip loading this vehicle if another train/vehicle is already handling
			 * the same cargo type at this station */
			if (_patches.improved_load && (u->current_order.flags & OF_FULL_LOAD) && LoadWait(v,u)) continue;

			/* TODO: Regarding this, when we do gradual loading, we
			 * should first unload all vehicles and then start
			 * loading them. Since this will cause
			 * VEHICLE_TRIGGER_EMPTY to be called at the time when
			 * the whole vehicle chain is really totally empty, the
			 * @completely_empty assignment can then be safely
			 * removed; that's how TTDPatch behaves too. --pasky */
			completely_empty = false;
			anything_loaded = true;

			if (cap > count) cap = count;
			if (_patches.gradual_loading) cap = min(cap, load_amount);
			if (cap < count) CLRBIT(u->load_status, LS_LOADING_FINISHED);
			cargoshare = cap * 10000 / ge->waiting_acceptance;
			feeder_profit_share = ge->feeder_profit * cargoshare / 10000;
			v->cargo_count += cap;
			ge->waiting_acceptance -= cap;
			u->profit_this_year -= feeder_profit_share;
			ge->feeder_profit -= feeder_profit_share;
			unloading_time += cap;
			st->time_since_load = 0;

			// And record the source of the cargo, and the days in travel.
			v->cargo_source = ge->enroute_from;
			v->cargo_days = ge->enroute_time;
			result |= 2;
			st->last_vehicle_type = v->type;
		}
	}

	v = u;

	if (_patches.gradual_loading) {
		/* The time it takes to load one 'slice' of cargo or passengers depends
		 * on the vehicle type - the values here are those found in TTDPatch */
		uint gradual_loading_wait_time[] = { 40, 20, 10, 20 };

		unloading_time = gradual_loading_wait_time[v->type - VEH_Train];
		if (HASBIT(v->load_status, LS_LOADING_FINISHED)) {
			if (anything_loaded) {
				unloading_time += 20;
			} else {
				unloading_time = 20;
			}
		}
	}

	if (v_profit_total > 0) {
		ShowFeederIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, v_profit_total);
	}

	if (v->type == VEH_Train) {
		// Each platform tile is worth 2 rail vehicles.
		int overhang = v->u.rail.cached_total_length - GetStationPlatforms(st, v->tile) * TILE_SIZE;
		if (overhang > 0) {
			unloading_time <<= 1;
			unloading_time += (overhang * unloading_time) / 8;
		}
	}

	v->load_unload_time_rem = unloading_time;

	if (completely_empty) {
		TriggerVehicle(v, VEHICLE_TRIGGER_EMPTY);
	}

	if (result != 0) {
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		MarkStationTilesDirty(st);

		if (result & 2) InvalidateWindow(WC_STATION_VIEW, last_visited);

		if (profit != 0) {
			v->profit_this_year += profit;
			SubtractMoneyFromPlayer(-profit);

			if (IsLocalPlayer() && !PlayVehicleSound(v, VSE_LOAD_UNLOAD)) {
				SndPlayVehicleFx(SND_14_CASHTILL, v);
			}

			ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, -profit);
		}
	}

	_current_player = old_player;
	return result;
}

void PlayersMonthlyLoop(void)
{
	PlayersGenStatistics();
	if (_patches.inflation && _cur_year < MAX_YEAR)
		AddInflation();
	PlayersPayInterest();
	// Reset the _current_player flag
	_current_player = OWNER_NONE;
	HandleEconomyFluctuations();
	SubsidyMonthlyHandler();
}

static void DoAcquireCompany(Player *p)
{
	Player *owner;
	int i,pi;
	int64 value;

	SetDParam(0, p->name_1);
	SetDParam(1, p->name_2);
	SetDParam(2, p->bankrupt_value);
	AddNewsItem( (StringID)(_current_player | NB_BMERGER), NEWS_FLAGS(NM_CALLBACK, 0, NT_COMPANY_INFO, DNC_BANKRUPCY),0,0);

	// original code does this a little bit differently
	pi = p->index;
	ChangeNetworkOwner(pi, _current_player);
	ChangeOwnershipOfPlayerItems(pi, _current_player);

	if (p->bankrupt_value == 0) {
		owner = GetPlayer(_current_player);
		owner->current_loan += p->current_loan;
	}

	value = CalculateCompanyValue(p) >> 2;
	for (i = 0; i != 4; i++) {
		if (p->share_owners[i] != PLAYER_SPECTATOR) {
			owner = GetPlayer(p->share_owners[i]);
			owner->money64 += value;
			owner->yearly_expenses[0][EXPENSES_OTHER] += value;
			UpdatePlayerMoney32(owner);
		}
	}

	p->is_active = false;

	DeletePlayerWindows(pi);
	RebuildVehicleLists(); //Updates the open windows to add the newly acquired vehicles to the lists
}

extern int GetAmountOwnedBy(Player *p, byte owner);

/** Acquire shares in an opposing company.
 * @param tile unused
 * @param p1 player to buy the shares from
 * @param p2 unused
 */
int32 CmdBuyShareInCompany(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p;
	int64 cost;

	/* Check if buying shares is allowed (protection against modified clients) */
	/* Cannot buy own shares */
	if (!IsValidPlayer((PlayerID)p1) || !_patches.allow_shares || _current_player == (PlayerID)p1) return CMD_ERROR;

	p = GetPlayer((PlayerID)p1);

	/* Cannot buy shares of non-existent nor bankrupted company */
	if (!p->is_active) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	/* Protect new companies from hostile takeovers */
	if (_cur_year - p->inaugurated_year < 6) return_cmd_error(STR_7080_PROTECTED);

	/* Those lines are here for network-protection (clients can be slow) */
	if (GetAmountOwnedBy(p, PLAYER_SPECTATOR) == 0) return 0;

	/* We can not buy out a real player (temporarily). TODO: well, enable it obviously */
	if (GetAmountOwnedBy(p, PLAYER_SPECTATOR) == 1 && !p->is_ai) return 0;

	cost = CalculateCompanyValue(p) >> 2;
	if (flags & DC_EXEC) {
		PlayerID* b = p->share_owners;
		int i;

		while (*b != PLAYER_SPECTATOR) b++; /* share owners is guaranteed to contain at least one PLAYER_SPECTATOR */
		*b = _current_player;

		for (i = 0; p->share_owners[i] == _current_player;) {
			if (++i == 4) {
				p->bankrupt_value = 0;
				DoAcquireCompany(p);
				break;
			}
		}
		InvalidateWindow(WC_COMPANY, p1);
	}
	return cost;
}

/** Sell shares in an opposing company.
 * @param tile unused
 * @param p1 player to sell the shares from
 * @param p2 unused
 */
int32 CmdSellShareInCompany(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p;
	int64 cost;

	/* Check if selling shares is allowed (protection against modified clients) */
	/* Cannot sell own shares */
	if (!IsValidPlayer((PlayerID)p1) || !_patches.allow_shares || _current_player == (PlayerID)p1) return CMD_ERROR;

	p = GetPlayer((PlayerID)p1);

	/* Cannot sell shares of non-existent nor bankrupted company */
	if (!p->is_active) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	/* Those lines are here for network-protection (clients can be slow) */
	if (GetAmountOwnedBy(p, _current_player) == 0) return 0;

	/* adjust it a little to make it less profitable to sell and buy */
	cost = CalculateCompanyValue(p) >> 2;
	cost = -(cost - (cost >> 7));

	if (flags & DC_EXEC) {
		PlayerID* b = p->share_owners;
		while (*b != _current_player) b++; /* share owners is guaranteed to contain player */
		*b = PLAYER_SPECTATOR;
		InvalidateWindow(WC_COMPANY, p1);
	}
	return cost;
}

/** Buy up another company.
 * When a competing company is gone bankrupt you get the chance to purchase
 * that company.
 * @todo currently this only works for AI players
 * @param tile unused
 * @param p1 player/company to buy up
 * @param p2 unused
 */
int32 CmdBuyCompany(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p;
	PlayerID pid = (PlayerID)p1;

	/* Disable takeovers in multiplayer games */
	if (!IsValidPlayer(pid) || _networking) return CMD_ERROR;

	/* Do not allow players to take over themselves */
	if (pid == _current_player) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);
	p = GetPlayer(pid);

	if (!p->is_ai) return CMD_ERROR;

	if (flags & DC_EXEC) {
		DoAcquireCompany(p);
	}
	return p->bankrupt_value;
}

// Prices
static void SaveLoad_PRIC(void)
{
	SlArray(&_price,      NUM_PRICES, SLE_INT32);
	SlArray(&_price_frac, NUM_PRICES, SLE_UINT16);
}

// Cargo payment rates
static void SaveLoad_CAPR(void)
{
	SlArray(&_cargo_payment_rates,      NUM_CARGO, SLE_INT32);
	SlArray(&_cargo_payment_rates_frac, NUM_CARGO, SLE_UINT16);
}

static const SaveLoad _economy_desc[] = {
	SLE_VAR(Economy, max_loan,         SLE_INT32),
	SLE_VAR(Economy, max_loan_unround, SLE_INT32),
	SLE_VAR(Economy, fluct,            SLE_FILE_I16 | SLE_VAR_I32),
	SLE_VAR(Economy, interest_rate,    SLE_UINT8),
	SLE_VAR(Economy, infl_amount,      SLE_UINT8),
	SLE_VAR(Economy, infl_amount_pr,   SLE_UINT8),
	SLE_END()
};

// Economy variables
static void SaveLoad_ECMY(void)
{
	SlObject(&_economy, _economy_desc);
}

const ChunkHandler _economy_chunk_handlers[] = {
	{ 'PRIC', SaveLoad_PRIC, SaveLoad_PRIC, CH_RIFF | CH_AUTO_LENGTH},
	{ 'CAPR', SaveLoad_CAPR, SaveLoad_CAPR, CH_RIFF | CH_AUTO_LENGTH},
	{ 'SUBS', Save_SUBS,     Load_SUBS,     CH_ARRAY},
	{ 'ECMY', SaveLoad_ECMY, SaveLoad_ECMY, CH_RIFF | CH_LAST},
};
