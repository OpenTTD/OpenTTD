/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file economy_sl.cpp Code handling saving and loading of economy data. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/economy_sl_compat.h"

#include "../economy_func.h"
#include "../economy_base.h"
#include "../vehicle_base.h"

#include "../safeguards.h"

/** Prices in pre 126 savegames */
struct PRICChunkHandler : ChunkHandler {
	PRICChunkHandler() : ChunkHandler("PRIC", ChunkType::ReadOnly) {}

	void Load() const override
	{
		size_t num_prices = 49;
		size_t record_size = (IsSavegameVersionBefore(SaveLoadVersion::UnifyCurrency) ? sizeof(uint32_t) : sizeof(uint64_t)) + sizeof(uint16_t);
		SlSkipBytes(num_prices * record_size);
	}
};

/** Cargo payment rates in pre 126 savegames */
struct CAPRChunkHandler : ChunkHandler {
	CAPRChunkHandler() : ChunkHandler("CAPR", ChunkType::ReadOnly) {}

	void Load() const override
	{
		size_t num_cargo = IsSavegameVersionBefore(SaveLoadVersion::NewGRFCargo) ? 12 : IsSavegameVersionBefore(SaveLoadVersion::ExtendCargotypes) ? 32 : NUM_CARGO;
		size_t record_size = sizeof(uint16_t) + (IsSavegameVersionBefore(SaveLoadVersion::UnifyCurrency) ? sizeof(uint32_t) : sizeof(uint64_t));
		SlSkipBytes(num_cargo * record_size);
	}
};

static const SaveLoad _economy_desc[] = {
	SaveLoad::Variable<VarFileType::I32>("old_max_loan_unround", SLE_OBJECT_ADDRESS(Economy, old_max_loan_unround), SaveLoadVersion::MinVersion, SaveLoadVersion::UnifyCurrency),
	SaveLoad::Variable<VarFileType::I64>("old_max_loan_unround", SLE_OBJECT_ADDRESS(Economy, old_max_loan_unround), SaveLoadVersion::UnifyCurrency, SaveLoadVersion::CumulatedInflation),
	SaveLoad::Variable<VarFileType::U16>("old_max_loan_unround_fract", SLE_OBJECT_ADDRESS(Economy, old_max_loan_unround_fract), SaveLoadVersion::CargoPaymentOverflow, SaveLoadVersion::CumulatedInflation),
	SaveLoad::Variable<VarFileType::U64>("inflation_prices", SLE_OBJECT_ADDRESS(Economy, inflation_prices), SaveLoadVersion::CumulatedInflation),
	SaveLoad::Variable<VarFileType::U64>("inflation_payment", SLE_OBJECT_ADDRESS(Economy, inflation_payment), SaveLoadVersion::CumulatedInflation),
	SaveLoad::Variable<VarFileType::I16>("fluct", SLE_OBJECT_ADDRESS(Economy, fluct)),
	SaveLoad::Variable<VarFileType::U8>("interest_rate", SLE_OBJECT_ADDRESS(Economy, interest_rate)),
	SaveLoad::Variable<VarFileType::U8>("infl_amount", SLE_OBJECT_ADDRESS(Economy, infl_amount)),
	SaveLoad::Variable<VarFileType::U8>("infl_amount_pr", SLE_OBJECT_ADDRESS(Economy, infl_amount_pr)),
	SaveLoad::Variable<VarFileType::U32>("industry_daily_change_counter", SLE_OBJECT_ADDRESS(Economy, industry_daily_change_counter), SaveLoadVersion::SpreadIndustryProductionChanges),
};

/** Economy variables */
struct ECMYChunkHandler : ChunkHandler {
	ECMYChunkHandler() : ChunkHandler("ECMY", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_economy_desc);

		SlSetArrayIndex(0);
		SlObject(&_economy, _economy_desc);
	}


	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_economy_desc, _economy_sl_compat);

		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() == -1) return;
		SlObject(&_economy, slt);
		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() != -1) SlErrorCorrupt("Too many ECMY entries");

		StartupIndustryDailyChanges(IsSavegameVersionBefore(SaveLoadVersion::SpreadIndustryProductionChanges)); // old savegames will need to be initialized
	}
};

static const SaveLoad _cargopayment_desc[] = {
	SaveLoad::Reference<SLRefType::Vehicle>("front", SLE_OBJECT_ADDRESS(CargoPayment, front)),
	SaveLoad::Variable<VarFileType::I64>("route_profit", SLE_OBJECT_ADDRESS(CargoPayment, route_profit)),
	SaveLoad::Variable<VarFileType::I64>("visual_profit", SLE_OBJECT_ADDRESS(CargoPayment, visual_profit)),
	SaveLoad::Variable<VarFileType::I64>("visual_transfer", SLE_OBJECT_ADDRESS(CargoPayment, visual_transfer), SaveLoadVersion::CargoReservation),
};

struct CAPYChunkHandler : ChunkHandler {
	CAPYChunkHandler() : ChunkHandler("CAPY", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_cargopayment_desc);

		for (CargoPayment *cp : CargoPayment::Iterate()) {
			SlSetArrayIndex(cp->index);
			SlObject(cp, _cargopayment_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_cargopayment_desc, _cargopayment_sl_compat);

		int index;

		while ((index = SlIterateArray()) != -1) {
			CargoPayment *cp = CargoPayment::CreateAtIndex(CargoPaymentID(index));
			SlObject(cp, slt);
		}
	}

	void FixPointers() const override
	{
		for (CargoPayment *cp : CargoPayment::Iterate()) {
			SlObject(cp, _cargopayment_desc);
		}
	}
};

static const CAPYChunkHandler CAPY;
static const PRICChunkHandler PRIC;
static const CAPRChunkHandler CAPR;
static const ECMYChunkHandler ECMY;
static const ChunkHandlerRef economy_chunk_handlers[] = {
	CAPY,
	PRIC,
	CAPR,
	ECMY,
};

extern const ChunkHandlerTable _economy_chunk_handlers(economy_chunk_handlers);
