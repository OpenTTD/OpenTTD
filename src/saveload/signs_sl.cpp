/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file signs_sl.cpp Code handling saving and loading of economy data. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/signs_sl_compat.h"

#include "../signs_base.h"
#include "../fios.h"

#include "../safeguards.h"

/** Description of a sign within the savegame. */
static const SaveLoad _sign_desc[] = {
	SaveLoad::Variable<VarFileType::StringID>(SLE_NAME_AND_OBJECT_ADDRESS(Sign, name), SaveLoadVersion::MinVersion, SaveLoadVersion::ReplaceCustomNameArray),
	SaveLoad::String(SLE_NAME_AND_OBJECT_ADDRESS(Sign, name), StringValidationSetting::AllowControlCode, SaveLoadVersion::ReplaceCustomNameArray),
	SaveLoad::Variable<VarFileType::I16>(SLE_NAME_AND_OBJECT_ADDRESS(Sign, x), SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),
	SaveLoad::Variable<VarFileType::I16>(SLE_NAME_AND_OBJECT_ADDRESS(Sign, y), SaveLoadVersion::MinVersion, SaveLoadVersion::BigMap),
	SaveLoad::Variable<VarFileType::I32>(SLE_NAME_AND_OBJECT_ADDRESS(Sign, x), SaveLoadVersion::BigMap),
	SaveLoad::Variable<VarFileType::I32>(SLE_NAME_AND_OBJECT_ADDRESS(Sign, y), SaveLoadVersion::BigMap),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Sign, owner), SaveLoadVersion::MultipleRoadStops),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Sign, z), SaveLoadVersion::MinVersion, SaveLoadVersion::VehicleCentreAndZPos),
	SaveLoad::Variable<VarFileType::I32>(SLE_NAME_AND_OBJECT_ADDRESS(Sign, z), SaveLoadVersion::VehicleCentreAndZPos),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Sign, text_colour), SaveLoadVersion::SignTextColours),
};

struct SIGNChunkHandler : ChunkHandler {
	SIGNChunkHandler() : ChunkHandler("SIGN", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_sign_desc);

		for (Sign *si : Sign::Iterate()) {
			SlSetArrayIndex(si->index);
			SlObject(si, _sign_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_sign_desc, _sign_sl_compat);

		int index;
		while ((index = SlIterateArray()) != -1) {
			Sign *si = Sign::CreateAtIndex(SignID(index));
			SlObject(si, slt);
			/* Before version 6.1, signs didn't have owner.
			 * Before version 83, invalid signs were determined by si->str == 0.
			 * Before version 103, owner could be a bankrupted company.
			 *  - we can't use IsValidCompany() now, so this is fixed in AfterLoadGame()
			 * All signs that were saved are valid (including those with just 'Sign' and INVALID_OWNER).
			 *  - so set owner to OWNER_NONE if needed (signs from pre-version 6.1 would be lost) */
			if (IsSavegameVersionBefore(SaveLoadVersion::MultipleRoadStops, 1) || (IsSavegameVersionBefore(SaveLoadVersion::DepotWaterOwners) && si->owner == INVALID_OWNER)) {
				si->owner = OWNER_NONE;
			}

			/* Signs placed in scenario editor shall now be OWNER_DEITY */
			if (IsSavegameVersionBefore(SaveLoadVersion::ScenarioDeitySigns) && si->owner == OWNER_NONE && _file_to_saveload.ftype.abstract == AbstractFileType::Scenario) {
				si->owner = OWNER_DEITY;
			}
		}
	}
};

static const SIGNChunkHandler SIGN;
static const ChunkHandlerRef sign_chunk_handlers[] = {
	SIGN,
};

extern const ChunkHandlerTable _sign_chunk_handlers(sign_chunk_handlers);
