/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file economy_func.h Functions related to the economy. */

#ifndef ECONOMY_FUNC_H
#define ECONOMY_FUNC_H

#include "core/geometry_type.hpp"
#include "economy_type.h"
#include "cargo_type.h"
#include "vehicle_type.h"
#include "tile_type.h"
#include "town_type.h"
#include "industry_type.h"
#include "company_type.h"
#include "station_type.h"

void ResetPriceBaseMultipliers();
void SetPriceBaseMultiplier(uint price, byte factor);
void ResetEconomy();

extern const ScoreInfo _score_info[];
extern int _score_part[MAX_COMPANIES][SCORE_END];
extern Economy _economy;
/* Prices and also the fractional part. */
extern Prices _price;
extern uint16 _price_frac[NUM_PRICES];
extern Money  _cargo_payment_rates[NUM_CARGO];
extern uint16 _cargo_payment_rates_frac[NUM_CARGO];

int UpdateCompanyRatingAndValue(Company *c, bool update);
void StartupIndustryDailyChanges(bool init_counter);

Money GetTransportedGoodsIncome(uint num_pieces, uint dist, byte transit_days, CargoID cargo_type);
uint MoveGoodsToStation(TileIndex tile, int w, int h, CargoID type, uint amount, SourceType source_type, SourceID source_id);

void PrepareUnload(Vehicle *front_v);
void LoadUnloadStation(Station *st);

Money GetPriceByIndex(uint8 index);

#endif /* ECONOMY_FUNC_H */
