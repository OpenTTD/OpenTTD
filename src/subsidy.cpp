/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
#include "timer/timer_game_economy.h"

#include "table/strings.h"

#include "safeguards.h"

SubsidyPool _subsidy_pool("Subsidy"); ///< Pool for the subsidies.
INSTANTIATE_POOL_METHODS(Subsidy)

/**
 * Get the NewsReference for a subsidy Source.
 * @returns NewsReference.
 */
NewsReference Source::GetNewsReference() const
{
	switch (this->type) {
		case SourceType::Industry: return static_cast<IndustryID>(this->id);
		case SourceType::Town: return static_cast<TownID>(this->id);
		default: NOT_REACHED();
	}
}

/**
 * Get the format string for a subsidy Source.
 * @returns The format string.
 */
StringID Source::GetFormat() const
{
	switch (this->type) {
		case SourceType::Industry: return STR_INDUSTRY_NAME;
		case SourceType::Town: return STR_TOWN_NAME;
		default: NOT_REACHED();
	}
}

/**
 * Marks subsidy as awarded, creates news and AI event
 * @param company awarded company
 */
void Subsidy::AwardTo(CompanyID company)
{
	assert(!this->IsAwarded());

	this->awarded = company;
	this->remaining = _settings_game.difficulty.subsidy_duration * CalendarTime::MONTHS_IN_YEAR;

	std::string company_name = GetString(STR_COMPANY_NAME, company);

	/* Add a news item */
	const CargoSpec *cs = CargoSpec::Get(this->cargo_type);
	EncodedString headline = GetEncodedString(STR_NEWS_SERVICE_SUBSIDY_AWARDED_HALF + _settings_game.difficulty.subsidy_multiplier, std::move(company_name), cs->name, this->src.GetFormat(), this->src.id, this->dst.GetFormat(), this->dst.id, _settings_game.difficulty.subsidy_duration);
	AddNewsItem(std::move(headline), NewsType::Subsidies, NewsStyle::Normal, {}, this->src.GetNewsReference(), this->dst.GetNewsReference());
	AI::BroadcastNewEvent(new ScriptEventSubsidyAwarded(this->index));
	Game::NewEvent(new ScriptEventSubsidyAwarded(this->index));

	InvalidateWindowData(WC_SUBSIDIES_LIST, 0);
}

/**
 * Sets a flag indicating that given town/industry is part of subsidised route.
 * @param source actual source
 * @param flag flag to set
 */
static inline void SetPartOfSubsidyFlag(Source source, PartOfSubsidy flag)
{
	switch (source.type) {
		case SourceType::Industry: Industry::Get(source.ToIndustryID())->part_of_subsidy.Set(flag); return;
		case SourceType::Town: Town::Get(source.ToTownID())->cache.part_of_subsidy.Set(flag); return;
		default: NOT_REACHED();
	}
}

/** Perform a full rebuild of the subsidies cache. */
void RebuildSubsidisedSourceAndDestinationCache()
{
	for (Town *t : Town::Iterate()) t->cache.part_of_subsidy = {};

	for (Industry *i : Industry::Iterate()) i->part_of_subsidy = {};

	for (const Subsidy *s : Subsidy::Iterate()) {
		SetPartOfSubsidyFlag(s->src, PartOfSubsidy::Source);
		SetPartOfSubsidyFlag(s->dst, PartOfSubsidy::Destination);
	}
}

/**
 * Delete the subsidies associated with a given cargo source type and id.
 * @param source The source to look for.
 */
void DeleteSubsidyWith(Source source)
{
	bool dirty = false;

	for (Subsidy *s : Subsidy::Iterate()) {
		if (s->src == source || s->dst == source) {
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
 * @param src The source.
 * @param dst The destination.
 * @return \c true if the subsidy already exists, \c false if not.
 */
static bool CheckSubsidyDuplicate(CargoType cargo, Source src, Source dst)
{
	for (const Subsidy *s : Subsidy::Iterate()) {
		if (s->cargo_type == cargo && s->src == src && s->dst == dst) {
			return true;
		}
	}
	return false;
}

/**
 * Checks if the source and destination of a subsidy are inside the distance limit.
 * @param src Source of cargo.
 * @param dst Destination of cargo.
 * @return True if they are inside the distance limit.
 */
static bool CheckSubsidyDistance(Source src, Source dst)
{
	TileIndex tile_src = (src.type == SourceType::Town) ? Town::Get(src.ToTownID())->xy : Industry::Get(src.ToIndustryID())->location.tile;
	TileIndex tile_dst = (dst.type == SourceType::Town) ? Town::Get(dst.ToTownID())->xy : Industry::Get(dst.ToIndustryID())->location.tile;

	return (DistanceManhattan(tile_src, tile_dst) <= SUBSIDY_MAX_DISTANCE);
}

/**
 * Creates a subsidy with the given parameters.
 * @param cargo_type Subsidised cargo.
 * @param src Source of cargo.
 * @param dst Destination of cargo.
 */
void CreateSubsidy(CargoType cargo_type, Source src, Source dst)
{
	Subsidy *s = Subsidy::Create(cargo_type, src, dst, SUBSIDY_OFFER_MONTHS);

	const CargoSpec *cs = CargoSpec::Get(s->cargo_type);
	EncodedString headline = GetEncodedString(STR_NEWS_SERVICE_SUBSIDY_OFFERED, cs->name, s->src.GetFormat(), s->src.id, s->dst.GetFormat(), s->dst.id, _settings_game.difficulty.subsidy_duration);
	AddNewsItem(std::move(headline), NewsType::Subsidies, NewsStyle::Normal, {}, s->src.GetNewsReference(), s->dst.GetNewsReference());
	SetPartOfSubsidyFlag(s->src, PartOfSubsidy::Source);
	SetPartOfSubsidyFlag(s->dst, PartOfSubsidy::Destination);
	AI::BroadcastNewEvent(new ScriptEventSubsidyOffer(s->index));
	Game::NewEvent(new ScriptEventSubsidyOffer(s->index));

	InvalidateWindowData(WC_SUBSIDIES_LIST, 0);
}

/**
 * Create a new subsidy.
 * @param flags type of operation
 * @param cargo_type CargoType of subsidy.
 * @param src Source.
 * @param dst Destination.
 * @return the cost of this operation or an error
 */
CommandCost CmdCreateSubsidy(DoCommandFlags flags, CargoType cargo_type, Source src, Source dst)
{
	if (!Subsidy::CanAllocateItem()) return CMD_ERROR;

	if (_current_company != OWNER_DEITY) return CMD_ERROR;

	if (cargo_type >= NUM_CARGO || !::CargoSpec::Get(cargo_type)->IsValid()) return CMD_ERROR;

	switch (src.type) {
		case SourceType::Town:
			if (!Town::IsValidID(src.ToTownID())) return CMD_ERROR;
			break;
		case SourceType::Industry:
			if (!Industry::IsValidID(src.ToIndustryID())) return CMD_ERROR;
			break;
		default:
			return CMD_ERROR;
	}
	switch (dst.type) {
		case SourceType::Town:
			if (!Town::IsValidID(dst.ToTownID())) return CMD_ERROR;
			break;
		case SourceType::Industry:
			if (!Industry::IsValidID(dst.ToIndustryID())) return CMD_ERROR;
			break;
		default:
			return CMD_ERROR;
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		CreateSubsidy(cargo_type, src, dst);
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

	/* Pick a random TPE_PASSENGER type */
	uint32_t r = RandomRange(static_cast<uint>(CargoSpec::town_production_cargoes[TPE_PASSENGERS].size()));
	CargoType cargo_type = CargoSpec::town_production_cargoes[TPE_PASSENGERS][r]->Index();

	const Town *src = Town::GetRandom();
	if (src->cache.population < SUBSIDY_PAX_MIN_POPULATION ||
			src->GetPercentTransported(cargo_type) > SUBSIDY_MAX_PCT_TRANSPORTED) {
		return false;
	}

	const Town *dst = Town::GetRandom();
	if (dst->cache.population < SUBSIDY_PAX_MIN_POPULATION || src == dst) {
		return false;
	}

	if (DistanceManhattan(src->xy, dst->xy) > SUBSIDY_MAX_DISTANCE) return false;
	if (CheckSubsidyDuplicate(cargo_type, {src->index, SourceType::Town}, {dst->index, SourceType::Town})) return false;

	CreateSubsidy(cargo_type, {src->index, SourceType::Town}, {dst->index, SourceType::Town});

	return true;
}

bool FindSubsidyCargoDestination(CargoType cargo_type, Source src);


/**
 * Tries to create a cargo subsidy with a town as source.
 * @return True iff the subsidy was created.
 */
bool FindSubsidyTownCargoRoute()
{
	if (!Subsidy::CanAllocateItem()) return false;

	/* Select a random town. */
	const Town *src_town = Town::GetRandom();
	if (src_town->cache.population < SUBSIDY_CARGO_MIN_POPULATION) return false;

	/* Calculate the produced cargo of houses around town center. */
	CargoArray town_cargo_produced{};
	TileArea ta = TileArea(src_town->xy, 1, 1).Expand(SUBSIDY_TOWN_CARGO_RADIUS);
	for (TileIndex tile : ta) {
		if (IsTileType(tile, TileType::House)) {
			AddProducedCargo(tile, town_cargo_produced);
		}
	}

	/* Passenger subsidies are not handled here. */
	for (const CargoSpec *cs : CargoSpec::town_production_cargoes[TPE_PASSENGERS]) {
		town_cargo_produced[cs->Index()] = 0;
	}

	uint8_t cargo_count = town_cargo_produced.GetCount();

	/* No cargo produced at all? */
	if (cargo_count == 0) return false;

	/* Choose a random cargo that is produced in the town. */
	uint8_t cargo_number = RandomRange(cargo_count);
	CargoType cargo_type;
	for (cargo_type = 0; cargo_type < NUM_CARGO; cargo_type++) {
		if (town_cargo_produced[cargo_type] > 0) {
			if (cargo_number == 0) break;
			cargo_number--;
		}
	}

	/* Avoid using invalid NewGRF cargoes. */
	if (!CargoSpec::Get(cargo_type)->IsValid() ||
			_settings_game.linkgraph.GetDistributionType(cargo_type) != DistributionType::Manual) {
		return false;
	}

	/* Quit if the percentage transported is large enough. */
	if (src_town->GetPercentTransported(cargo_type) > SUBSIDY_MAX_PCT_TRANSPORTED) return false;

	return FindSubsidyCargoDestination(cargo_type, {src_town->index, SourceType::Town});
}

/**
 * Tries to create a cargo subsidy with an industry as source.
 * @return True iff the subsidy was created.
 */
bool FindSubsidyIndustryCargoRoute()
{
	if (!Subsidy::CanAllocateItem()) return false;

	/* Select a random industry. */
	const Industry *src_ind = Industry::GetRandom();
	if (src_ind == nullptr) return false;

	uint trans, total;

	CargoType cargo_type;

	/* Randomize cargo type */
	int num_cargos = std::ranges::count_if(src_ind->produced, [](const auto &p) { return IsValidCargoType(p.cargo); });
	if (num_cargos == 0) return false; // industry produces nothing
	int cargo_num = RandomRange(num_cargos) + 1;

	auto it = std::begin(src_ind->produced);
	for (/* nothing */; it != std::end(src_ind->produced); ++it) {
		if (IsValidCargoType(it->cargo)) cargo_num--;
		if (cargo_num == 0) break;
	}
	assert(it != std::end(src_ind->produced)); // indicates loop didn't end as intended

	cargo_type = it->cargo;
	trans = it->history[LAST_MONTH].PctTransported();
	total = it->history[LAST_MONTH].production;

	/* Quit if no production in this industry
	 * or if the pct transported is already large enough
	 * or if the cargo is automatically distributed */
	if (total == 0 || trans > SUBSIDY_MAX_PCT_TRANSPORTED ||
			!IsValidCargoType(cargo_type) ||
			_settings_game.linkgraph.GetDistributionType(cargo_type) != DistributionType::Manual) {
		return false;
	}

	return FindSubsidyCargoDestination(cargo_type, {src_ind->index, SourceType::Industry});
}

/**
 * Tries to find a suitable destination for the given source and cargo.
 * @param cargo_type Subsidized cargo.
 * @param src Source of cargo.
 * @return True iff the subsidy was created.
 */
bool FindSubsidyCargoDestination(CargoType cargo_type, Source src)
{
	/* Choose a random destination. */
	Source dst{Source::Invalid, Chance16(1, 2) ? SourceType::Town : SourceType::Industry};

	switch (dst.type) {
		case SourceType::Town: {
			/* Select a random town. */
			const Town *dst_town = Town::GetRandom();

			/* Calculate cargo acceptance of houses around town center. */
			CargoArray town_cargo_accepted{};
			CargoTypes always_accepted{};
			TileArea ta = TileArea(dst_town->xy, 1, 1).Expand(SUBSIDY_TOWN_CARGO_RADIUS);
			for (TileIndex tile : ta) {
				if (IsTileType(tile, TileType::House)) {
					AddAcceptedCargo(tile, town_cargo_accepted, always_accepted);
				}
			}

			/* Check if the town can accept this cargo. */
			if (town_cargo_accepted[cargo_type] < 8) return false;

			dst.SetIndex(dst_town->index);
			break;
		}

		case SourceType::Industry: {
			/* Select a random industry. */
			const Industry *dst_ind = Industry::GetRandom();
			if (dst_ind == nullptr) return false;

			/* The industry must accept the cargo */
			if (!dst_ind->IsCargoAccepted(cargo_type)) return false;

			dst.SetIndex(dst_ind->index);
			break;
		}

		default: NOT_REACHED();
	}

	/* Check that the source and the destination are not the same. */
	if (src == dst) return false;

	/* Check distance between source and destination. */
	if (!CheckSubsidyDistance(src, dst)) return false;

	/* Avoid duplicate subsidies. */
	if (CheckSubsidyDuplicate(cargo_type, src, dst)) return false;

	CreateSubsidy(cargo_type, src, dst);

	return true;
}

/** Perform the economy monthly update of open subsidies, and try to create a new one. */
static const IntervalTimer<TimerGameEconomy> _economy_subsidies_monthly({TimerGameEconomy::Trigger::Month, TimerGameEconomy::Priority::Subsidy}, [](auto)
{
	bool modified = false;

	for (Subsidy *s : Subsidy::Iterate()) {
		if (--s->remaining == 0) {
			if (!s->IsAwarded()) {
				const CargoSpec *cs = CargoSpec::Get(s->cargo_type);
				EncodedString headline = GetEncodedString(STR_NEWS_OFFER_OF_SUBSIDY_EXPIRED, cs->name, s->src.GetFormat(), s->src.id, s->dst.GetFormat(), s->dst.id);
				AddNewsItem(std::move(headline), NewsType::Subsidies, NewsStyle::Normal, {}, s->src.GetNewsReference(), s->dst.GetNewsReference());
				AI::BroadcastNewEvent(new ScriptEventSubsidyOfferExpired(s->index));
				Game::NewEvent(new ScriptEventSubsidyOfferExpired(s->index));
			} else {
				if (s->awarded == _local_company) {
					const CargoSpec *cs = CargoSpec::Get(s->cargo_type);
					EncodedString headline = GetEncodedString(STR_NEWS_SUBSIDY_WITHDRAWN_SERVICE, cs->name, s->src.GetFormat(), s->src.id, s->dst.GetFormat(), s->dst.id);
					AddNewsItem(std::move(headline), NewsType::Subsidies, NewsStyle::Normal, {}, s->src.GetNewsReference(), s->dst.GetNewsReference());
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
	} else if (_settings_game.linkgraph.distribution_pax != DistributionType::Manual &&
			   _settings_game.linkgraph.distribution_mail != DistributionType::Manual &&
			   _settings_game.linkgraph.distribution_armoured != DistributionType::Manual &&
			   _settings_game.linkgraph.distribution_default != DistributionType::Manual) {
		/* Return early if there are no manually distributed cargoes and if we
		 * don't need to invalidate the subsidies window. */
		return;
	}

	bool passenger_subsidy = false;
	bool town_subsidy = false;
	bool industry_subsidy = false;

	int random_chance = RandomRange(16);

	if (random_chance < 2 && _settings_game.linkgraph.distribution_pax == DistributionType::Manual) {
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
 * @param src source of cargo
 * @param st station where the cargo is delivered to
 * @return is the delivery subsidised?
 */
bool CheckSubsidised(CargoType cargo_type, CompanyID company, Source src, const Station *st)
{
	/* If the source isn't subsidised, don't continue */
	if (!src.IsValid()) return false;
	switch (src.type) {
		case SourceType::Industry:
			if (!Industry::Get(src.ToIndustryID())->part_of_subsidy.Test(PartOfSubsidy::Source)) return false;
			break;
		case SourceType::Town:
			if (!Town::Get(src.ToTownID())->cache.part_of_subsidy.Test(PartOfSubsidy::Source)) return false;
			break;
		default: return false;
	}

	/* Remember all towns near this station (at least one house in its catchment radius)
	 * which are destination of subsidised path. Do that only if needed */
	std::vector<const Town *> towns_near;
	if (!st->rect.IsEmpty()) {
		for (const Subsidy *s : Subsidy::Iterate()) {
			/* Don't create the cache if there is no applicable subsidy with town as destination */
			if (s->dst.type != SourceType::Town) continue;
			if (s->cargo_type != cargo_type || s->src != src) continue;
			if (s->IsAwarded() && s->awarded != company) continue;

			BitmapTileIterator it(st->catchment_tiles);
			for (TileIndex tile = it; tile != INVALID_TILE; tile = ++it) {
				if (!IsTileType(tile, TileType::House)) continue;
				const Town *t = Town::GetByTile(tile);
				if (t->cache.part_of_subsidy.Test(PartOfSubsidy::Destination)) include(towns_near, t);
			}
			break;
		}
	}

	bool subsidised = false;

	/* Check if there's a (new) subsidy that applies. There can be more subsidies triggered by this delivery!
	 * Think about the case that subsidies are A->B and A->C and station has both B and C in its catchment area */
	for (Subsidy *s : Subsidy::Iterate()) {
		if (s->cargo_type == cargo_type && s->src == src && (!s->IsAwarded() || s->awarded == company)) {
			switch (s->dst.type) {
				case SourceType::Industry:
					for (const auto &i : st->industries_near) {
						if (s->dst.ToIndustryID() == i.industry->index) {
							assert(i.industry->part_of_subsidy.Test(PartOfSubsidy::Destination));
							subsidised = true;
							if (!s->IsAwarded()) s->AwardTo(company);
						}
					}
					break;
				case SourceType::Town:
					for (const Town *tp : towns_near) {
						if (s->dst.ToTownID() == tp->index) {
							assert(tp->cache.part_of_subsidy.Test(PartOfSubsidy::Destination));
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
