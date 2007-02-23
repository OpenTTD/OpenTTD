/* $Id$ */

/** @file economy.h */

#ifndef ECONOMY_H
#define ECONOMY_H

void ResetPriceBaseMultipliers(void);
void SetPriceBaseMultiplier(uint price, byte factor);

typedef struct {
	int32 max_loan;         ///< Maximum possible loan
	int32 max_loan_unround; ///< Economy fluctuation status
	int fluct;
	byte interest_rate;     ///< Interest
	byte infl_amount;       ///< inflation amount
	byte infl_amount_pr;    ///< "floating" portion of inflation
} Economy;

VARDEF Economy _economy;

typedef struct Subsidy {
	CargoID cargo_type;
	byte age;
	/* from and to can either be TownID, StationID or IndustryID */
	uint16 from;
	uint16 to;
} Subsidy;


enum ScoreID {
	SCORE_BEGIN      = 0,
	SCORE_VEHICLES   = 0,
	SCORE_STATIONS   = 1,
	SCORE_MIN_PROFIT = 2,
	SCORE_MIN_INCOME = 3,
	SCORE_MAX_INCOME = 4,
	SCORE_DELIVERED  = 5,
	SCORE_CARGO      = 6,
	SCORE_MONEY      = 7,
	SCORE_LOAN       = 8,
	SCORE_TOTAL      = 9,  ///< This must always be the last entry
	SCORE_END        = 10, ///< How many scores are there..

	SCORE_MAX = 1000       ///< The max score that can be in the performance history
	/* the scores together of score_info is allowed to be more! */
};

DECLARE_POSTFIX_INCREMENT(ScoreID);

typedef struct ScoreInfo {
	byte id;    ///< Unique ID of the score
	int needed; ///< How much you need to get the perfect score
	int score;  ///< How much score it will give
} ScoreInfo;

extern const ScoreInfo _score_info[];
extern int _score_part[MAX_PLAYERS][SCORE_END];

int UpdateCompanyRatingAndValue(Player *p, bool update);

VARDEF Subsidy _subsidies[MAX_PLAYERS];
Pair SetupSubsidyDecodeParam(const Subsidy* s, bool mode);
void DeleteSubsidyWithTown(TownID index);
void DeleteSubsidyWithIndustry(IndustryID index);
void DeleteSubsidyWithStation(StationID index);

int32 GetTransportedGoodsIncome(uint num_pieces, uint dist, byte transit_days, CargoID cargo_type);
uint MoveGoodsToStation(TileIndex tile, int w, int h, int type, uint amount);

#endif /* ECONOMY_H */
