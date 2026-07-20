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
		/* Old games store 49 base prices, very old games store them as int32_t */
		VarFileType vt = IsSavegameVersionBefore(SaveLoadVersion::UnifyCurrency) ? VarFileType::I32 : VarFileType::I64;
		SlCopy(nullptr, 49, vt | VarMemType::Null);
		SlCopy(nullptr, 49, VarFileType::U16 | VarMemType::Null);
	}
};

/** Cargo payment rates in pre 126 savegames */
struct CAPRChunkHandler : ChunkHandler {
	CAPRChunkHandler() : ChunkHandler("CAPR", ChunkType::ReadOnly) {}

	void Load() const override
	{
		uint num_cargo = IsSavegameVersionBefore(SaveLoadVersion::NewGRFCargo) ? 12 : IsSavegameVersionBefore(SaveLoadVersion::ExtendCargotypes) ? 32 : NUM_CARGO;
		VarFileType vt = IsSavegameVersionBefore(SaveLoadVersion::UnifyCurrency) ? VarFileType::I32 : VarFileType::I64;
		SlCopy(nullptr, num_cargo, vt | VarMemType::Null);
		SlCopy(nullptr, num_cargo, VarFileType::U16 | VarMemType::Null);
	}
};

static const SaveLoad _economy_desc[] = {
	SLE_CONDVAR(Economy, old_max_loan_unround, VarFileType::I32 | VarMemType::I64, SaveLoadVersion::MinVersion, SaveLoadVersion::UnifyCurrency),
	SLE_CONDVAR(Economy, old_max_loan_unround, VarTypes::I64, SaveLoadVersion::UnifyCurrency, SaveLoadVersion::CumulatedInflation),
	SLE_CONDVAR(Economy, old_max_loan_unround_fract, VarTypes::U16, SaveLoadVersion::CargoPaymentOverflow, SaveLoadVersion::CumulatedInflation),
	SLE_CONDVAR(Economy, inflation_prices, VarTypes::U64, SaveLoadVersion::CumulatedInflation, SaveLoadVersion::MaxVersion),
	SLE_CONDVAR(Economy, inflation_payment, VarTypes::U64, SaveLoadVersion::CumulatedInflation, SaveLoadVersion::MaxVersion),
	SaveLoad::Variable<VarFileType::I16>(SLE_NAME_AND_OBJECT_ADDRESS(Economy, fluct)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Economy, interest_rate)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Economy, infl_amount)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Economy, infl_amount_pr)),
	SLE_CONDVAR(Economy, industry_daily_change_counter, VarTypes::U32, SaveLoadVersion::SpreadIndustryProductionChanges, SaveLoadVersion::MaxVersion),
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
	SaveLoad::Reference<SLRefType::Vehicle>(SLE_NAME_AND_OBJECT_ADDRESS(CargoPayment, front)),
	SaveLoad::Variable<VarFileType::I64>(SLE_NAME_AND_OBJECT_ADDRESS(CargoPayment, route_profit)),
	SaveLoad::Variable<VarFileType::I64>(SLE_NAME_AND_OBJECT_ADDRESS(CargoPayment, visual_profit)),
	SLE_CONDVAR(CargoPayment, visual_transfer, VarTypes::I64, SaveLoadVersion::CargoReservation, SaveLoadVersion::MaxVersion),
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
