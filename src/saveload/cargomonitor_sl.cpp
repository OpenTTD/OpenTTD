/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargomonitor_sl.cpp Code handling saving and loading of Cargo monitoring. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/cargomonitor_sl_compat.h"

#include "../cargomonitor.h"

#include "../safeguards.h"

/** Temporary storage of cargo monitoring data for loading or saving it. */
struct TempStorage {
	CargoMonitorID number;
	uint32_t amount;
};

/** Description of the #TempStorage structure for the purpose of load and save. */
static const SaveLoad _cargomonitor_pair_desc[] = {
	SLE_VAR(TempStorage, number, SLE_UINT32),
	SLE_VAR(TempStorage, amount, SLE_UINT32),
};

static CargoMonitorID FixupCargoMonitor(CargoMonitorID number)
{
	/* Between SLV_EXTEND_CARGOTYPES and SLV_FIX_CARGO_MONITOR, the
	 * CargoMonitorID structure had insufficient packing for more
	 * than 32 cargo types. Here we have to shuffle bits to account
	 * for the change.
	 * Company moved from bits 24-31 to 25-28.
	 * Cargo type increased from bits 19-23 to 19-24.
	 */
	SB(number, 25, 4, GB(number, 24, 4));
	SB(number, 29, 3, 0);
	ClrBit(number, 24);
	return number;
}

/** #_cargo_deliveries monitoring map. */
struct CMDLChunkHandler : ChunkHandler {
	CMDLChunkHandler() : ChunkHandler('CMDL', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_cargomonitor_pair_desc);

		TempStorage storage;

		int i = 0;
		CargoMonitorMap::const_iterator iter = _cargo_deliveries.begin();
		while (iter != _cargo_deliveries.end()) {
			storage.number = iter->first;
			storage.amount = iter->second;

			SlSetArrayIndex(i);
			SlObject(&storage, _cargomonitor_pair_desc);

			i++;
			iter++;
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_cargomonitor_pair_desc, _cargomonitor_pair_sl_compat);

		TempStorage storage;
		bool fix = IsSavegameVersionBefore(SLV_FIX_CARGO_MONITOR);

		ClearCargoDeliveryMonitoring();
		for (;;) {
			if (SlIterateArray() < 0) break;
			SlObject(&storage, slt);

			if (fix) storage.number = FixupCargoMonitor(storage.number);

			std::pair<CargoMonitorID, uint32_t> p(storage.number, storage.amount);
			_cargo_deliveries.insert(p);
		}
	}
};

/** #_cargo_pickups monitoring map. */
struct CMPUChunkHandler : ChunkHandler {
	CMPUChunkHandler() : ChunkHandler('CMPU', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_cargomonitor_pair_desc);

		TempStorage storage;

		int i = 0;
		CargoMonitorMap::const_iterator iter = _cargo_pickups.begin();
		while (iter != _cargo_pickups.end()) {
			storage.number = iter->first;
			storage.amount = iter->second;

			SlSetArrayIndex(i);
			SlObject(&storage, _cargomonitor_pair_desc);

			i++;
			iter++;
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_cargomonitor_pair_desc, _cargomonitor_pair_sl_compat);

		TempStorage storage;
		bool fix = IsSavegameVersionBefore(SLV_FIX_CARGO_MONITOR);

		ClearCargoPickupMonitoring();
		for (;;) {
			if (SlIterateArray() < 0) break;
			SlObject(&storage, slt);

			if (fix) storage.number = FixupCargoMonitor(storage.number);

			std::pair<CargoMonitorID, uint32_t> p(storage.number, storage.amount);
			_cargo_pickups.insert(p);
		}
	}
};

/** Chunk definition of the cargomonitoring maps. */
static const CMDLChunkHandler CMDL;
static const CMPUChunkHandler CMPU;
static const ChunkHandlerRef cargomonitor_chunk_handlers[] = {
	CMDL,
	CMPU,
};

extern const ChunkHandlerTable _cargomonitor_chunk_handlers(cargomonitor_chunk_handlers);
