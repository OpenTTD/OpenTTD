/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_sl.cpp Code handling saving and loading of industries */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/industry_sl_compat.h"

#include "../industry.h"
#include "newgrf_sl.h"

#include "../safeguards.h"

static OldPersistentStorage _old_ind_persistent_storage;

static const SaveLoad _industry_desc[] = {
	SLE_CONDVAR(Industry, location.tile,              SLE_FILE_U16 | SLE_VAR_U32,  SL_MIN_VERSION, SLV_6),
	SLE_CONDVAR(Industry, location.tile,              SLE_UINT32,                  SLV_6, SL_MAX_VERSION),
	    SLE_VAR(Industry, location.w,                 SLE_FILE_U8 | SLE_VAR_U16),
	    SLE_VAR(Industry, location.h,                 SLE_FILE_U8 | SLE_VAR_U16),
	    SLE_REF(Industry, town,                       REF_TOWN),
	SLE_CONDREF(Industry, neutral_station,            REF_STATION,                SLV_SERVE_NEUTRAL_INDUSTRIES, SL_MAX_VERSION),
	SLE_CONDARR(Industry, produced_cargo,             SLE_UINT8,   2,              SLV_78, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, produced_cargo,             SLE_UINT8,  16,             SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),
	SLE_CONDARR(Industry, incoming_cargo_waiting,     SLE_UINT16,  3,              SLV_70, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, incoming_cargo_waiting,     SLE_UINT16, 16,             SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),
	SLE_CONDARR(Industry, produced_cargo_waiting,     SLE_UINT16,  2,               SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, produced_cargo_waiting,     SLE_UINT16, 16,             SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),
	SLE_CONDARR(Industry, production_rate,            SLE_UINT8,   2,               SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, production_rate,            SLE_UINT8,  16,             SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),
	SLE_CONDARR(Industry, accepts_cargo,              SLE_UINT8,   3,              SLV_78, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, accepts_cargo,              SLE_UINT8,  16,             SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),
	    SLE_VAR(Industry, prod_level,                 SLE_UINT8),
	SLE_CONDARR(Industry, this_month_production,      SLE_UINT16,  2,               SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, this_month_production,      SLE_UINT16, 16,             SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),
	SLE_CONDARR(Industry, this_month_transported,     SLE_UINT16,  2,               SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, this_month_transported,     SLE_UINT16, 16,             SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),
	SLE_CONDARR(Industry, last_month_pct_transported, SLE_UINT8,   2,               SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, last_month_pct_transported, SLE_UINT8,  16,             SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),
	SLE_CONDARR(Industry, last_month_production,      SLE_UINT16,  2,               SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, last_month_production,      SLE_UINT16, 16,             SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),
	SLE_CONDARR(Industry, last_month_transported,     SLE_UINT16,  2,               SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, last_month_transported,     SLE_UINT16, 16,             SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),

	    SLE_VAR(Industry, counter,                    SLE_UINT16),

	    SLE_VAR(Industry, type,                       SLE_UINT8),
	    SLE_VAR(Industry, owner,                      SLE_UINT8),
	    SLE_VAR(Industry, random_colour,              SLE_UINT8),
	SLE_CONDVAR(Industry, last_prod_year,             SLE_FILE_U8 | SLE_VAR_I32,  SL_MIN_VERSION, SLV_31),
	SLE_CONDVAR(Industry, last_prod_year,             SLE_INT32,                 SLV_31, SL_MAX_VERSION),
	    SLE_VAR(Industry, was_cargo_delivered,        SLE_UINT8),
	SLE_CONDVAR(Industry, ctlflags,                   SLE_UINT8,                 SLV_GS_INDUSTRY_CONTROL, SL_MAX_VERSION),

	SLE_CONDVAR(Industry, founder,                    SLE_UINT8,                 SLV_70, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, construction_date,          SLE_INT32,                 SLV_70, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, construction_type,          SLE_UINT8,                 SLV_70, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, last_cargo_accepted_at[0],  SLE_INT32,                 SLV_70, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLE_CONDARR(Industry, last_cargo_accepted_at,     SLE_INT32, 16,            SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, selected_layout,            SLE_UINT8,                 SLV_73, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, exclusive_supplier,         SLE_UINT8,                 SLV_GS_INDUSTRY_CONTROL, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, exclusive_consumer,         SLE_UINT8,                 SLV_GS_INDUSTRY_CONTROL, SL_MAX_VERSION),

	SLEG_CONDARR("storage", _old_ind_persistent_storage.storage, SLE_UINT32, 16, SLV_76, SLV_161),
	SLE_CONDREF(Industry, psa,                        REF_STORAGE,              SLV_161, SL_MAX_VERSION),

	SLE_CONDVAR(Industry, random,                     SLE_UINT16,                SLV_82, SL_MAX_VERSION),
	SLE_CONDSSTR(Industry, text,     SLE_STR | SLF_ALLOW_CONTROL,     SLV_INDUSTRY_TEXT, SL_MAX_VERSION),
};

struct INDYChunkHandler : ChunkHandler {
	INDYChunkHandler() : ChunkHandler('INDY', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_industry_desc);

		/* Write the industries */
		for (Industry *ind : Industry::Iterate()) {
			SlSetArrayIndex(ind->index);
			SlObject(ind, _industry_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_industry_desc, _industry_sl_compat);

		int index;

		Industry::ResetIndustryCounts();

		while ((index = SlIterateArray()) != -1) {
			Industry *i = new (index) Industry();
			SlObject(i, slt);

			/* Before savegame version 161, persistent storages were not stored in a pool. */
			if (IsSavegameVersionBefore(SLV_161) && !IsSavegameVersionBefore(SLV_76)) {
				/* Store the old persistent storage. The GRFID will be added later. */
				assert(PersistentStorage::CanAllocateItem());
				i->psa = new PersistentStorage(0, 0, 0);
				memcpy(i->psa->storage, _old_ind_persistent_storage.storage, sizeof(_old_ind_persistent_storage.storage));
			}
			Industry::IncIndustryTypeCount(i->type);
		}
	}

	void FixPointers() const override
	{
		for (Industry *i : Industry::Iterate()) {
			SlObject(i, _industry_desc);
		}
	}
};

struct IIDSChunkHandler : NewGRFMappingChunkHandler {
	IIDSChunkHandler() : NewGRFMappingChunkHandler('IIDS', _industry_mngr) {}
};

struct TIDSChunkHandler : NewGRFMappingChunkHandler {
	TIDSChunkHandler() : NewGRFMappingChunkHandler('TIDS', _industile_mngr) {}
};

/** Description of the data to save and load in #IndustryBuildData. */
static const SaveLoad _industry_builder_desc[] = {
	SLEG_VAR("wanted_inds", _industry_builder.wanted_inds, SLE_UINT32),
};

/** Industry builder. */
struct IBLDChunkHandler : ChunkHandler {
	IBLDChunkHandler() : ChunkHandler('IBLD', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_industry_builder_desc);

		SlSetArrayIndex(0);
		SlGlobList(_industry_builder_desc);
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_industry_builder_desc, _industry_builder_sl_compat);

		if (!IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY) && SlIterateArray() == -1) return;
		SlGlobList(slt);
		if (!IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY) && SlIterateArray() != -1) SlErrorCorrupt("Too many IBLD entries");
	}
};

/** Description of the data to save and load in #IndustryTypeBuildData. */
static const SaveLoad _industrytype_builder_desc[] = {
	SLE_VAR(IndustryTypeBuildData, probability,  SLE_UINT32),
	SLE_VAR(IndustryTypeBuildData, min_number,   SLE_UINT8),
	SLE_VAR(IndustryTypeBuildData, target_count, SLE_UINT16),
	SLE_VAR(IndustryTypeBuildData, max_wait,     SLE_UINT16),
	SLE_VAR(IndustryTypeBuildData, wait_count,   SLE_UINT16),
};

/** Industry-type build data. */
struct ITBLChunkHandler : ChunkHandler {
	ITBLChunkHandler() : ChunkHandler('ITBL', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_industrytype_builder_desc);

		for (int i = 0; i < NUM_INDUSTRYTYPES; i++) {
			SlSetArrayIndex(i);
			SlObject(_industry_builder.builddata + i, _industrytype_builder_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_industrytype_builder_desc, _industrytype_builder_sl_compat);

		for (IndustryType it = 0; it < NUM_INDUSTRYTYPES; it++) {
			_industry_builder.builddata[it].Reset();
		}
		int index;
		while ((index = SlIterateArray()) != -1) {
			if ((uint)index >= NUM_INDUSTRYTYPES) SlErrorCorrupt("Too many industry builder datas");
			SlObject(_industry_builder.builddata + index, slt);
		}
	}
};

static const INDYChunkHandler INDY;
static const IIDSChunkHandler IIDS;
static const TIDSChunkHandler TIDS;
static const IBLDChunkHandler IBLD;
static const ITBLChunkHandler ITBL;
static const ChunkHandlerRef industry_chunk_handlers[] = {
	INDY,
	IIDS,
	TIDS,
	IBLD,
	ITBL,
};

extern const ChunkHandlerTable _industry_chunk_handlers(industry_chunk_handlers);
