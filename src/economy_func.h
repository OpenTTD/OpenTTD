/* $Id$ */

/** @file economy_func.h Functions related to the economy. */

#ifndef ECONOMY_FUNC_H
#define ECONOMY_FUNC_H

#include "economy_type.h"

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

int UpdateCompanyRatingAndValue(Player *p, bool update);
Pair SetupSubsidyDecodeParam(const Subsidy *s, bool mode);
void DeleteSubsidyWithTown(TownID index);
void DeleteSubsidyWithIndustry(IndustryID index);
void DeleteSubsidyWithStation(StationID index);

Money GetTransportedGoodsIncome(uint num_pieces, uint dist, byte transit_days, CargoID cargo_type);
uint MoveGoodsToStation(TileIndex tile, int w, int h, CargoID type, uint amount);

void VehiclePayment(Vehicle *front_v);
void LoadUnloadStation(Station *st);

#endif /* ECONOMY_FUNC_H */
