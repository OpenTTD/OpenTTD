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
#include "strings_func.h"
#include "window_func.h"
#include "subsidy_base.h"
#include "subsidy_func.h"
#include "core/pool_func.hpp"
#include "core/random_func.hpp"
#include "core/container_func.hpp"
#include "game/game.hpp"
#include "command_func.h"
#include "string_func.h"
#include "tile_cmd.h"
#include "subsidy_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"

#include "table/strings.h"

#include "safeguards.h"

SubsidyPool _subsidy_pool("Subsidy"); ///< Pool for the subsidies.
INSTANTIATE_POOL_METHODS(Subsidy)

/**
 * Marks subsidy as awarded, creates news and AI event
 * @param company awarded company
 */
void Subsidy::AwardTo(CompanyID company)
{
	assert(!this->IsAwarded());

	this->awarded = company;
	this->remaining = _settings_game.difficulty.subsidy_duration * CalendarTime::MONTHS_IN_YEAR;

	SetDParam(0, company);
	NewsStringData *company_name = new NewsStringData(GetString(STR_COMPANY_NAME));

	/* Add a news item */
	std::pair<NewsReferenceType, NewsReferenceType> reftype = SetupSubsidyDecodeParam(this, SubsidyDecodeParamType::NewsAwarded, 1);

	SetDParamStr(0, company_name->string);
	AddNewsItem(
		STR_NEWS_SERVICE_SUBSIDY_AWARDED_HALF + _settings_game.difficulty.subsidy_multiplier,
		NT_SUBSIDIES, NF_NORMAL,
		reftype.first, this->src, reftype.second, this->dst,
		company_name
	);
	AI::BroadcastNewEvent(new ScriptEventSubsidyAwarded(this->index));
	Game::NewEvent(new ScriptEventSubsidyAwarded(this->index));

	InvalidateWindowData(WC_SUBSIDIES_LIST, 0);
}

/**
 * Setup the string parameters for printing the subsidy at the screen, and compute the news reference for the subsidy.
 * @param s %Subsidy being printed.
 * @param mode Type of subsidy news message to decide on parameter format.
 * @param parameter_offset The location/index in the String DParams to start decoding the subsidy's parameters. Defaults to 0.
 * @return Reference of the subsidy in the news system.
 */
std::pair<NewsReferenceType, NewsReferenceType> SetupSubsidyDecodeParam(const Subsidy *s, SubsidyDecodeParamType mode, uint parameter_offset)
{
	NewsReferenceType reftype1 = NR_NONE;
	NewsReferenceType reftype2 = NR_NONE;

	/* Always use the plural form of the cargo name - trying to decide between plural or singular causes issues for translations */
	const CargoSpec *cs = CargoSpec::Get(s->cargo_type);
	SetDParam(parameter_offset, cs->name);

	switch (s->src_type) {
		case SourceType::Industry:
			reftype1 = NR_INDUSTRY;
			SetDParam(parameter_offset + 1, STR_INDUSTRY_NAME);
			break;
		case SourceType::Town:
			reftype1 = NR_TOWN;
			SetDParam(parameter_offset + 1, STR_TOWN_NAME);
			break;
		default: NOT_REACHED();
	}
	SetDParam(parameter_offset + 2, s->src);

	switch (s->dst_type) {
		case SourceType::Industry:
			reftype2 = NR_INDUSTRY;
			SetDParam(parameter_offset + 4, STR_INDUSTRY_NAME);
			break;
		case SourceType::Town:
			reftype2 = NR_TOWN;
			SetDParam(parameter_offset + 4, STR_TOWN_NAME);
			break;
		default: NOT_REACHED();
	}
	SetDParam(parameter_offset + 5, s->dst);

	/* If the subsidy is being offered or awarded, the news item mentions the subsidy duration. */
	if (mode == SubsidyDecodeParamType::NewsOffered || mode == SubsidyDecodeParamType::NewsAwarded) {
		SetDParam(parameter_offset + 7, _settings_game.difficulty.subsidy_duration);
	}

	return std::pair<NewsReferenceType, NewsReferenceType>(reftype1, reftype2);
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
		case SourceType::Industry: Industry::Get(index)->part_of_subsidy |= flag; return;
		case SourceType::Town:   Town::Get(index)->cache.part_of_subsidy |= flag; return;
		default: NOT_REACHED();
	}
}

/** Perform a full rebuild of the subsidies cache. */
void RebuildSubsidisedSourceAndDestinationCache()
{
	for (Town *t : Town::Iterate()) t->cache.part_of_subsidy = POS_NONE;

	for (Industry *i : Industry::Iterate()) i->part_of_subsidy = POS_NONE;

	for (const Subsidy *s : Subsidy::Iterate()) {
		SetPartOfSubsidyFlag(s->src_type, s->src, POS_SRC);
		SetPartOfSubsidyFlag(s->dst_type, s->dst, POS_DST);
	}
}

/**
 * Delete the subsidies associated with a given cargo source type and id.
 * @param type  Cargo source type of the id.
 * @param index Id to remove.
 */
void DeleteSubsidyWith(SourceType type, SourceID index)
{
	bool dirty = false;

	for (Subsidy *s : Subsidy::Iterate()) {
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

/**
 * Check whether a specific subsidy already exists.
 * @param cargo Cargo type.
 * @param src_type Type of source of the cargo, affects interpretation of \a src.
 * @param src Id of the source.
 * @param dst_type Type of the destination of the cargo, affects interpretation of \a dst.
 * @param dst Id of the destination.
 * @return \c true if the subsidy already exists, \c false if not.
 */
static bool CheckSubsidyDuplicate(CargoID cargo, SourceType src_type, SourceID src, SourceType dst_type, SourceID dst)
{
	for (const Subsidy *s : Subsidy::Iterate()) {
		if (s->cargo_type == cargo &&
				s->src_type == src_type && s->src == src &&
				s->dst_type == dst_type && s->dst == dst) {
			return true;
		}
	}
	return false;
}

/**
 * Checks if the source and destination of a subsidy are inside the distance limit.
 * @param src_type Type of \a src.
 * @param src      Index of source.
 * @param dst_type Type of \a dst.
 * @param dst      Index of destination.
 * @return True if they are inside the distance limit.
 */
static bool CheckSubsidyDistance(SourceType src_type, SourceID src, SourceType dst_type, SourceID dst)
{
	TileIndex tile_src = (src_type == SourceType::Town) ? Town::Get(src)->xy : Industry::Get(src)->location.tile;
	TileIndex tile_dst = (dst_type == SourceType::Town) ? Town::Get(dst)->xy : Industry::Get(dst)->location.tile;

	return (DistanceManhattan(tile_src, tile_dst) <= SUBSIDY_MAX_DISTANCE);
}

/**
 * Creates a subsidy with the given parameters.
 * @param cid      Subsidised cargo.
 * @param src_type Type of \a src.
 * @param src      Index of source.
 * @param dst_type Type of \a dst.
 * @param dst      Index of destination.
 */
void CreateSubsidy(CargoID cid, SourceType src_type, SourceID src, SourceType dst_type, SourceID dst)
{
	Subsidy *s = new Subsidy();
	s->cargo_type = cid;
	s->src_type = src_type;
	s->src = src;
	s->dst_type = dst_type;
	s->dst = dst;
	s->remaining = SUBSIDY_OFFER_MONTHS;
	s->awarded = INVALID_COMPANY;

	std::pair<NewsReferenceType, NewsReferenceType> reftype = SetupSubsidyDecodeParam(s, SubsidyDecodeParamType::NewsOffered);
	AddNewsItem(STR_NEWS_SERVICE_SUBSIDY_OFFERED, NT_SUBSIDIES, NF_NORMAL, reftype.first, s->src, reftype.second, s->dst);
	SetPartOfSubsidyFlag(s->src_type, s->src, POS_SRC);
	SetPartOfSubsidyFlag(s->dst_type, s->dst, POS_DST);
	AI::BroadcastNewEvent(new ScriptEventSubsidyOffer(s->index));
	Game::NewEvent(new ScriptEventSubsidyOffer(s->index));

	InvalidateWindowData(WC_SUBSIDIES_LIST, 0);
}

/**
 * Create a new subsidy.
 * @param flags type of operation
 * @param cid CargoID of subsidy.
 * @param src_type SourceType of source.
 * @param src SourceID of source.
 * @param dst_type SourceType of destination.
 * @param dst SourceID of destination.
 * @return the cost of this operation or an error
 */
CommandCost CmdCreateSubsidy(DoCommandFlag flags, CargoID cid, SourceType src_type, SourceID src, SourceType dst_type, SourceID dst)
{
	if (!Subsidy::CanAllocateItem()) return CMD_ERROR;

	if (_current_company != OWNER_DEITY) return CMD_ERROR;

	if (cid >= NUM_CARGO || !::CargoSpec::Get(cid)->IsValid()) return CMD_ERROR;

	switch (src_type) {
		case SourceType::Town:
			if (!Town::IsValidID(src)) return CMD_ERROR;
			break;
		case SourceType::Industry:
			if (!Industry::IsValidID(src)) return CMD_ERROR;
			break;
		default:
			return CMD_ERROR;
	}
	switch (dst_type) {
		case SourceType::Town:
			if (!Town::IsValidID(dst)) return CMD_ERROR;
			break;
		case SourceType::Industry:
			if (!Industry::IsValidID(dst)) return CMD_ERROR;
			break;
		default:
			return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		CreateSubsidy(cid, src_type, src, dst_type, dst);
	}

	return CommandCost();
}

/**
 * Tries to create a passenger subsidy between two towns.
 * @return True iff the subsidy was created.
 */
bool FindSubsidyPassengerRoute()
{
	if (!Subsidy::CanAllocateItem()) return false;

	const Town *src = Town::GetRandom();
	if (src->cache.population < SUBSIDY_PAX_MIN_POPULATION ||
			src->GetPercentTransported(CT_PASSENGERS) > SUBSIDY_MAX_PCT_TRANSPORTED) {
		return false;
	}

	const Town *dst = Town::GetRandom();
	if (dst->cache.population < SUBSIDY_PAX_MIN_POPULATION || src == dst) {
		return false;
	}

	if (DistanceManhattan(src->xy, dst->xy) > SUBSIDY_MAX_DISTANCE) return false;
	if (CheckSubsidyDuplicate(CT_PASSENGERS, SourceType::Town, src->index, SourceType::Town, dst->index)) return false;

	CreateSubsidy(CT_PASSENGERS, SourceType::Town, src->index, SourceType::Town, dst->index);

	return true;
}

bool FindSubsidyCargoDestination(CargoID cid, SourceType src_type, SourceID src);


/**
 * Tries to create a cargo subsidy with a town as source.
 * @return True iff the subsidy was created.
 */
bool FindSubsidyTownCargoRoute()
{
	if (!Subsidy::CanAllocateItem()) return false;

	SourceType src_type = SourceType::Town;

	/* Select a random town. */
	const Town *src_town = Town::GetRandom();
	if (src_town->cache.population < SUBSIDY_CARGO_MIN_POPULATION) return false;

	/* Calculate the produced cargo of houses around town center. */
	CargoArray town_cargo_produced{};
	TileArea ta = TileArea(src_town->xy, 1, 1).Expand(SUBSIDY_TOWN_CARGO_RADIUS);
	for (TileIndex tile : ta) {
		if (IsTileType(tile, MP_HOUSE)) {
			AddProducedCargo(tile, town_cargo_produced);
		}
	}

	/* Passenger subsidies are not handled here. */
	town_cargo_produced[CT_PASSENGERS] = 0;

	uint8_t cargo_count = town_cargo_produced.GetCount();

	/* No cargo produced at all? */
	if (cargo_count == 0) return false;

	/* Choose a random cargo that is produced in the town. */
	uint8_t cargo_number = RandomRange(cargo_count);
	CargoID cid;
	for (cid = 0; cid < NUM_CARGO; cid++) {
		if (town_cargo_produced[cid] > 0) {
			if (cargo_number == 0) break;
			cargo_number--;
		}
	}

	/* Avoid using invalid NewGRF cargoes. */
	if (!CargoSpec::Get(cid)->IsValid() ||
			_settings_game.linkgraph.GetDistributionType(cid) != DT_MANUAL) {
		return false;
	}

	/* Quit if the percentage transported is large enough. */
	if (src_town->GetPercentTransported(cid) > SUBSIDY_MAX_PCT_TRANSPORTED) return false;

	SourceID src = src_town->index;

	return FindSubsidyCargoDestination(cid, src_type, src);
}

/**
 * Tries to create a cargo subsidy with an industry as source.
 * @return True iff the subsidy was created.
 */
bool FindSubsidyIndustryCargoRoute()
{
	if (!Subsidy::CanAllocateItem()) return false;

	SourceType src_type = SourceType::Industry;

	/* Select a random industry. */
	const Industry *src_ind = Industry::GetRandom();
	if (src_ind == nullptr) return false;

	uint trans, total;

	CargoID cid;

	/* Randomize cargo type */
	int num_cargos = std::count_if(std::begin(src_ind->produced), std::end(src_ind->produced), [](const auto &p) { return IsValidCargoID(p.cargo); });
	if (num_cargos == 0) return false; // industry produces nothing
	int cargo_num = RandomRange(num_cargos) + 1;

	auto it = std::begin(src_ind->produced);
	for (/* nothing */; it != std::end(src_ind->produced); ++it) {
		if (IsValidCargoID(it->cargo)) cargo_num--;
		if (cargo_num == 0) break;
	}
	assert(it != std::end(src_ind->produced)); // indicates loop didn't end as intended

	cid = it->cargo;
	trans = it->history[LAST_MONTH].PctTransported();
	total = it->history[LAST_MONTH].production;

	/* Quit if no production in this industry
	 * or if the pct transported is already large enough
	 * or if the cargo is automatically distributed */
	if (total == 0 || trans > SUBSIDY_MAX_PCT_TRANSPORTED ||
			!IsValidCargoID(cid) ||
			_settings_game.linkgraph.GetDistributionType(cid) != DT_MANUAL) {
		return false;
	}

	SourceID src = src_ind->index;

	return FindSubsidyCargoDestination(cid, src_type, src);
}

/**
 * Tries to find a suitable destination for the given source and cargo.
 * @param cid      Subsidized cargo.
 * @param src_type Type of \a src.
 * @param src      Index of source.
 * @return True iff the subsidy was created.
 */
bool FindSubsidyCargoDestination(CargoID cid, SourceType src_type, SourceID src)
{
	/* Choose a random destination. */
	SourceType dst_type = Chance16(1, 2) ? SourceType::Town : SourceType::Industry;

	SourceID dst;
	switch (dst_type) {
		case SourceType::Town: {
			/* Select a random town. */
			const Town *dst_town = Town::GetRandom();

			/* Calculate cargo acceptance of houses around town center. */
			CargoArray town_cargo_accepted{};
			TileArea ta = TileArea(dst_town->xy, 1, 1).Expand(SUBSIDY_TOWN_CARGO_RADIUS);
			for (TileIndex tile : ta) {
				if (IsTileType(tile, MP_HOUSE)) {
					AddAcceptedCargo(tile, town_cargo_accepted, nullptr);
				}
			}

			/* Check if the town can accept this cargo. */
			if (town_cargo_accepted[cid] < 8) return false;

			dst = dst_town->index;
			break;
		}

		case SourceType::Industry: {
			/* Select a random industry. */
			const Industry *dst_ind = Industry::GetRandom();
			if (dst_ind == nullptr) return false;

			/* The industry must accept the cargo */
			if (!dst_ind->IsCargoAccepted(cid)) return false;

			dst = dst_ind->index;
			break;
		}

		default: NOT_REACHED();
	}

	/* Check that the source and the destination are not the same. */
	if (src_type == dst_type && src == dst) return false;

	/* Check distance between source and destination. */
	if (!CheckSubsidyDistance(src_type, src, dst_type, dst)) return false;

	/* Avoid duplicate subsidies. */
	if (CheckSubsidyDuplicate(cid, src_type, src, dst_type, dst)) return false;

	CreateSubsidy(cid, src_type, src, dst_type, dst);

	return true;
}

/** Perform the monthly update of open subsidies, and try to create a new one. */
static IntervalTimer<TimerGameCalendar> _subsidies_monthly({TimerGameCalendar::MONTH, TimerGameCalendar::Priority::SUBSIDY}, [](auto)
{
	bool modified = false;

	for (Subsidy *s : Subsidy::Iterate()) {
		if (--s->remaining == 0) {
			if (!s->IsAwarded()) {
				std::pair<NewsReferenceType, NewsReferenceType> reftype = SetupSubsidyDecodeParam(s, SubsidyDecodeParamType::NewsWithdrawn);
				AddNewsItem(STR_NEWS_OFFER_OF_SUBSIDY_EXPIRED, NT_SUBSIDIES, NF_NORMAL, reftype.first, s->src, reftype.second, s->dst);
				AI::BroadcastNewEvent(new ScriptEventSubsidyOfferExpired(s->index));
				Game::NewEvent(new ScriptEventSubsidyOfferExpired(s->index));
			} else {
				if (s->awarded == _local_company) {
					std::pair<NewsReferenceType, NewsReferenceType> reftype = SetupSubsidyDecodeParam(s, SubsidyDecodeParamType::NewsWithdrawn);
					AddNewsItem(STR_NEWS_SUBSIDY_WITHDRAWN_SERVICE, NT_SUBSIDIES, NF_NORMAL, reftype.first, s->src, reftype.second, s->dst);
				}
				AI::BroadcastNewEvent(new ScriptEventSubsidyExpired(s->index));
				Game::NewEvent(new ScriptEventSubsidyExpired(s->index));
			}
			delete s;
			modified = true;
		}
	}

	if (modified) {
		RebuildSubsidisedSourceAndDestinationCache();
	} else if (_settings_game.difficulty.subsidy_duration == 0) {
		/* If subsidy duration is set to 0, subsidies are disabled, so bail out. */
		return;
	} else if (_settings_game.linkgraph.distribution_pax != DT_MANUAL &&
			   _settings_game.linkgraph.distribution_mail != DT_MANUAL &&
			   _settings_game.linkgraph.distribution_armoured != DT_MANUAL &&
			   _settings_game.linkgraph.distribution_default != DT_MANUAL) {
		/* Return early if there are no manually distributed cargoes and if we
		 * don't need to invalidate the subsidies window. */
		return;
	}

	bool passenger_subsidy = false;
	bool town_subsidy = false;
	bool industry_subsidy = false;

	int random_chance = RandomRange(16);

	if (random_chance < 2 && _settings_game.linkgraph.distribution_pax == DT_MANUAL) {
		/* There is a 1/8 chance each month of generating a passenger subsidy. */
		int n = 1000;

		do {
			passenger_subsidy = FindSubsidyPassengerRoute();
		} while (!passenger_subsidy && n--);
	} else if (random_chance == 2) {
		/* Cargo subsidies with a town as a source have a 1/16 chance. */
		int n = 1000;

		do {
			town_subsidy = FindSubsidyTownCargoRoute();
		} while (!town_subsidy && n--);
	} else if (random_chance == 3) {
		/* Cargo subsidies with an industry as a source have a 1/16 chance. */
		int n = 1000;

		do {
			industry_subsidy = FindSubsidyIndustryCargoRoute();
		} while (!industry_subsidy && n--);
	}

	modified |= passenger_subsidy || town_subsidy || industry_subsidy;

	if (modified) InvalidateWindowData(WC_SUBSIDIES_LIST, 0);
});

/**
 * Tests whether given delivery is subsidised and possibly awards the subsidy to delivering company
 * @param cargo_type type of cargo
 * @param company company delivering the cargo
 * @param src_type type of \a src
 * @param src index of source
 * @param st station where the cargo is delivered to
 * @return is the delivery subsidised?
 */
bool CheckSubsidised(CargoID cargo_type, CompanyID company, SourceType src_type, SourceID src, const Station *st)
{
	/* If the source isn't subsidised, don't continue */
	if (src == INVALID_SOURCE) return false;
	switch (src_type) {
		case SourceType::Industry:
			if (!(Industry::Get(src)->part_of_subsidy & POS_SRC)) return false;
			break;
		case SourceType::Town:
			if (!(Town::Get(src)->cache.part_of_subsidy & POS_SRC)) return false;
			break;
		default: return false;
	}

	/* Remember all towns near this station (at least one house in its catchment radius)
	 * which are destination of subsidised path. Do that only if needed */
	std::vector<const Town *> towns_near;
	if (!st->rect.IsEmpty()) {
		for (const Subsidy *s : Subsidy::Iterate()) {
			/* Don't create the cache if there is no applicable subsidy with town as destination */
			if (s->dst_type != SourceType::Town) continue;
			if (s->cargo_type != cargo_type || s->src_type != src_type || s->src != src) continue;
			if (s->IsAwarded() && s->awarded != company) continue;

			BitmapTileIterator it(st->catchment_tiles);
			for (TileIndex tile = it; tile != INVALID_TILE; tile = ++it) {
				if (!IsTileType(tile, MP_HOUSE)) continue;
				const Town *t = Town::GetByTile(tile);
				if (t->cache.part_of_subsidy & POS_DST) include(towns_near, t);
			}
			break;
		}
	}

	bool subsidised = false;

	/* Check if there's a (new) subsidy that applies. There can be more subsidies triggered by this delivery!
	 * Think about the case that subsidies are A->B and A->C and station has both B and C in its catchment area */
	for (Subsidy *s : Subsidy::Iterate()) {
		if (s->cargo_type == cargo_type && s->src_type == src_type && s->src == src && (!s->IsAwarded() || s->awarded == company)) {
			switch (s->dst_type) {
				case SourceType::Industry:
					for (const auto &i : st->industries_near) {
						if (s->dst == i.industry->index) {
							assert(i.industry->part_of_subsidy & POS_DST);
							subsidised = true;
							if (!s->IsAwarded()) s->AwardTo(company);
						}
					}
					break;
				case SourceType::Town:
					for (const Town *tp : towns_near) {
						if (s->dst == tp->index) {
							assert(tp->cache.part_of_subsidy & POS_DST);
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
