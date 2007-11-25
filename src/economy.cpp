/* $Id$ */

/** @file economy.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "functions.h"
#include "landscape.h"
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
#include "network/network.h"
#include "sound.h"
#include "engine.h"
#include "network/network_data.h"
#include "variables.h"
#include "vehicle_gui.h"
#include "ai/ai.h"
#include "train.h"
#include "roadveh.h"
#include "aircraft.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "newgrf_callbacks.h"
#include "newgrf_industries.h"
#include "newgrf_industrytiles.h"
#include "unmovable.h"
#include "date.h"
#include "cargotype.h"
#include "player_face.h"
#include "group.h"

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

int _score_part[MAX_PLAYERS][SCORE_END];

Money CalculateCompanyValue(const Player* p)
{
	PlayerID owner = p->index;
	/* Do a little nasty by using CommandCost, so we can use the "overflow" protection of CommandCost */
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
	value -= p->current_loan;
	value += p->player_money;

	return max(value, (Money)1);
}

/** if update is set to true, the economy is updated with this score
 *  (also the house is updated, should only be true in the on-tick event)
 * @param update the economy with calculated score
 * @param p player been evaluated
 * @return actual score of this player
 * */
int UpdateCompanyRatingAndValue(Player *p, bool update)
{
	byte owner = p->index;
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
			if (IsPlayerBuildableVehicleType(v->type) && v->IsPrimaryVehicle()) {
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
			_score_part[owner][SCORE_MIN_PROFIT] = ClampToI32(min_profit);
	}

/* Count stations */
	{
		uint num = 0;
		const Station* st;

		FOR_ALL_STATIONS(st) {
			if (st->owner == owner) num += CountBits(st->facilities);
		}
		_score_part[owner][SCORE_STATIONS] = num;
	}

/* Generate statistics depending on recent income statistics */
	{
		int numec = min(p->num_valid_stat_ent, 12);
		if (numec != 0) {
			const PlayerEconomyEntry *pee = p->old_economy;
			Money min_income = pee->income + pee->expenses;
			Money max_income = pee->income + pee->expenses;

			do {
				min_income = min(min_income, pee->income + pee->expenses);
				max_income = max(max_income, pee->income + pee->expenses);
			} while (++pee,--numec);

			if (min_income > 0)
				_score_part[owner][SCORE_MIN_INCOME] = ClampToI32(min_income);

			_score_part[owner][SCORE_MAX_INCOME] = ClampToI32(max_income);
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
		uint num = CountBits(p->cargo_types);
		_score_part[owner][SCORE_CARGO] = num;
		if (update) p->cargo_types = 0;
	}

/* Generate score for player money */
	{
		if (p->player_money > 0) {
			_score_part[owner][SCORE_MONEY] = ClampToI32(p->player_money);
		}
	}

/* Generate score for loan */
	{
		_score_part[owner][SCORE_LOAN] = ClampToI32(_score_info[SCORE_LOAN].needed - p->current_loan);
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
		p->old_economy[0].performance_history = score;
		UpdateCompanyHQ(p, score);
		p->old_economy[0].company_value = CalculateCompanyValue(p);
	}

	InvalidateWindow(WC_PERFORMANCE_DETAIL, 0);
	return score;
}

/*  use PLAYER_SPECTATOR as new_player to delete the player. */
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
					CommandCost res = DoCommand(0, p->index, 0, DC_EXEC, CMD_SELL_SHARE_IN_COMPANY);
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
				CommandCost res = DoCommand(0, old_player, 0, DC_EXEC, CMD_SELL_SHARE_IN_COMPANY);
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
		GetPlayer(old_player)->player_money = MAX_UVALUE(uint64) >> 2; // jackpot ;p
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
			if (HasBit(t->have_ratings, old_player)) {
				if (HasBit(t->have_ratings, new_player)) {
					// use max of the two ratings.
					t->ratings[new_player] = max(t->ratings[new_player], t->ratings[old_player]);
				} else {
					SetBit(t->have_ratings, new_player);
					t->ratings[new_player] = t->ratings[old_player];
				}
			}
		}

		/* Reset the ratings for the old player */
		t->ratings[old_player] = 500;
		ClrBit(t->have_ratings, old_player);
	}

	{
		int num_train = 0;
		int num_road = 0;
		int num_ship = 0;
		int num_aircraft = 0;
		Vehicle *v;

		/*  Determine Ids for the new vehicles */
		FOR_ALL_VEHICLES(v) {
			if (v->owner == new_player) {
				switch (v->type) {
					case VEH_TRAIN:    if (IsFrontEngine(v)) num_train++; break;
					case VEH_ROAD:     if (IsRoadVehFront(v)) num_road++; break;
					case VEH_SHIP:     num_ship++; break;
					case VEH_AIRCRAFT: if (IsNormalAircraft(v)) num_aircraft++; break;
					default: break;
				}
			}
		}

		FOR_ALL_VEHICLES(v) {
			if (v->owner == old_player && IsInsideMM(v->type, VEH_TRAIN, VEH_AIRCRAFT + 1)) {
				if (new_player == PLAYER_SPECTATOR) {
					DeleteWindowById(WC_VEHICLE_VIEW, v->index);
					DeleteWindowById(WC_VEHICLE_DETAILS, v->index);
					DeleteWindowById(WC_VEHICLE_ORDERS, v->index);

					if (v->IsPrimaryVehicle() || (v->type == VEH_TRAIN && IsFreeWagon(v))) {
						switch (v->type) {
							default: NOT_REACHED();

							case VEH_TRAIN: {
								Vehicle *u = v;
								do {
									Vehicle *next = GetNextVehicle(u);
									delete u;
									u = next;
								} while (u != NULL);
							} break;

							case VEH_ROAD:
							case VEH_SHIP:
								delete v;
								break;

							case VEH_AIRCRAFT:
								DeleteVehicleChain(v);
								break;
						}
					}
				} else {
					v->owner = new_player;
					v->group_id = DEFAULT_GROUP;
					if (IsEngineCountable(v)) GetPlayer(new_player)->num_engines[v->engine_type]++;
					switch (v->type) {
						case VEH_TRAIN:    if (IsFrontEngine(v)) v->unitnumber = ++num_train; break;
						case VEH_ROAD:     if (IsRoadVehFront(v)) v->unitnumber = ++num_road; break;
						case VEH_SHIP:     v->unitnumber = ++num_ship; break;
						case VEH_AIRCRAFT: if (IsNormalAircraft(v)) v->unitnumber = ++num_aircraft; break;
						default: NOT_REACHED();
					}
				}
			}
		}
	}

	/*  Change ownership of tiles */
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
	if (!_networking) return;

	if (current_player == _local_player) {
		_network_playas = new_player;
		SetLocalPlayer(new_player);
	}

	if (!_network_server) return;

	/* The server has to handle all administrative issues, for example
	* updating and notifying all clients of what has happened */
	NetworkTCPSocketHandler *cs;
	NetworkClientInfo *ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);

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

	/*  If the player has money again, it does not go bankrupt */
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

			/* Check if the company has any value.. if not, declare it bankrupt
			 *  right now */
			Money val = CalculateCompanyValue(p);
			if (val > 0) {
				p->bankrupt_value = val;
				p->bankrupt_asked = 1 << owner; // Don't ask the owner
				p->bankrupt_timeout = 0;
				break;
			}
			/* Else, falltrue to case 4... */
		}
		case 4: {
			/* Close everything the owner has open */
			DeletePlayerWindows(owner);

			/* Show bankrupt news */
			SetDParam(0, p->index);
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
	DrawNewsBorder(w);

	const NewsItem *ni = WP(w, news_d).ni;
	Player *p = GetPlayer((PlayerID)GB(ni->string_id, 0, 4));
	DrawPlayerFace(p->face, p->player_color, 2, 23);
	GfxFillRect(3, 23, 3 + 91, 23 + 118, PALETTE_TO_STRUCT_GREY | (1 << USE_COLORTABLE));

	SetDParam(0, p->index);

	DrawStringMultiCenter(49, 148, STR_7058_PRESIDENT, 94);

	switch (ni->string_id & 0xF0) {
	case NB_BTROUBLE:
		DrawStringCentered(w->width >> 1, 1, STR_7056_TRANSPORT_COMPANY_IN_TROUBLE, TC_FROMSTRING);

		SetDParam(0, p->index);

		DrawStringMultiCenter(
			((w->width - 101) >> 1) + 98,
			90,
			STR_7057_WILL_BE_SOLD_OFF_OR_DECLARED,
			w->width - 101);
		break;

	case NB_BMERGER:
		DrawStringCentered(w->width >> 1, 1, STR_7059_TRANSPORT_COMPANY_MERGER, TC_FROMSTRING);
		SetDParam(0, ni->params[0]);
		SetDParam(1, p->index);
		SetDParam(2, ni->params[1]);
		DrawStringMultiCenter(
			((w->width - 101) >> 1) + 98,
			90,
			ni->params[1] == 0 ? STR_707F_HAS_BEEN_TAKEN_OVER_BY : STR_705A_HAS_BEEN_SOLD_TO_FOR,
			w->width - 101);
		break;

	case NB_BBANKRUPT:
		DrawStringCentered(w->width >> 1, 1, STR_705C_BANKRUPT, TC_FROMSTRING);
		SetDParam(0, ni->params[0]);
		DrawStringMultiCenter(
			((w->width - 101) >> 1) + 98,
			90,
			STR_705D_HAS_BEEN_CLOSED_DOWN_BY,
			w->width - 101);
		break;

	case NB_BNEWCOMPANY:
		DrawStringCentered(w->width >> 1, 1, STR_705E_NEW_TRANSPORT_COMPANY_LAUNCHED, TC_FROMSTRING);
		SetDParam(0, p->index);
		SetDParam(1, ni->params[0]);
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
	const Player *p = GetPlayer((PlayerID)GB(ni->string_id, 0, 4));

	switch (ni->string_id & 0xF0) {
	case NB_BTROUBLE:
		SetDParam(0, STR_7056_TRANSPORT_COMPANY_IN_TROUBLE);
		SetDParam(1, STR_7057_WILL_BE_SOLD_OFF_OR_DECLARED);
		SetDParam(2, p->index);
		return STR_02B6;
	case NB_BMERGER:
		SetDParam(0, STR_7059_TRANSPORT_COMPANY_MERGER);
		SetDParam(1, ni->params[1] == 0 ? STR_707F_HAS_BEEN_TAKEN_OVER_BY : STR_705A_HAS_BEEN_SOLD_TO_FOR);
		SetDParam(2, ni->params[0]);
		SetDParam(3, p->index);
		SetDParam(4, ni->params[1]);
		return STR_02B6;
	case NB_BBANKRUPT:
		SetDParam(0, STR_705C_BANKRUPT);
		SetDParam(1, STR_705D_HAS_BEEN_CLOSED_DOWN_BY);
		SetDParam(2, ni->params[0]);
		return STR_02B6;
	case NB_BNEWCOMPANY:
		SetDParam(0, STR_705E_NEW_TRANSPORT_COMPANY_LAUNCHED);
		SetDParam(1, STR_705F_STARTS_CONSTRUCTION_NEAR);
		SetDParam(2, p->index);
		SetDParam(3, ni->params[0]);
		return STR_02B6;
	default:
		NOT_REACHED();
	}
}

static void PlayersGenStatistics()
{
	Station *st;
	Player *p;

	FOR_ALL_STATIONS(st) {
		_current_player = st->owner;
		SET_EXPENSES_TYPE(EXPENSES_PROPERTY);
		SubtractMoneyFromPlayer(_price.station_value >> 1);
	}

	if (!HasBit(1<<0|1<<3|1<<6|1<<9, _cur_month))
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

static void AddInflation()
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
	if ((_cur_year - _patches.starting_year) >= (ORIGINAL_MAX_YEAR - ORIGINAL_BASE_YEAR)) return;

	/* Approximation for (100 + infl_amount)% ** (1 / 12) - 100%
	 * scaled by 65536
	 * 12 -> months per year
	 * This is only a good approxiamtion for small values
	 */
	Money inf = _economy.infl_amount * 54;

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

static void PlayersPayInterest()
{
	const Player* p;
	int interest = _economy.interest_rate * 54;

	FOR_ALL_PLAYERS(p) {
		if (!p->is_active) continue;

		_current_player = p->index;
		SET_EXPENSES_TYPE(EXPENSES_LOAN_INT);

		SubtractMoneyFromPlayer(CommandCost((Money)BigMulSU(p->current_loan, interest, 16)));

		SET_EXPENSES_TYPE(EXPENSES_OTHER);
		SubtractMoneyFromPlayer(_price.station_value >> 2);
	}
}

static void HandleEconomyFluctuations()
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
	     20, ///< clear_1
	     40, ///< purchase_land
	    200, ///< clear_2
	    500, ///< clear_3
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
	   5600, ///< running_rail[0] railroad
	   5200, ///< running_rail[1] monorail
	   4800, ///< running_rail[2] maglev
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

void StartupEconomy()
{
	int i;

	assert(sizeof(_price) == NUM_PRICES * sizeof(Money));

	for (i = 0; i != NUM_PRICES; i++) {
		Money price = _price_base[i];
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
		((Money*)&_price)[i] = price;
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

	/* 25% chance to go on */
	if (Chance16(1,4)) {
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

static void Save_SUBS()
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

static void Load_SUBS()
{
	int index;
	while ((index = SlIterateArray()) != -1)
		SlObject(&_subsidies[index], _subsidies_desc);
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
	if (_opt.landscape == LT_TEMPERATE && cs->label == 'VALU' && dist < 10) return 0;


	static const int MIN_TIME_FACTOR = 31;
	static const int MAX_TIME_FACTOR = 255;

	const int days1 = cs->transit_days[0];
	const int days2 = cs->transit_days[1];
	const int days_over_days1 = transit_days - days1;

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
	int time_factor;
	if (days_over_days1 <= 0) {
		time_factor = MAX_TIME_FACTOR;
	} else if (days_over_days1 <= days2) {
		time_factor = MAX_TIME_FACTOR - days_over_days1;
	} else {
		time_factor = MAX_TIME_FACTOR - 2 * days_over_days1 + days2;
	}

	if (time_factor < MIN_TIME_FACTOR) time_factor = MIN_TIME_FACTOR;

	return BigMulS(dist * time_factor * num_pieces, _cargo_payment_rates[cargo_type], 21);
}

static void DeliverGoodsToIndustry(TileIndex xy, CargoID cargo_type, int num_pieces)
{
	Industry *best = NULL;
	Industry *ind;
	const IndustrySpec *indspec;
	uint best_dist;
	uint accepted_cargo_index = 0;  ///< unlikely value, just for warning removing

	/* Check if there's an industry close to the station that accepts the cargo
	 * XXX - Think of something better to
	 *       1) Only deliver to industries which are withing the catchment radius
	 *       2) Distribute between industries if more then one is present */
	best_dist = (_patches.station_spread + 8) * 2;
	FOR_ALL_INDUSTRIES(ind) {
		indspec = GetIndustrySpec(ind->type);
		uint i;

		for (i = 0; i < lengthof(ind->accepts_cargo); i++) {
			if (cargo_type == ind->accepts_cargo[i]) break;
		}

		/* Check if matching cargo has been found */
		if (i == lengthof(ind->accepts_cargo)) continue;

		if (HasBit(indspec->callback_flags, CBM_IND_REFUSE_CARGO)) {
			uint16 res = GetIndustryCallback(CBID_INDUSTRY_REFUSE_CARGO, 0, GetReverseCargoTranslation(cargo_type, indspec->grf_prop.grffile), ind, ind->type, ind->xy);
			if (res == 0) continue;
		}

		uint dist = DistanceManhattan(ind->xy, xy);

		if (dist < best_dist) {
			best = ind;
			best_dist = dist;
			accepted_cargo_index = i;
		}
	}

	/* Found one? */
	if (best != NULL) {
		indspec = GetIndustrySpec(best->type);
		uint16 callback = indspec->callback_flags;

		best->was_cargo_delivered = true;
		best->last_cargo_accepted_at = _date;

		if (HasBit(callback, CBM_IND_PRODUCTION_CARGO_ARRIVAL) || HasBit(callback, CBM_IND_PRODUCTION_256_TICKS)) {
			best->incoming_cargo_waiting[accepted_cargo_index] = min(num_pieces + best->incoming_cargo_waiting[accepted_cargo_index], 0xFFFF);
			if (HasBit(callback, CBM_IND_PRODUCTION_CARGO_ARRIVAL)) {
				IndustryProductionCallback(best, 0);
			} else {
				InvalidateWindow(WC_INDUSTRY_VIEW, best->index);
			}
		} else {
			best->produced_cargo_waiting[0] = min(best->produced_cargo_waiting[0] + (num_pieces * indspec->input_cargo_multiplier[accepted_cargo_index][0] / 256), 0xFFFF);
			best->produced_cargo_waiting[1] = min(best->produced_cargo_waiting[1] + (num_pieces * indspec->input_cargo_multiplier[accepted_cargo_index][1] / 256), 0xFFFF);
		}

		TriggerIndustry(best, INDUSTRY_TRIGGER_RECEIVED_CARGO);
		StartStopIndustryTileAnimation(best, IAT_INDUSTRY_RECEIVED_CARGO);
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
				xy = (GetIndustry(s->from))->xy;
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

			SetDParam(0, _current_player);
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

static Money DeliverGoods(int num_pieces, CargoID cargo_type, StationID source, StationID dest, TileIndex source_tile, byte days_in_transit)
{
	bool subsidised;
	Station *s_from, *s_to;
	Money profit;

	assert(num_pieces > 0);

	/* Update player statistics */
	{
		Player *p = GetPlayer(_current_player);
		p->cur_economy.delivered_cargo += num_pieces;
		SetBit(p->cargo_types, cargo_type);
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
	DeliverGoodsToIndustry(s_to->xy, cargo_type, num_pieces);

	/* Determine profit */
	profit = GetTransportedGoodsIncome(num_pieces, DistanceManhattan(source_tile, s_to->xy), days_in_transit, cargo_type);

	/* Modify profit if a subsidy is in effect */
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
	PlayerID old_player = _current_player;
	_current_player = front_v->owner;

	/* At this moment loading cannot be finished */
	ClrBit(front_v->vehicle_flags, VF_LOADING_FINISHED);

	/* Start unloading in at the first possible moment */
	front_v->load_unload_time_rem = 1;

	for (Vehicle *v = front_v; v != NULL; v = v->Next()) {
		/* No cargo to unload */
		if (v->cargo_cap == 0 || v->cargo.Empty()) continue;

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
					(front_v->current_order.flags & OF_TRANSFER) == 0) {
				/* Deliver goods to the station */
				st->time_since_unload = 0;

				/* handle end of route payment */
				Money profit = DeliverGoods(cp->count, v->cargo_type, cp->source, last_visited, cp->source_xy, cp->days_in_transit);
				cp->paid_for = true;
				route_profit   += profit; // display amount paid for final route delivery, A-D of a chain A-B-C-D
				vehicle_profit += profit - cp->feeder_share;                    // whole vehicle is not payed for transfers picked up earlier

				result |= 1;

				SetBit(v->vehicle_flags, VF_CARGO_UNLOADING);
			} else if (front_v->current_order.flags & (OF_UNLOAD | OF_TRANSFER)) {
				if (!cp->paid_for && (front_v->current_order.flags & OF_TRANSFER) != 0) {
					Money profit = GetTransportedGoodsIncome(
						cp->count,
						/* pay transfer vehicle for only the part of transfer it has done: ie. cargo_loaded_at_xy to here */
						DistanceManhattan(cp->loaded_at_xy, GetStation(last_visited)->xy),
						cp->days_in_transit,
						v->cargo_type);

					front_v->profit_this_year += profit;
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

	if (virtual_profit > 0) {
		ShowFeederIncomeAnimation(front_v->x_pos, front_v->y_pos, front_v->z_pos, virtual_profit);
	}

	if (route_profit != 0) {
		front_v->profit_this_year += vehicle_profit;
		SubtractMoneyFromPlayer(-route_profit);

		if (IsLocalPlayer() && !PlayVehicleSound(front_v, VSE_LOAD_UNLOAD)) {
			SndPlayVehicleFx(SND_14_CASHTILL, front_v);
		}

		ShowCostOrIncomeAnimation(front_v->x_pos, front_v->y_pos, front_v->z_pos, -vehicle_profit);
	}

	_current_player = old_player;
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
	assert(v->current_order.type == OT_LOADING);

	/* We have not waited enough time till the next round of loading/unloading */
	if (--v->load_unload_time_rem != 0) {
		if (_patches.improved_load && HasBit(v->current_order.flags, OFB_FULL_LOAD)) {
			/* 'Reserve' this cargo for this vehicle, because we were first. */
			for (; v != NULL; v = v->Next()) {
				if (v->cargo_cap != 0) cargo_left[v->cargo_type] -= v->cargo_cap - v->cargo.Count();
			}
		}
		return;
	}

	if (v->type == VEH_TRAIN && !IsTileType(v->tile, MP_STATION)) {
		/* The train reversed in the station. Take the "easy" way
		 * out and let the train just leave as it always did. */
		SetBit(v->vehicle_flags, VF_LOADING_FINISHED);
		return;
	}

	int unloading_time = 0;
	Vehicle *u = v;
	int result = 0;
	uint cap;

	bool completely_empty  = true;
	bool anything_unloaded = false;
	bool anything_loaded   = false;
	uint32 cargo_not_full  = 0;
	uint32 cargo_full      = 0;

	v->cur_speed = 0;

	StationID last_visited = v->last_station_visited;
	Station *st = GetStation(last_visited);

	for (; v != NULL; v = v->Next()) {
		if (v->cargo_cap == 0) continue;

		byte load_amount = EngInfo(v->engine_type)->load_amount;
		if (_patches.gradual_loading && HasBit(EngInfo(v->engine_type)->callbackmask, CBM_VEHICLE_LOAD_AMOUNT)) {
			uint16 cb_load_amount = GetVehicleCallback(CBID_VEHICLE_LOAD_AMOUNT, 0, 0, v->engine_type, v);
			if (cb_load_amount != CALLBACK_FAILED && cb_load_amount != 0) load_amount = cb_load_amount & 0xFF;
		}

		GoodsEntry *ge = &st->goods[v->cargo_type];

		if (HasBit(v->vehicle_flags, VF_CARGO_UNLOADING)) {
			uint cargo_count = v->cargo.Count();
			uint amount_unloaded = _patches.gradual_loading ? min(cargo_count, load_amount) : cargo_count;
			bool remaining; // Are there cargo entities in this vehicle that can still be unloaded here?

			if (HasBit(ge->acceptance_pickup, GoodsEntry::ACCEPTANCE) && !(u->current_order.flags & OF_TRANSFER)) {
				/* The cargo has reached it's final destination, the packets may now be destroyed */
				remaining = v->cargo.MoveTo(NULL, amount_unloaded, CargoList::MTA_FINAL_DELIVERY, last_visited);

				result |= 1;
			} else if (u->current_order.flags & (OF_UNLOAD | OF_TRANSFER)) {
				remaining = v->cargo.MoveTo(&ge->cargo, amount_unloaded);
				SetBit(ge->acceptance_pickup, GoodsEntry::PICKUP);

				result |= 2;
			} else {
				/* The order changed while unloading (unset unload/transfer) or the
				 * station does not accept goods anymore. */
				ClrBit(v->vehicle_flags, VF_CARGO_UNLOADING);
				continue;
			}

			/* Deliver goods to the station */
			st->time_since_unload = 0;

			unloading_time += amount_unloaded;

			anything_unloaded = true;
			if (_patches.gradual_loading && remaining) {
				completely_empty = false;
			} else {
				/* We have finished unloading (cargo count == 0) */
				ClrBit(v->vehicle_flags, VF_CARGO_UNLOADING);
			}

			continue;
		}

		/* Do not pick up goods that we unloaded */
		if (u->current_order.flags & OF_UNLOAD) continue;

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
		if (!ge->cargo.Empty() &&
				(cap = v->cargo_cap - v->cargo.Count()) != 0) {
			uint count = ge->cargo.Count();

			/* Skip loading this vehicle if another train/vehicle is already handling
			 * the same cargo type at this station */
			if (_patches.improved_load && cargo_left[v->cargo_type] <= 0) {
				SetBit(cargo_not_full, v->cargo_type);
				continue;
			}

			if (cap > count) cap = count;
			if (_patches.gradual_loading) cap = min(cap, load_amount);
			if (_patches.improved_load) {
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
			 * completely_empty assignment can then be safely
			 * removed; that's how TTDPatch behaves too. --pasky */
			completely_empty = false;
			anything_loaded = true;

			ge->cargo.MoveTo(&v->cargo, cap, CargoList::MTA_CARGO_LOAD, st->xy);

			st->time_since_load = 0;
			st->last_vehicle_type = v->type;

			unloading_time += cap;

			result |= 2;
		}

		if (v->cargo.Count() == v->cargo_cap) {
			SetBit(cargo_full, v->cargo_type);
		} else {
			SetBit(cargo_not_full, v->cargo_type);
		}
	}

	/* We update these variables here, so gradual loading still fills
	 * all wagons at the same time instead of using the same 'improved'
	 * loading algorithm for the wagons (only fill wagon when there is
	 * enough to fill the previous wagons) */
	if (_patches.improved_load && HasBit(u->current_order.flags, OFB_FULL_LOAD)) {
		/* Update left cargo */
		for (v = u; v != NULL; v = v->Next()) {
			if (v->cargo_cap != 0) cargo_left[v->cargo_type] -= v->cargo_cap - v->cargo.Count();
		}
	}

	v = u;

	if (anything_loaded || anything_unloaded) {
		if (_patches.gradual_loading) {
			/* The time it takes to load one 'slice' of cargo or passengers depends
			* on the vehicle type - the values here are those found in TTDPatch */
			const uint gradual_loading_wait_time[] = { 40, 20, 10, 20 };

			unloading_time = gradual_loading_wait_time[v->type];
		}
	} else {
		bool finished_loading = true;
		if (HasBit(v->current_order.flags, OFB_FULL_LOAD)) {
			if (_patches.full_load_any) {
				/* if the aircraft carries passengers and is NOT full, then
				 * continue loading, no matter how much mail is in */
				if ((v->type == VEH_AIRCRAFT && IsCargoInClass(v->cargo_type, CC_PASSENGERS) && v->cargo_cap != v->cargo.Count()) ||
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
	 * If _patches.loading_indicators == 2, show indicators (bool can be promoted to int as 0 or 1 - results in 2 > 0,1 )
	 * if _patches.loading_indicators == 1, _local_player must be the owner or must be a spectator to show ind., so 1 > 0
	 * if _patches.loading_indicators == 0, do not display indicators ... 0 is never greater than anything
	 */
	if (_game_mode != GM_MENU && (_patches.loading_indicators > (uint)(v->owner != _local_player && _local_player != PLAYER_SPECTATOR))) {
		StringID percent_up_down = STR_NULL;
		int percent = CalcPercentVehicleFilled(v, &percent_up_down);
		if (v->fill_percent_te_id == INVALID_TE_ID) {
			v->fill_percent_te_id = ShowFillingPercent(v->x_pos, v->y_pos, v->z_pos + 20, percent, percent_up_down);
		} else {
			UpdateFillingPercent(v->fill_percent_te_id, percent, percent_up_down);
		}
	}

	v->load_unload_time_rem = unloading_time;

	if (completely_empty) {
		TriggerVehicle(v, VEHICLE_TRIGGER_EMPTY);
	}

	if (result != 0) {
		InvalidateWindow(v->GetVehicleListWindowClass(), v->owner);
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

void PlayersMonthlyLoop()
{
	PlayersGenStatistics();
	if (_patches.inflation && _cur_year < MAX_YEAR)
		AddInflation();
	PlayersPayInterest();
	/* Reset the _current_player flag */
	_current_player = OWNER_NONE;
	HandleEconomyFluctuations();
	SubsidyMonthlyHandler();
}

static void DoAcquireCompany(Player *p)
{
	Player *owner;
	int i;
	Money value;

	SetDParam(0, p->index);
	SetDParam(1, p->bankrupt_value);
	AddNewsItem( (StringID)(_current_player | NB_BMERGER), NEWS_FLAGS(NM_CALLBACK, 0, NT_COMPANY_INFO, DNC_BANKRUPCY),0,0);

	/* original code does this a little bit differently */
	PlayerID pi = p->index;
	ChangeNetworkOwner(pi, _current_player);
	ChangeOwnershipOfPlayerItems(pi, _current_player);

	if (p->bankrupt_value == 0) {
		owner = GetPlayer(_current_player);
		owner->current_loan += p->current_loan;
	}

	value = CalculateCompanyValue(p) >> 2;
	PlayerID old_player = _current_player;
	for (i = 0; i != 4; i++) {
		if (p->share_owners[i] != PLAYER_SPECTATOR) {
			SET_EXPENSES_TYPE(EXPENSES_OTHER);
			_current_player = p->share_owners[i];
			SubtractMoneyFromPlayer(CommandCost(-value));
		}
	}
	_current_player = old_player;

	p->is_active = false;

	DeletePlayerWindows(pi);
	RebuildVehicleLists(); //Updates the open windows to add the newly acquired vehicles to the lists
}

extern int GetAmountOwnedBy(const Player *p, PlayerID owner);

/** Acquire shares in an opposing company.
 * @param tile unused
 * @param flags type of operation
 * @param p1 player to buy the shares from
 * @param p2 unused
 */
CommandCost CmdBuyShareInCompany(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p;
	CommandCost cost;

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
	if (GetAmountOwnedBy(p, PLAYER_SPECTATOR) == 0) return cost;

	/* We can not buy out a real player (temporarily). TODO: well, enable it obviously */
	if (GetAmountOwnedBy(p, PLAYER_SPECTATOR) == 1 && !p->is_ai) return cost;

	cost.AddCost(CalculateCompanyValue(p) >> 2);
	if (flags & DC_EXEC) {
		PlayerByte* b = p->share_owners;
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
 * @param flags type of operation
 * @param p1 player to sell the shares from
 * @param p2 unused
 */
CommandCost CmdSellShareInCompany(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Player *p;
	Money cost;

	/* Check if selling shares is allowed (protection against modified clients) */
	/* Cannot sell own shares */
	if (!IsValidPlayer((PlayerID)p1) || !_patches.allow_shares || _current_player == (PlayerID)p1) return CMD_ERROR;

	p = GetPlayer((PlayerID)p1);

	/* Cannot sell shares of non-existent nor bankrupted company */
	if (!p->is_active) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_OTHER);

	/* Those lines are here for network-protection (clients can be slow) */
	if (GetAmountOwnedBy(p, _current_player) == 0) return CommandCost();

	/* adjust it a little to make it less profitable to sell and buy */
	cost = CalculateCompanyValue(p) >> 2;
	cost = -(cost - (cost >> 7));

	if (flags & DC_EXEC) {
		PlayerByte* b = p->share_owners;
		while (*b != _current_player) b++; // share owners is guaranteed to contain player
		*b = PLAYER_SPECTATOR;
		InvalidateWindow(WC_COMPANY, p1);
	}
	return CommandCost(cost);
}

/** Buy up another company.
 * When a competing company is gone bankrupt you get the chance to purchase
 * that company.
 * @todo currently this only works for AI players
 * @param tile unused
 * @param flags type of operation
 * @param p1 player/company to buy up
 * @param p2 unused
 */
CommandCost CmdBuyCompany(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
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
	return CommandCost(p->bankrupt_value);
}

/** Prices */
static void SaveLoad_PRIC()
{
	int vt = CheckSavegameVersion(65) ? (SLE_FILE_I32 | SLE_VAR_I64) : SLE_INT64;
	SlArray(&_price,      NUM_PRICES, vt);
	SlArray(&_price_frac, NUM_PRICES, SLE_UINT16);
}

/** Cargo payment rates */
static void SaveLoad_CAPR()
{
	uint num_cargo = CheckSavegameVersion(55) ? 12 : NUM_CARGO;
	int vt = CheckSavegameVersion(65) ? (SLE_FILE_I32 | SLE_VAR_I64) : SLE_INT64;
	SlArray(&_cargo_payment_rates,      num_cargo, vt);
	SlArray(&_cargo_payment_rates_frac, num_cargo, SLE_UINT16);
}

static const SaveLoad _economy_desc[] = {
	SLE_CONDVAR(Economy, max_loan,         SLE_FILE_I32 | SLE_VAR_I64,  0, 64),
	SLE_CONDVAR(Economy, max_loan,         SLE_INT64,                  65, SL_MAX_VERSION),
	SLE_CONDVAR(Economy, max_loan_unround, SLE_FILE_I32 | SLE_VAR_I64,  0, 64),
	SLE_CONDVAR(Economy, max_loan_unround, SLE_INT64,                  65, SL_MAX_VERSION),
	SLE_CONDVAR(Economy, max_loan_unround_fract, SLE_UINT16,           70, SL_MAX_VERSION),
	    SLE_VAR(Economy, fluct,            SLE_FILE_I16 | SLE_VAR_I32),
	    SLE_VAR(Economy, interest_rate,    SLE_UINT8),
	    SLE_VAR(Economy, infl_amount,      SLE_UINT8),
	    SLE_VAR(Economy, infl_amount_pr,   SLE_UINT8),
	    SLE_END()
};

/** Economy variables */
static void SaveLoad_ECMY()
{
	SlObject(&_economy, _economy_desc);
}

extern const ChunkHandler _economy_chunk_handlers[] = {
	{ 'PRIC', SaveLoad_PRIC, SaveLoad_PRIC, CH_RIFF | CH_AUTO_LENGTH},
	{ 'CAPR', SaveLoad_CAPR, SaveLoad_CAPR, CH_RIFF | CH_AUTO_LENGTH},
	{ 'SUBS', Save_SUBS,     Load_SUBS,     CH_ARRAY},
	{ 'ECMY', SaveLoad_ECMY, SaveLoad_ECMY, CH_RIFF | CH_LAST},
};
