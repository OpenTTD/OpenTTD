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


VARDEF Subsidy _subsidies[MAX_PLAYERS];
Pair SetupSubsidyDecodeParam(Subsidy *s, bool mode);
void DeleteSubsidyWithIndustry(byte index);
void DeleteSubsidyWithStation(byte index);

int32 GetTransportedGoodsIncome(uint num_pieces, uint dist, byte transit_days, byte cargo_type);
uint MoveGoodsToStation(uint tile, int w, int h, int type, uint amount);

#endif /* ECONOMY_H */
