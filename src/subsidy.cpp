/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file subsidy.cpp Handling of subsidies. */

#include "stdafx.h"
#include "company_func.h"
#include "industry.h"
#include "town.h"
#include "news_func.h"
#include "ai/ai.hpp"
#include "station_base.h"
#include "cargotype.h"
#include "strings_func.h"
#include "window_func.h"
#include "subsidy_base.h"
#include "subsidy_func.h"
#include "core/pool_func.hpp"
#include "core/random_func.hpp"

#include "table/strings.h"

SubsidyPool _subsidy_pool("Subsidy");
INSTANTIATE_POOL_METHODS(Subsidy)

/**
 * Marks subsidy as awarded, creates news and AI event
 * @param company awarded company
 */
void Subsidy::AwardTo(CompanyID company)
{
	assert(!this->IsAwarded());

	this->awarded = company;
	this->remaining = SUBSIDY_CONTRACT_MONTHS;

	char company_name[MAX_LENGTH_COMPANY_NAME_CHARS * MAX_CHAR_LENGTH];
	SetDParam(0, company);
	GetString(company_name, STR_COMPANY_NAME, lastof(company_name));

	char *cn = strdup(company_name);

	/* Add a news item */
	Pair reftype = SetupSubsidyDecodeParam(this, false);
	InjectDParam(1);

	SetDParamStr(0, cn);
	AddNewsItem(
		STR_NEWS_SERVICE_SUBSIDY_AWARDED_HALF + _settings_game.difficulty.subsidy_multiplier,
		NS_SUBSIDIES,
		(NewsReferenceType)reftype.a, this->src, (NewsReferenceType)reftype.b, this->dst,
		cn
	);
	AI::BroadcastNewEvent(new AIEventSubsidyAwarded(this->index));

	InvalidateWindowData(WC_SUBSIDIES_LIST, 0);
}

/**
 * Initializes subsidies, files don't have to include subsidy_base,h this way
 */
void InitializeSubsidies()
{
	_subsidy_pool.CleanPool();
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
		default: NOT_REACHED();
	}
	SetDParam(5, s->dst);

	Pair p;
	p.a = reftype1;
	p.b = reftype2;
	return p;
}

/**
 * Sets a flag indicating that given town/industry is part of subsidised route.
 * @param type is it a town or an industry?
 * @param index index of town/industry
 * @param flag flag to set
 */
static inline void SetPartOfSubsidyFlag(SourceType type, SourceID index, PartOfSubsidy flag)
{
	switch (type) {
		case ST_INDUSTRY: Industry::Get(index)->part_of_subsidy |= flag; return;
		case ST_TOWN:         Town::Get(index)->part_of_subsidy |= flag; return;
		default: NOT_REACHED();
	}
}

void RebuildSubsidisedSourceAndDestinationCache()
{
	Town *t;
	FOR_ALL_TOWNS(t) t->part_of_subsidy = POS_NONE;

	Industry *i;
	FOR_ALL_INDUSTRIES(i) i->part_of_subsidy = POS_NONE;

	const Subsidy *s;
	FOR_ALL_SUBSIDIES(s) {
		SetPartOfSubsidyFlag(s->src_type, s->src, POS_SRC);
		SetPartOfSubsidyFlag(s->dst_type, s->dst, POS_DST);
	}
}

void DeleteSubsidyWith(SourceType type, SourceID index)
{
	bool dirty = false;

	Subsidy *s;
	FOR_ALL_SUBSIDIES(s) {
		if ((s->src_type == type && s->src == index) || (s->dst_type == type && s->dst == index)) {
			delete s;
			dirty = true;
		}
	}

	if (dirty) {
		InvalidateWindowData(WC_SUBSIDIES_LIST, 0);
		RebuildSubsidisedSourceAndDestinationCache();
	}
}

static bool CheckSubsidyDuplicate(CargoID cargo, SourceType src_type, SourceID src, SourceType dst_type, SourceID dst)
{
	const Subsidy *s;
	FOR_ALL_SUBSIDIES(s) {
		if (s->cargo_type == cargo &&
				s->src_type == src_type && s->src == src &&
				s->dst_type == dst_type && s->dst == dst) {
			return true;
		}
	}
	return false;
}

static Subsidy *FindSubsidyPassengerRoute()
{
	assert(Subsidy::CanAllocateItem());

	const Town *src = Town::GetRandom();
	if (src->population < SUBSIDY_PAX_MIN_POPULATION ||
			src->pct_pass_transported > SUBSIDY_MAX_PCT_TRANSPORTED) {
		return NULL;
	}

	const Town *dst = Town::GetRandom();
	if (dst->population < SUBSIDY_PAX_MIN_POPULATION || src == dst) {
		return NULL;
	}

	if (DistanceManhattan(src->xy, dst->xy) > SUBSIDY_MAX_DISTANCE) return NULL;
	if (CheckSubsidyDuplicate(CT_PASSENGERS, ST_TOWN, src->index, ST_TOWN, dst->index)) return NULL;

	Subsidy *s = new Subsidy();
	s->cargo_type = CT_PASSENGERS;
	s->src_type = s->dst_type = ST_TOWN;
	s->src = src->index;
	s->dst = dst->index;

	return s;
}

static Subsidy *FindSubsidyCargoRoute()
{
	assert(Subsidy::CanAllocateItem());

	const Industry *i = Industry::GetRandom();
	if (i == NULL) return NULL;

	CargoID cargo;
	uint trans, total;

	/* Randomize cargo type */
	if (i->produced_cargo[1] != CT_INVALID && HasBit(Random(), 0)) {
		cargo = i->produced_cargo[1];
		trans = i->last_month_pct_transported[1];
		total = i->last_month_production[1];
	} else {
		cargo = i->produced_cargo[0];
		trans = i->last_month_pct_transported[0];
		total = i->last_month_production[0];
	}

	/* Quit if no production in this industry
	 * or if the pct transported is already large enough */
	if (total == 0 || trans > SUBSIDY_MAX_PCT_TRANSPORTED || cargo == CT_INVALID) return NULL;

	/* Don't allow passengers subsidies from industry */
	const CargoSpec *cs = CargoSpec::Get(cargo);
	if (cs->town_effect == TE_PASSENGERS) return NULL;

	SourceType dst_type;
	SourceID dst;

	if (cs->town_effect == TE_GOODS || cs->town_effect == TE_FOOD) {
		/*  The destination is a town */
		dst_type = ST_TOWN;
		const Town *t = Town::GetRandom();

		/* Only want big towns */
		if (t->population < SUBSIDY_CARGO_MIN_POPULATION) return NULL;

		if (DistanceManhattan(i->location.tile, t->xy) > SUBSIDY_MAX_DISTANCE) return NULL;

		dst = t->index;
	} else {
		/* The destination is an industry */
		dst_type = ST_INDUSTRY;
		const Industry *i2 = Industry::GetRandom();

		/* The industry must accept the cargo */
		if (i2 == NULL || i == i2 ||
				(cargo != i2->accepts_cargo[0] &&
				 cargo != i2->accepts_cargo[1] &&
				 cargo != i2->accepts_cargo[2])) {
			return NULL;
		}

		if (DistanceManhattan(i->location.tile, i2->location.tile) > SUBSIDY_MAX_DISTANCE) return NULL;

		dst = i2->index;
	}

	if (CheckSubsidyDuplicate(cargo, ST_INDUSTRY, i->index, dst_type, dst)) return NULL;

	Subsidy *s = new Subsidy();
	s->cargo_type = cargo;
	s->src_type = ST_INDUSTRY;
	s->src = i->index;
	s->dst_type = dst_type;
	s->dst = dst;

	return s;
}

void SubsidyMonthlyLoop()
{
	bool modified = false;

	Subsidy *s;
	FOR_ALL_SUBSIDIES(s) {
		if (--s->remaining == 0) {
			if (!s->IsAwarded()) {
				Pair reftype = SetupSubsidyDecodeParam(s, true);
				AddNewsItem(STR_NEWS_OFFER_OF_SUBSIDY_EXPIRED, NS_SUBSIDIES, (NewsReferenceType)reftype.a, s->src, (NewsReferenceType)reftype.b, s->dst);
				AI::BroadcastNewEvent(new AIEventSubsidyOfferExpired(s->index));
			} else {
				if (s->awarded == _local_company) {
					Pair reftype = SetupSubsidyDecodeParam(s, true);
					AddNewsItem(STR_NEWS_SUBSIDY_WITHDRAWN_SERVICE, NS_SUBSIDIES, (NewsReferenceType)reftype.a, s->src, (NewsReferenceType)reftype.b, s->dst);
				}
				AI::BroadcastNewEvent(new AIEventSubsidyExpired(s->index));
			}
			delete s;
			modified = true;
		}
	}

	if (modified) RebuildSubsidisedSourceAndDestinationCache();

	/* 25% chance to go on */
	if (Subsidy::CanAllocateItem() && Chance16(1, 4)) {
		uint n = 1000;
		do {
			Subsidy *s = FindSubsidyPassengerRoute();
			if (s == NULL) s = FindSubsidyCargoRoute();
			if (s != NULL) {
				s->remaining = SUBSIDY_OFFER_MONTHS;
				s->awarded = INVALID_COMPANY;
				Pair reftype = SetupSubsidyDecodeParam(s, false);
				AddNewsItem(STR_NEWS_SERVICE_SUBSIDY_OFFERED, NS_SUBSIDIES, (NewsReferenceType)reftype.a, s->src, (NewsReferenceType)reftype.b, s->dst);
				SetPartOfSubsidyFlag(s->src_type, s->src, POS_SRC);
				SetPartOfSubsidyFlag(s->dst_type, s->dst, POS_DST);
				AI::BroadcastNewEvent(new AIEventSubsidyOffer(s->index));
				modified = true;
				break;
			}
		} while (n--);
	}

	if (modified) InvalidateWindowData(WC_SUBSIDIES_LIST, 0);
}

/**
 * Tests whether given delivery is subsidised and possibly awards the subsidy to delivering company
 * @param cargo_type type of cargo
 * @param company company delivering the cargo
 * @param src_type type of #src
 * @param src index of source
 * @param st station where the cargo is delivered to
 * @return is the delivery subsidised?
 */
bool CheckSubsidised(CargoID cargo_type, CompanyID company, SourceType src_type, SourceID src, const Station *st)
{
	/* If the source isn't subsidised, don't continue */
	if (src == INVALID_SOURCE) return false;
	switch (src_type) {
		case ST_INDUSTRY:
			if (!(Industry::Get(src)->part_of_subsidy & POS_SRC)) return false;
			break;
		case ST_TOWN:
			if (!(    Town::Get(src)->part_of_subsidy & POS_SRC)) return false;
			break;
		default: return false;
	}

	/* Remember all towns near this station (at least one house in its catchment radius)
	 * which are destination of subsidised path. Do that only if needed */
	SmallVector<const Town *, 2> towns_near;
	if (!st->rect.IsEmpty()) {
		Subsidy *s;
		FOR_ALL_SUBSIDIES(s) {
			/* Don't create the cache if there is no applicable subsidy with town as destination */
			if (s->dst_type != ST_TOWN) continue;
			if (s->cargo_type != cargo_type || s->src_type != src_type || s->src != src) continue;
			if (s->IsAwarded() && s->awarded != company) continue;

			Rect rect = st->GetCatchmentRect();

			for (int y = rect.top; y <= rect.bottom; y++) {
				for (int x = rect.left; x <= rect.right; x++) {
					TileIndex tile = TileXY(x, y);
					if (!IsTileType(tile, MP_HOUSE)) continue;
					const Town *t = Town::GetByTile(tile);
					if (t->part_of_subsidy & POS_DST) towns_near.Include(t);
				}
			}
			break;
		}
	}

	bool subsidised = false;

	/* Check if there's a (new) subsidy that applies. There can be more subsidies triggered by this delivery!
	 * Think about the case that subsidies are A->B and A->C and station has both B and C in its catchment area */
	Subsidy *s;
	FOR_ALL_SUBSIDIES(s) {
		if (s->cargo_type == cargo_type && s->src_type == src_type && s->src == src && (!s->IsAwarded() || s->awarded == company)) {
			switch (s->dst_type) {
				case ST_INDUSTRY:
					for (const Industry * const *ip = st->industries_near.Begin(); ip != st->industries_near.End(); ip++) {
						if (s->dst == (*ip)->index) {
							assert((*ip)->part_of_subsidy & POS_DST);
							subsidised = true;
							if (!s->IsAwarded()) s->AwardTo(company);
						}
					}
					break;
				case ST_TOWN:
					for (const Town * const *tp = towns_near.Begin(); tp != towns_near.End(); tp++) {
						if (s->dst == (*tp)->index) {
							assert((*tp)->part_of_subsidy & POS_DST);
							subsidised = true;
							if (!s->IsAwarded()) s->AwardTo(company);
						}
					}
					break;
				default:
					NOT_REACHED();
			}
		}
	}

	return subsidised;
}
