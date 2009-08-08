/* $Id$ */

/** @file subsidy.cpp Handling of subsidies. */

#include "stdafx.h"
#include "company_func.h"
#include "industry.h"
#include "map_func.h"
#include "town.h"
#include "news_func.h"
#include "ai/ai.hpp"
#include "station_base.h"
#include "cargotype.h"
#include "strings_func.h"
#include "window_func.h"
#include "subsidy_base.h"
#include "subsidy_func.h"

#include "table/strings.h"

/* static */ Subsidy Subsidy::array[MAX_COMPANIES];

/**
 * Marks subsidy as awarded, creates news and AI event
 * @param from source station
 * @param to destination station
 * @param company awarded company
 */
void Subsidy::AwardTo(StationID from, StationID to, CompanyID company)
{
	assert(!this->IsAwarded());

	this->age = 12;
	this->src_type = this->dst_type = ST_STATION;
	this->src = from;
	this->dst = to;

	/* Add a news item */
	Pair reftype = SetupSubsidyDecodeParam(this, 0);
	InjectDParam(1);

	char *company_name = MallocT<char>(MAX_LENGTH_COMPANY_NAME_BYTES);
	SetDParam(0, company);
	GetString(company_name, STR_COMPANY_NAME, company_name + MAX_LENGTH_COMPANY_NAME_BYTES - 1);

	SetDParamStr(0, company_name);
	AddNewsItem(
		STR_NEWS_SERVICE_SUBSIDY_AWARDED_HALF + _settings_game.difficulty.subsidy_multiplier,
		NS_SUBSIDIES,
		(NewsReferenceType)reftype.a, this->src, (NewsReferenceType)reftype.b, this->dst,
		company_name
	);
	AI::BroadcastNewEvent(new AIEventSubsidyAwarded(this->Index()));

	InvalidateWindow(WC_SUBSIDIES_LIST, 0);
}

/**
 * Allocates one subsidy
 * @return pointer to first invalid subsidy, NULL if there is none
 */
/* static */ Subsidy *Subsidy::AllocateItem()
{
	for (Subsidy *s = Subsidy::array; s < endof(Subsidy::array); s++) {
		if (!s->IsValid()) return s;
	}

	return NULL;
}

/**
 * Resets the array of subsidies marking all invalid
 */
/* static */ void Subsidy::Clean()
{
	memset(Subsidy::array, 0, sizeof(Subsidy::array));
	for (Subsidy *s = Subsidy::array; s < endof(Subsidy::array); s++) {
		s->cargo_type = CT_INVALID;
	}
}

/**
 * Initializes subsidies, files don't have to include subsidy_base,h this way
 */
void InitializeSubsidies()
{
	Subsidy::Clean();
}

Pair SetupSubsidyDecodeParam(const Subsidy *s, bool mode)
{
	NewsReferenceType reftype1 = NR_NONE;
	NewsReferenceType reftype2 = NR_NONE;

	/* if mode is false, use the singular form */
	const CargoSpec *cs = CargoSpec::Get(s->cargo_type);
	SetDParam(0, mode ? cs->name : cs->name_single);

	switch (s->src_type) {
		case ST_INDUSTRY:
			reftype1 = NR_INDUSTRY;
			SetDParam(1, STR_INDUSTRY_NAME);
			break;
		case ST_TOWN:
			reftype1 = NR_TOWN;
			SetDParam(1, STR_TOWN_NAME);
			break;
		case ST_STATION:
			reftype1 = NR_STATION;
			SetDParam(1, s->src);
			break;
		default: NOT_REACHED();
	}
	SetDParam(2, s->src);

	switch (s->dst_type) {
		case ST_INDUSTRY:
			reftype2 = NR_INDUSTRY;
			SetDParam(4, STR_INDUSTRY_NAME);
			break;
		case ST_TOWN:
			reftype2 = NR_TOWN;
			SetDParam(4, STR_TOWN_NAME);
			break;
		case ST_STATION:
			reftype2 = NR_STATION;
			SetDParam(2, s->dst);
			break;
		default: NOT_REACHED();
	}
	SetDParam(5, s->dst);

	Pair p;
	p.a = reftype1;
	p.b = reftype2;
	return p;
}

void DeleteSubsidyWith(SourceType type, SourceID index)
{
	bool dirty = false;

	Subsidy *s;
	FOR_ALL_SUBSIDIES(s) {
		if ((s->src_type == type && s->src == index) || (s->dst_type == type && s->dst == index)) {
			s->cargo_type = CT_INVALID;
			dirty = true;
		}
	}

	if (dirty) InvalidateWindow(WC_SUBSIDIES_LIST, 0);
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

	fr->from = from = Town::GetRandom();
	if (from == NULL || from->population < 400) return;

	fr->to = to = Town::GetRandom();
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

	fr->from = i = Industry::GetRandom();
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

	const CargoSpec *cs = CargoSpec::Get(cargo);
	if (cs->town_effect == TE_PASSENGERS) return;

	fr->cargo = cargo;

	if (cs->town_effect == TE_GOODS || cs->town_effect == TE_FOOD) {
		/*  The destination is a town */
		Town *t = Town::GetRandom();

		/* Only want big towns */
		if (t == NULL || t->population < 900) return;

		fr->distance = DistanceManhattan(i->xy, t->xy);
		fr->to = t;
	} else {
		/* The destination is an industry */
		Industry *i2 = Industry::GetRandom();

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
	FOR_ALL_SUBSIDIES(ss) {
		if (s != ss && ss->cargo_type == s->cargo_type &&
				ss->src_type == s->src_type && ss->src == s->src &&
				ss->dst_type == s->dst_type && ss->dst == s->dst) {
			s->cargo_type = CT_INVALID;
			return true;
		}
	}
	return false;
}


void SubsidyMonthlyLoop()
{
	bool modified = false;

	Subsidy *s;
	FOR_ALL_SUBSIDIES(s) {
		if (s->age == 12 - 1) {
			Pair reftype = SetupSubsidyDecodeParam(s, 1);
			AddNewsItem(STR_NEWS_OFFER_OF_SUBSIDY_EXPIRED, NS_SUBSIDIES, (NewsReferenceType)reftype.a, s->src, (NewsReferenceType)reftype.b, s->dst);
			s->cargo_type = CT_INVALID;
			modified = true;
			AI::BroadcastNewEvent(new AIEventSubsidyOfferExpired(s->Index()));
		} else if (s->age == 2 * 12 - 1) {
			Station *st = Station::Get(s->dst);
			if (st->owner == _local_company) {
				Pair reftype = SetupSubsidyDecodeParam(s, 1);
				AddNewsItem(STR_NEWS_SUBSIDY_WITHDRAWN_SERVICE, NS_SUBSIDIES, (NewsReferenceType)reftype.a, s->src, (NewsReferenceType)reftype.b, s->dst);
			}
			s->cargo_type = CT_INVALID;
			modified = true;
			AI::BroadcastNewEvent(new AIEventSubsidyExpired(s->Index()));
		} else {
			s->age++;
		}
	}

	/* 25% chance to go on */
	if (Chance16(1, 4)) {
		/*  Find a free slot*/
		s = Subsidy::AllocateItem();
		if (s == NULL) goto no_add;

		uint n = 1000;
		do {
			FoundRoute fr;
			FindSubsidyPassengerRoute(&fr);
			if (fr.distance <= 70) {
				s->cargo_type = CT_PASSENGERS;
				s->src_type = s->dst_type = ST_TOWN;
				s->src = ((Town *)fr.from)->index;
				s->dst = ((Town *)fr.to)->index;
				goto add_subsidy;
			}
			FindSubsidyCargoRoute(&fr);
			if (fr.distance <= 70) {
				s->cargo_type = fr.cargo;
				s->src_type = ST_INDUSTRY;
				s->src = ((Industry *)fr.from)->index;
				{
					const CargoSpec *cs = CargoSpec::Get(fr.cargo);
					if (cs->town_effect == TE_GOODS || cs->town_effect == TE_FOOD) {
						s->dst_type = ST_TOWN;
						s->dst = ((Town *)fr.to)->index;
					} else {
						s->dst_type = ST_INDUSTRY;
						s->dst = ((Industry *)fr.to)->index;
					}
				}
	add_subsidy:
				if (!CheckSubsidyDuplicate(s)) {
					s->age = 0;
					Pair reftype = SetupSubsidyDecodeParam(s, 0);
					AddNewsItem(STR_NEWS_SERVICE_SUBSIDY_OFFERED, NS_SUBSIDIES, (NewsReferenceType)reftype.a, s->src, (NewsReferenceType)reftype.b, s->dst);
					AI::BroadcastNewEvent(new AIEventSubsidyOffer(s->Index()));
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

bool CheckSubsidised(const Station *from, const Station *to, CargoID cargo_type, CompanyID company)
{
	Subsidy *s;
	TileIndex xy;

	/* check if there is an already existing subsidy that applies to us */
	FOR_ALL_SUBSIDIES(s) {
		if (s->cargo_type == cargo_type &&
				s->IsAwarded() &&
				s->src == from->index &&
				s->dst == to->index) {
			return true;
		}
	}

	/* check if there's a new subsidy that applies.. */
	FOR_ALL_SUBSIDIES(s) {
		if (s->cargo_type == cargo_type && !s->IsAwarded()) {
			/* Check distance from source */
			const CargoSpec *cs = CargoSpec::Get(cargo_type);
			if (cs->town_effect == TE_PASSENGERS || cs->town_effect == TE_MAIL) {
				xy = Town::Get(s->src)->xy;
			} else {
				xy = Industry::Get(s->src)->xy;
			}
			if (DistanceMax(xy, from->xy) > 9) continue;

			/* Check distance from dest */
			switch (cs->town_effect) {
				case TE_PASSENGERS:
				case TE_MAIL:
				case TE_GOODS:
				case TE_FOOD:
					xy = Town::Get(s->dst)->xy;
					break;

				default:
					xy = Industry::Get(s->dst)->xy;
					break;
			}
			if (DistanceMax(xy, to->xy) > 9) continue;

			s->AwardTo(from->index, to->index, company);
			return true;
		}
	}
	return false;
}
