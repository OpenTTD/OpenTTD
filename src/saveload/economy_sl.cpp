/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file economy_sl.cpp Code handling saving and loading of economy data */

#include "../stdafx.h"
#include "../economy_func.h"
#include "../economy_base.h"

#include "saveload.h"

#include "../safeguards.h"

/** Prices in pre 126 savegames */
static void Load_PRIC()
{
	/* Old games store 49 base prices, very old games store them as int32 */
	int vt = IsSavegameVersionBefore(65) ? SLE_FILE_I32 : SLE_FILE_I64;
	SlArray(NULL, 49, vt | SLE_VAR_NULL);
	SlArray(NULL, 49, SLE_FILE_U16 | SLE_VAR_NULL);
}

/** Cargo payment rates in pre 126 savegames */
static void Load_CAPR()
{
	uint num_cargo = IsSavegameVersionBefore(55) ? 12 : IsSavegameVersionBefore(199) ? 32 : NUM_CARGO;
	int vt = IsSavegameVersionBefore(65) ? SLE_FILE_I32 : SLE_FILE_I64;
	SlArray(NULL, num_cargo, vt | SLE_VAR_NULL);
	SlArray(NULL, num_cargo, SLE_FILE_U16 | SLE_VAR_NULL);
}

static const SaveLoad _economy_desc[] = {
	SLE_CONDNULL(4,                                                                  0, 64),             // max_loan
	SLE_CONDNULL(8,                                                                 65, 143), // max_loan
	SLE_CONDVAR(Economy, old_max_loan_unround,          SLE_FILE_I32 | SLE_VAR_I64,  0, 64),
	SLE_CONDVAR(Economy, old_max_loan_unround,          SLE_INT64,                  65, 125),
	SLE_CONDVAR(Economy, old_max_loan_unround_fract,    SLE_UINT16,                 70, 125),
	SLE_CONDVAR(Economy, inflation_prices,              SLE_UINT64,                126, SL_MAX_VERSION),
	SLE_CONDVAR(Economy, inflation_payment,             SLE_UINT64,                126, SL_MAX_VERSION),
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
	StartupIndustryDailyChanges(IsSavegameVersionBefore(102));  // old savegames will need to be initialized
}

static const SaveLoad _cargopayment_desc[] = {
	    SLE_REF(CargoPayment, front,           REF_VEHICLE),
	    SLE_VAR(CargoPayment, route_profit,    SLE_INT64),
	    SLE_VAR(CargoPayment, visual_profit,   SLE_INT64),
	SLE_CONDVAR(CargoPayment, visual_transfer, SLE_INT64, 181, SL_MAX_VERSION),
	    SLE_END()
};

static void Save_CAPY()
{
	CargoPayment *cp;
	FOR_ALL_CARGO_PAYMENTS(cp) {
		SlSetArrayIndex(cp->index);
		SlObject(cp, _cargopayment_desc);
	}
}

static void Load_CAPY()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		CargoPayment *cp = new (index) CargoPayment();
		SlObject(cp, _cargopayment_desc);
	}
}

static void Ptrs_CAPY()
{
	CargoPayment *cp;
	FOR_ALL_CARGO_PAYMENTS(cp) {
		SlObject(cp, _cargopayment_desc);
	}
}


extern const ChunkHandler _economy_chunk_handlers[] = {
	{ 'CAPY', Save_CAPY,     Load_CAPY,     Ptrs_CAPY, NULL, CH_ARRAY},
	{ 'PRIC', NULL,          Load_PRIC,     NULL,      NULL, CH_RIFF | CH_AUTO_LENGTH},
	{ 'CAPR', NULL,          Load_CAPR,     NULL,      NULL, CH_RIFF | CH_AUTO_LENGTH},
	{ 'ECMY', Save_ECMY,     Load_ECMY,     NULL,      NULL, CH_RIFF | CH_LAST},
};
