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
#include "subsidy_type.h"

#include "table/strings.h"

Subsidy _subsidies[MAX_COMPANIES];

Pair SetupSubsidyDecodeParam(const Subsidy *s, bool mode)
{
	NewsReferenceType reftype1 = NR_NONE;
	NewsReferenceType reftype2 = NR_NONE;

	/* if mode is false, use the singular form */
	const CargoSpec *cs = GetCargo(s->cargo_type);
	SetDParam(0, mode ? cs->name : cs->name_single);

	if (s->age < 12) {
		if (cs->town_effect != TE_PASSENGERS && cs->town_effect != TE_MAIL) {
			SetDParam(1, STR_INDUSTRY);
			SetDParam(2, s->from);
			reftype1 = NR_INDUSTRY;

			if (cs->town_effect != TE_GOODS && cs->town_effect != TE_FOOD) {
				SetDParam(4, STR_INDUSTRY);
				SetDParam(5, s->to);
				reftype2 = NR_INDUSTRY;
			} else {
				SetDParam(4, STR_TOWN);
				SetDParam(5, s->to);
				reftype2 = NR_TOWN;
			}
		} else {
			SetDParam(1, STR_TOWN);
			SetDParam(2, s->from);
			reftype1 = NR_TOWN;

			SetDParam(4, STR_TOWN);
			SetDParam(5, s->to);
			reftype2 = NR_TOWN;
		}
	} else {
		SetDParam(1, s->from);
		reftype1 = NR_STATION;

		SetDParam(2, s->to);
		reftype2 = NR_STATION;
	}

	Pair p;
	p.a = reftype1;
	p.b = reftype2;
	return p;
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


void SubsidyMonthlyLoop()
{
	Subsidy *s;
	Station *st;
	uint n;
	FoundRoute fr;
	bool modified = false;

	for (s = _subsidies; s != endof(_subsidies); s++) {
		if (s->cargo_type == CT_INVALID) continue;

		if (s->age == 12 - 1) {
			Pair reftype = SetupSubsidyDecodeParam(s, 1);
			AddNewsItem(STR_NEWS_OFFER_OF_SUBSIDY_EXPIRED, NS_SUBSIDIES, (NewsReferenceType)reftype.a, s->from, (NewsReferenceType)reftype.b, s->to);
			s->cargo_type = CT_INVALID;
			modified = true;
			AI::BroadcastNewEvent(new AIEventSubsidyOfferExpired(s - _subsidies));
		} else if (s->age == 2 * 12 - 1) {
			st = Station::Get(s->to);
			if (st->owner == _local_company) {
				Pair reftype = SetupSubsidyDecodeParam(s, 1);
				AddNewsItem(STR_NEWS_SUBSIDY_WITHDRAWN_SERVICE, NS_SUBSIDIES, (NewsReferenceType)reftype.a, s->from, (NewsReferenceType)reftype.b, s->to);
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
					Pair reftype = SetupSubsidyDecodeParam(s, 0);
					AddNewsItem(STR_NEWS_SERVICE_SUBSIDY_OFFERED, NS_SUBSIDIES, (NewsReferenceType)reftype.a, s->from, (NewsReferenceType)reftype.b, s->to);
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

bool CheckSubsidised(const Station *from, const Station *to, CargoID cargo_type)
{
	Subsidy *s;
	TileIndex xy;

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
				xy = Town::Get(s->from)->xy;
			} else {
				xy = Industry::Get(s->from)->xy;
			}
			if (DistanceMax(xy, from->xy) > 9) continue;

			/* Check distance from dest */
			switch (cs->town_effect) {
				case TE_PASSENGERS:
				case TE_MAIL:
				case TE_GOODS:
				case TE_FOOD:
					xy = Town::Get(s->to)->xy;
					break;

				default:
					xy = Industry::Get(s->to)->xy;
					break;
			}
			if (DistanceMax(xy, to->xy) > 9) continue;

			/* Found a subsidy, change the values to indicate that it's in use */
			s->age = 12;
			s->from = from->index;
			s->to = to->index;

			/* Add a news item */
			Pair reftype = SetupSubsidyDecodeParam(s, 0);
			InjectDParam(1);

			SetDParam(0, _current_company);
			AddNewsItem(
				STR_NEWS_SERVICE_SUBSIDY_AWARDED_HALF + _settings_game.difficulty.subsidy_multiplier,
				NS_SUBSIDIES,
				(NewsReferenceType)reftype.a, s->from, (NewsReferenceType)reftype.b, s->to
			);
			AI::BroadcastNewEvent(new AIEventSubsidyAwarded(s - _subsidies));

			InvalidateWindow(WC_SUBSIDIES_LIST, 0);
			return true;
		}
	}
	return false;
}
