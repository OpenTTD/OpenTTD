/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file economy_func.h Functions related to the economy. */

#ifndef ECONOMY_FUNC_H
#define ECONOMY_FUNC_H

#include "economy_type.h"
#include "station_type.h"
#include "cargo_type.h"
#include "vehicle_type.h"
#include "company_type.h"
#include "settings_type.h"
#include "core/random_func.hpp"

void ResetPriceBaseMultipliers();
void SetPriceBaseMultiplier(Price price, int factor);

extern const ScoreInfo _score_info[];
extern int64_t _score_part[MAX_COMPANIES][SCORE_END];
extern Economy _economy;
/* Prices and also the fractional part. */
extern Prices _price;

int UpdateCompanyRatingAndValue(Company *c, bool update);
void StartupIndustryDailyChanges(bool init_counter);

Money GetTransportedGoodsIncome(uint num_pieces, uint dist, uint16_t transit_periods, CargoID cargo_type);
uint MoveGoodsToStation(CargoID type, uint amount, SourceType source_type, SourceID source_id, const StationList *all_stations, Owner exclusivity = INVALID_OWNER);

void PrepareUnload(Vehicle *front_v);
void LoadUnloadStation(Station *st);

Money GetPrice(Price index, uint cost_factor, const struct GRFFile *grf_file, int shift = 0);

void InitializeEconomy();
void RecomputePrices();
bool AddInflation(bool check_year = true);

/**
 * Is the economy in recession?
 * @return \c True if economy is in recession, \c false otherwise.
 */
inline bool EconomyIsInRecession()
{
	return _economy.fluct <= 0;
}

/**
 * Scale a number by the inverse of the cargo scale setting, e.g. a scale of 25% multiplies the number by 4.
 * @param num The number to scale.
 * @param town Are we scaling town production, or industry production?
 * @return The number scaled by the inverse of the cargo scale setting, minimum of 1.
 */
static uint ScaleByInverseCargoScale(uint num, bool town)
{
	uint16_t percentage = (town ? _settings_game.economy.town_cargo_scale : _settings_game.economy.industry_cargo_scale);

	/* We might not need to do anything. */
	if (percentage == 100) return num;

	/* Never return 0, since we often divide by this number. */
	return std::max((num * 100) / percentage, 1u);
}

/**
 * Scale a number by the cargo scale setting.
 * @param num The number to scale.
 * @param town Are we scaling town production, or industry production?
 * @return The number scaled by the current cargo scale setting. May be 0.
 */
inline uint ScaleByCargoScale(uint num, bool town)
{
	/* Don't bother scaling in the menu, especially since settings don't exist when starting OpenTTD and trying to read them crashes the game. */
	if (_game_mode == GM_MENU) return num;

	if (num == 0) return num;

	uint16_t percentage = (town ? _settings_game.economy.town_cargo_scale : _settings_game.economy.industry_cargo_scale);

	/* We might not need to do anything. */
	if (percentage == 100) return num;

	uint scaled = (num * percentage) / 100;

	/* We might round down to 0, so we compensate with a random chance approximately equal to the economy scale,
	 *  e.g. at 25% scale there's a 1/4 chance to round up to 1. */
	if (scaled == 0 && Chance16(1, ScaleByInverseCargoScale(1, town))) return 1;

	return scaled;
}

#endif /* ECONOMY_FUNC_H */
