/* $Id$ */

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
#include "player_type.h"
#include "station_type.h"

struct Player;

void ResetPriceBaseMultipliers();
void SetPriceBaseMultiplier(uint price, byte factor);

extern const ScoreInfo _score_info[];
extern int _score_part[MAX_PLAYERS][SCORE_END];
extern Economy _economy;
extern Subsidy _subsidies[MAX_PLAYERS];
/* Prices and also the fractional part. */
extern Prices _price;
extern uint16 _price_frac[NUM_PRICES];
extern Money  _cargo_payment_rates[NUM_CARGO];
extern uint16 _cargo_payment_rates_frac[NUM_CARGO];

int UpdateCompanyRatingAndValue(Player *p, bool update);
Pair SetupSubsidyDecodeParam(const Subsidy *s, bool mode);
void DeleteSubsidyWithTown(TownID index);
void DeleteSubsidyWithIndustry(IndustryID index);
void DeleteSubsidyWithStation(StationID index);

Money GetTransportedGoodsIncome(uint num_pieces, uint dist, byte transit_days, CargoID cargo_type);
uint MoveGoodsToStation(TileIndex tile, int w, int h, CargoID type, uint amount);

void VehiclePayment(Vehicle *front_v);
void LoadUnloadStation(Station *st);

Money GetPriceByIndex(uint8 index);

#endif /* ECONOMY_FUNC_H */
