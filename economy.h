#ifndef ECONOMY_H
#define ECONOMY_H

typedef struct {
	// Maximum possible loan
	int32 max_loan;
	int32 max_loan_unround;
	// Economy fluctuation status
	int fluct;
	// Interest
	byte interest_rate;
	byte infl_amount;
	byte infl_amount_pr;
} Economy;

VARDEF Economy _economy;

typedef struct Subsidy {
	byte cargo_type;
	byte age;
	byte from;
	byte to;
} Subsidy;


enum {
    SCORE_VEHICLES = 0,
    SCORE_STATIONS = 1,
    SCORE_MIN_PROFIT = 2,
    SCORE_MIN_INCOME = 3,
    SCORE_MAX_INCOME = 4,
    SCORE_DELIVERED = 5,
    SCORE_CARGO = 6,
    SCORE_MONEY = 7,
    SCORE_LOAN = 8,
    SCORE_TOTAL = 9, // This must always be the last entry

    NUM_SCORE = 10, // How many scores are there..

    SCORE_MAX = 1000, 	// The max score that can be in the performance history
    					//  the scores together of score_info is allowed to be more!
};

typedef struct ScoreInfo {
    byte id;			// Unique ID of the score
    int needed;			// How much you need to get the perfect score
    int score;			// How much score it will give
} ScoreInfo;

static const ScoreInfo score_info[] = {
    {SCORE_VEHICLES,		120, 			100},
    {SCORE_STATIONS,		80, 			100},
    {SCORE_MIN_PROFIT,	10000,		100},
    {SCORE_MIN_INCOME,	50000,		50},
    {SCORE_MAX_INCOME,	100000,		100},
    {SCORE_DELIVERED,		40000, 		400},
    {SCORE_CARGO,				8,				50},
    {SCORE_MONEY,				10000000,	50},
    {SCORE_LOAN,				250000,		50},
    {SCORE_TOTAL,				0,				0}
};

int _score_part[MAX_PLAYERS][NUM_SCORE];

int UpdateCompanyRatingAndValue(Player *p, bool update);
void UpdatePlayerHouse(Player *p, uint score);


VARDEF Subsidy _subsidies[MAX_PLAYERS];
Pair SetupSubsidyDecodeParam(Subsidy *s, bool mode);
void DeleteSubsidyWithIndustry(byte index);
void DeleteSubsidyWithStation(byte index);
void RemoteSubsidyAdd(Subsidy *s_new); 

int32 GetTransportedGoodsIncome(uint num_pieces, uint dist, byte transit_days, byte cargo_type);
uint MoveGoodsToStation(uint tile, int w, int h, int type, uint amount);

#endif /* ECONOMY_H */
