/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file vice_func.cpp Implementation of the City Vice system. */

#include "stdafx.h"

#include "vice_func.h"
#include "vice_type.h"
#include "town.h"
#include "station_base.h"
#include "station_func.h"
#include "vehicle_base.h"
#include "company_base.h"
#include "news_func.h"
#include "strings_func.h"
#include "effectvehicle_func.h"
#include "landscape.h"
#include "newgrf_house.h"
#include "town_map.h"
#include "settings_type.h"
#include "cargotype.h"
#include "core/random_func.hpp"
#include "core/math_func.hpp"
#include "map_func.h"
#include "ai/ai.hpp"
#include "game/game.hpp"
#include "script/api/script_event_types.hpp"
#include "table/strings.h"
#include "window_func.h"

#include "safeguards.h"


/**
 * Event weight tables indexed by population tier.
 * Order: PettyVandalism, VehicleSabotage, Arson, Riot, CrimeWave
 */
static constexpr uint8_t _vice_event_weights[3][5] = {
	{60, 25, 10,  5,  0},  /* Population < 5000 */
	{40, 25, 20, 10,  5},  /* Population 5000-14999 */
	{20, 20, 25, 20, 15},  /* Population >= 15000 */
};


/** Vice level thresholds for notifications. */
static constexpr uint8_t _vice_thresholds[] = {20, 50, 75};


/**
 * Broadcast a vice level change event to AI/GS.
 * @param t The town whose vice level changed.
 */
static void BroadcastViceLevelChanged(Town *t)
{
	AI::BroadcastNewEvent(new ScriptEventViceLevelChanged(t->index, t->vice_level));
	Game::NewEvent(new ScriptEventViceLevelChanged(t->index, t->vice_level));
}

/**
 * Check for vice level threshold crossing and generate notifications.
 * @param t The town whose vice level may have crossed a threshold.
 */
static void CheckViceThresholdNotification(Town *t)
{
	for (uint8_t threshold : _vice_thresholds) {
		bool prev_below = t->vice_level_prev < threshold;
		bool curr_below = t->vice_level < threshold;

		if (prev_below && !curr_below) {
			/* Rising past threshold */
			StringID str;
			NewsStyle style;
			if (threshold == 20) { str = STR_NEWS_VICE_RISING_CONCERN; style = NewsStyle::Thin; }
			else if (threshold == 50) { str = STR_NEWS_VICE_RISING_WARNING; style = NewsStyle::Small; }
			else { str = STR_NEWS_VICE_RISING_CRITICAL; style = NewsStyle::Normal; }

			AddNewsItem(GetEncodedString(str, t->index),
				NewsType::Crime, style, {}, t->index);
			BroadcastViceLevelChanged(t);
		} else if (!prev_below && curr_below) {
			/* Falling past threshold */
			StringID str;
			NewsStyle style;
			if (threshold == 20) { str = STR_NEWS_VICE_FALLING_CONCERN; style = NewsStyle::Thin; }
			else if (threshold == 50) { str = STR_NEWS_VICE_FALLING_WARNING; style = NewsStyle::Small; }
			else { str = STR_NEWS_VICE_FALLING_CRITICAL; style = NewsStyle::Normal; }

			AddNewsItem(GetEncodedString(str, t->index),
				NewsType::Crime, style, {}, t->index);
			BroadcastViceLevelChanged(t);
		}
	}
}


/**
 * Calculate and update a town's vice level.
 * Called monthly from the economy timer.
 * @param t The town to update.
 */
void UpdateTownViceLevel(Town *t)
{
	t->vice_level_prev = t->vice_level;

	/* Population floor check. */
	if (t->cache.population < _settings_game.economy.city_vice_min_population) {
		t->vice_level = 0;
		CheckViceThresholdNotification(t);
		return;
	}

	/* 1. Base vice level (0-99). Use uint64_t to prevent overflow. */
	uint8_t base_vice = static_cast<uint8_t>(
		(static_cast<uint64_t>(t->cache.population) * 100) /
		(t->cache.population + 5000)
	);

	/* 2. Service modifier (0-60): based on passenger transport percentage. */
	const CargoSpec *passengers = FindFirstCargoWithTownAcceptanceEffect(TAE_PASSENGERS);
	uint8_t pct_transported = 0;
	if (passengers != nullptr) {
		pct_transported = t->GetPercentTransported(passengers->Index());
	}
	uint8_t service_modifier = (pct_transported * 60) / 256;

	/* 3. Adjusted vice after service. */
	int adjusted_vice = std::max(0, static_cast<int>(base_vice) - static_cast<int>(service_modifier));

	/* 4. Rating modifier (-20 to +20): best company rating in town. */
	int16_t best_rating = -1000;
	bool has_rating = false;
	for (const Company *c : Company::Iterate()) {
		if (t->have_ratings.Test(c->index)) {
			has_rating = true;
			best_rating = std::max(best_rating, t->ratings[c->index]);
		}
	}
	int rating_modifier = has_rating ? -(best_rating * 20) / 1000 : 0;

	/* 5. Final vice level (0-100). */
	int final_vice = std::clamp(adjusted_vice + rating_modifier, 0, 100);

	/* 6. Apply policing reduction. */
	if (t->policing_months > 0) {
		final_vice = std::max(0, final_vice - static_cast<int>(t->policing_reduction));
	}

	/* 7. Apply inciting boost. */
	if (t->inciting_months > 0) {
		final_vice = std::min(100, final_vice + static_cast<int>(t->inciting_increase));
	}

	t->vice_level = static_cast<uint8_t>(final_vice);

	/* Check for threshold crossing notifications. */
	CheckViceThresholdNotification(t);
}


/**
 * Pick a random house tile in a town suitable for destruction.
 * @param t The town to search in.
 * @return A valid house tile, or INVALID_TILE if none found.
 */
static TileIndex PickRandomHouseInTown(Town *t)
{
	uint32_t max_sq = t->cache.squared_town_zone_radius[to_underlying(HouseZone::TownEdge)];

	/* Compute integer upper bound on radius without floating point. */
	uint32_t radius = 0;
	while (radius * radius < max_sq) radius++;
	if (radius == 0) radius = 1;

	for (int attempt = 0; attempt < 20; attempt++) {
		int dx = static_cast<int>(RandomRange(radius * 2 + 1)) - static_cast<int>(radius);
		int dy = static_cast<int>(RandomRange(radius * 2 + 1)) - static_cast<int>(radius);
		TileIndex tile = t->xy + TileDiffXY(dx, dy);

		if (!IsValidTile(tile)) continue;
		if (!IsTileType(tile, TileType::House)) continue;
		if (Town::GetByTile(tile) != t) continue;
		if (!IsHouseCompleted(tile)) continue;
		if (!CanDeleteHouse(tile)) continue;

		return tile;
	}

	return INVALID_TILE;
}


/**
 * Perform the Petty Vandalism vice event.
 * @param t The town where the event occurs.
 */
static void DoViceEventPettyVandalism(Town *t)
{
	/* Build a list of stations near the town owned by any company. */
	std::vector<Station *> candidates;
	for (Station *st : t->stations_near) {
		if (Company::IsValidID(st->owner)) candidates.push_back(st);
	}
	if (candidates.empty()) return;

	Station *st = candidates[RandomRange(static_cast<uint>(candidates.size()))];

	/* Reduce rating by 15 for one random cargo at this station. */
	std::vector<CargoType> valid_cargoes;
	for (CargoType ct = 0; ct < NUM_CARGO; ct++) {
		if (st->goods[ct].rating > 0) valid_cargoes.push_back(ct);
	}
	if (!valid_cargoes.empty()) {
		CargoType ct = valid_cargoes[RandomRange(static_cast<uint>(valid_cargoes.size()))];
		st->goods[ct].rating = std::max(0, static_cast<int>(st->goods[ct].rating) - 15);
	}

	AddNewsItem(GetEncodedString(STR_NEWS_VICE_PETTY_VANDALISM, st->index, t->index),
		NewsType::Crime, NewsStyle::Thin, {}, st->index, t->index);

	AI::BroadcastNewEvent(new ScriptEventStationVandalized(st->index, t->index, 15));
	Game::NewEvent(new ScriptEventStationVandalized(st->index, t->index, 15));
}


/**
 * Perform the Vehicle Sabotage vice event.
 * @param t The town where the event occurs.
 */
static void DoViceEventVehicleSabotage(Town *t)
{
	uint32_t max_sq = t->cache.squared_town_zone_radius[to_underlying(HouseZone::TownEdge)];
	std::vector<Vehicle *> candidates;

	for (Vehicle *v : Vehicle::Iterate()) {
		if (v->type != VEH_TRAIN && v->type != VEH_ROAD) continue;
		if (v->vehstatus.Test(VehState::Hidden)) continue;
		if (DistanceSquare(v->tile, t->xy) > max_sq) continue;
		candidates.push_back(v);
	}

	if (candidates.empty()) return;

	Vehicle *v = candidates[RandomRange(static_cast<uint>(candidates.size()))];
	v->breakdown_ctr = 2;
	v->breakdown_delay = 120;

	AddNewsItem(GetEncodedString(STR_NEWS_VICE_VEHICLE_SABOTAGE, t->index),
		NewsType::Crime, NewsStyle::Thin, {}, v->index, t->index);

	AI::BroadcastNewEvent(new ScriptEventVehicleSabotaged(v->index, t->index));
	Game::NewEvent(new ScriptEventVehicleSabotaged(v->index, t->index));
}


/**
 * Perform the Arson vice event.
 * @param t The town where the event occurs.
 */
static void DoViceEventArson(Town *t)
{
	TileIndex tile = PickRandomHouseInTown(t);
	if (tile == INVALID_TILE) return;

	ClearTownHouse(t, tile);

	CreateEffectVehicleAbove(
		TileX(tile) * TILE_SIZE + TILE_SIZE / 2,
		TileY(tile) * TILE_SIZE + TILE_SIZE / 2,
		0, EV_EXPLOSION_SMALL);

	AddNewsItem(GetEncodedString(STR_NEWS_VICE_ARSON, t->index),
		NewsType::Crime, NewsStyle::Small, {}, tile, t->index);

	AI::BroadcastNewEvent(new ScriptEventBuildingDestroyed(t->index, tile, 1));
	Game::NewEvent(new ScriptEventBuildingDestroyed(t->index, tile, 1));
}


/**
 * Perform the Riot vice event.
 * @param t The town where the event occurs.
 */
static void DoViceEventRiot(Town *t)
{
	/* Building count: 1 + RandomRange(clamp(population / 20000, 0, 2)) */
	uint32_t pop = t->cache.population;
	uint32_t max_extra = std::clamp(pop / 20000, 0U, 2U);
	if (_settings_game.difficulty.city_vice_severity == to_underlying(ViceDifficulty::Harsh)) {
		max_extra = std::clamp(pop / 20000, 0U, 3U);
	}
	int buildings = 1 + static_cast<int>(RandomRange(max_extra + 1));

	TileIndex first_tile = INVALID_TILE;
	for (int i = 0; i < buildings; i++) {
		TileIndex tile = PickRandomHouseInTown(t);
		if (tile == INVALID_TILE) continue;
		ClearTownHouse(t, tile);
		if (first_tile == INVALID_TILE) first_tile = tile;

		EffectVehicleType effect = (i == 0) ? EV_EXPLOSION_LARGE : EV_EXPLOSION_SMALL;
		CreateEffectVehicleAbove(
			TileX(tile) * TILE_SIZE + TILE_SIZE / 2,
			TileY(tile) * TILE_SIZE + TILE_SIZE / 2,
			0, effect);
	}

	/* Hit one random station with -15 rating. */
	std::vector<Station *> stations;
	for (Station *st : t->stations_near) {
		if (Company::IsValidID(st->owner)) stations.push_back(st);
	}
	if (!stations.empty()) {
		Station *st = stations[RandomRange(static_cast<uint>(stations.size()))];
		std::vector<CargoType> valid_cargoes;
		for (CargoType ct = 0; ct < NUM_CARGO; ct++) {
			if (st->goods[ct].rating > 0) valid_cargoes.push_back(ct);
		}
		if (!valid_cargoes.empty()) {
			CargoType ct = valid_cargoes[RandomRange(static_cast<uint>(valid_cargoes.size()))];
			st->goods[ct].rating = std::max(0, static_cast<int>(st->goods[ct].rating) - 15);
		}
	}

	/* Reduce all company ratings by 50. */
	for (const Company *c : Company::Iterate()) {
		if (t->have_ratings.Test(c->index)) {
			t->ratings[c->index] = std::max<int16_t>(RATING_BRIBE_DOWN_TO, t->ratings[c->index] - 50);
		}
	}
	SetWindowDirty(WC_TOWN_AUTHORITY, t->index);

	AddNewsItem(GetEncodedString(STR_NEWS_VICE_RIOT, t->index, buildings),
		NewsType::Crime, NewsStyle::Normal, {},
		(first_tile == INVALID_TILE) ? t->xy : first_tile, t->index);

	AI::BroadcastNewEvent(new ScriptEventRiot(t->index, buildings, 1));
	Game::NewEvent(new ScriptEventRiot(t->index, buildings, 1));
}


/**
 * Perform the Crime Wave vice event.
 * @param t The town where the event occurs.
 */
static void DoViceEventCrimeWave(Town *t)
{
	t->crime_wave_months = 3;

	for (Station *st : t->stations_near) {
		if (!Company::IsValidID(st->owner)) continue;
		for (GoodsEntry &ge : st->goods) {
			if (ge.rating > 0) {
				ge.rating = std::max(0, static_cast<int>(ge.rating) - 10);
			}
		}
	}

	AddNewsItem(GetEncodedString(STR_NEWS_VICE_CRIME_WAVE, t->index),
		NewsType::Crime, NewsStyle::Normal, {}, t->index);

	AI::BroadcastNewEvent(new ScriptEventCrimeWave(t->index, 3));
	Game::NewEvent(new ScriptEventCrimeWave(t->index, 3));
}


/**
 * Check whether a vice event should trigger this month.
 * Called monthly after UpdateTownViceLevel().
 * @param t The town to check.
 */
void CheckTownViceEvent(Town *t)
{
	/* Skip if vice is zero or town is in cooldown. */
	if (t->vice_level == 0) return;
	if (t->vice_cooldown > 0) return;
	if (t->cache.population < _settings_game.economy.city_vice_min_population) return;

	/* Calculate effective vice for probability roll. */
	uint effective_vice = t->vice_level * _settings_game.economy.city_vice_intensity;

	/* Apply Harsh severity multiplier. */
	if (_settings_game.difficulty.city_vice_severity == to_underlying(ViceDifficulty::Harsh)) {
		effective_vice = effective_vice * 3 / 2;
	}

	/* Roll against 1000. */
	if (RandomRange(1000) >= effective_vice) return;

	/* Select event type based on population tier. */
	int tier;
	if (t->cache.population < 5000) tier = 0;
	else if (t->cache.population < 15000) tier = 1;
	else tier = 2;

	/* Copy weights, zeroing out disallowed types for Mild severity. */
	uint8_t weights[5];
	std::copy(std::begin(_vice_event_weights[tier]),
	          std::end(_vice_event_weights[tier]), weights);

	if (_settings_game.difficulty.city_vice_severity == to_underlying(ViceDifficulty::Mild)) {
		weights[2] = 0; /* Arson */
		weights[3] = 0; /* Riot */
		weights[4] = 0; /* CrimeWave */
	}

	uint total_weight = 0;
	for (auto w : weights) total_weight += w;
	if (total_weight == 0) return;

	uint roll = RandomRange(total_weight);
	ViceEventType event_type = ViceEventType::PettyVandalism;
	uint cumulative = 0;
	for (int i = 0; i < 5; i++) {
		cumulative += weights[i];
		if (roll < cumulative) {
			event_type = static_cast<ViceEventType>(i);
			break;
		}
	}

	/* Execute the event. */
	switch (event_type) {
		case ViceEventType::PettyVandalism:  DoViceEventPettyVandalism(t); break;
		case ViceEventType::VehicleSabotage: DoViceEventVehicleSabotage(t); break;
		case ViceEventType::Arson:           DoViceEventArson(t); break;
		case ViceEventType::Riot:            DoViceEventRiot(t); break;
		case ViceEventType::CrimeWave:       DoViceEventCrimeWave(t); break;
		default: NOT_REACHED();
	}

	/* Set cooldown based on difficulty. */
	switch (static_cast<ViceDifficulty>(_settings_game.difficulty.city_vice_severity)) {
		case ViceDifficulty::Mild:   t->vice_cooldown = 3; break;
		case ViceDifficulty::Normal: t->vice_cooldown = 2; break;
		case ViceDifficulty::Harsh:  t->vice_cooldown = 1; break;
	}
}
