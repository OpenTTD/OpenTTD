/* $Id$ */

/** @file economy_sl.cpp Code handling saving and loading of economy data */

#include "../stdafx.h"
#include "../economy_func.h"

#include "saveload.h"

/** Prices */
static void SaveLoad_PRIC()
{
	int vt = CheckSavegameVersion(65) ? (SLE_FILE_I32 | SLE_VAR_I64) : SLE_INT64;
	SlArray(&_price,      NUM_PRICES, vt);
	SlArray(&_price_frac, NUM_PRICES, SLE_UINT16);
}

/** Cargo payment rates */
static void SaveLoad_CAPR()
{
	uint num_cargo = CheckSavegameVersion(55) ? 12 : NUM_CARGO;
	int vt = CheckSavegameVersion(65) ? (SLE_FILE_I32 | SLE_VAR_I64) : SLE_INT64;
	SlArray(&_cargo_payment_rates,      num_cargo, vt);
	SlArray(&_cargo_payment_rates_frac, num_cargo, SLE_UINT16);
}

static const SaveLoad _economy_desc[] = {
	SLE_CONDVAR(Economy, max_loan,                      SLE_FILE_I32 | SLE_VAR_I64,  0, 64),
	SLE_CONDVAR(Economy, max_loan,                      SLE_INT64,                  65, SL_MAX_VERSION),
	SLE_CONDVAR(Economy, max_loan_unround,              SLE_FILE_I32 | SLE_VAR_I64,  0, 64),
	SLE_CONDVAR(Economy, max_loan_unround,              SLE_INT64,                  65, SL_MAX_VERSION),
	SLE_CONDVAR(Economy, max_loan_unround_fract,        SLE_UINT16,                 70, SL_MAX_VERSION),
	    SLE_VAR(Economy, fluct,                         SLE_INT16),
	    SLE_VAR(Economy, interest_rate,                 SLE_UINT8),
	    SLE_VAR(Economy, infl_amount,                   SLE_UINT8),
	    SLE_VAR(Economy, infl_amount_pr,                SLE_UINT8),
	SLE_CONDVAR(Economy, industry_daily_change_counter, SLE_UINT32,                102, SL_MAX_VERSION),
	    SLE_END()
};

/** Economy variables */
static void Save_ECMY()
{
	SlObject(&_economy, _economy_desc);
}

/** Economy variables */
static void Load_ECMY()
{
	SlObject(&_economy, _economy_desc);
	StartupIndustryDailyChanges(CheckSavegameVersion(102));  // old savegames will need to be initialized
}

extern const ChunkHandler _economy_chunk_handlers[] = {
	{ 'PRIC', SaveLoad_PRIC, SaveLoad_PRIC, CH_RIFF | CH_AUTO_LENGTH},
	{ 'CAPR', SaveLoad_CAPR, SaveLoad_CAPR, CH_RIFF | CH_AUTO_LENGTH},
	{ 'ECMY', Save_ECMY,     Load_ECMY,     CH_RIFF | CH_LAST},
};
