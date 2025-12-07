/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_sl.cpp Code handling saving and loading of industries */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/industry_sl_compat.h"

#include "../industry.h"
#include "newgrf_sl.h"

#include "../safeguards.h"

static OldPersistentStorage _old_ind_persistent_storage;

class SlIndustryAcceptedHistory : public DefaultSaveLoadHandler<SlIndustryAcceptedHistory, Industry::AcceptedCargo> {
public:
	static inline const SaveLoad description[] = {
		 SLE_VAR(Industry::AcceptedHistory, accepted, SLE_UINT16),
		 SLE_VAR(Industry::AcceptedHistory, waiting, SLE_UINT16),
	};
	static inline const SaveLoadCompatTable compat_description = _industry_produced_history_sl_compat;

	void Save(Industry::AcceptedCargo *a) const override
	{
		if (!IsValidCargoType(a->cargo) || a->history == nullptr) {
			/* Don't save any history if cargo slot isn't used. */
			SlSetStructListLength(0);
			return;
		}

		SlSetStructListLength(a->history->size());

		for (auto &h : *a->history) {
			SlObject(&h, this->GetDescription());
		}
	}

	void Load(Industry::AcceptedCargo *a) const override
	{
		size_t len = SlGetStructListLength(UINT32_MAX);
		if (len == 0) return;

		auto &history = a->GetOrCreateHistory();
		for (auto &h : history) {
			if (--len > history.size()) break; // unsigned so wraps after hitting zero.
			SlObject(&h, this->GetLoadDescription());
		}
	}
};

class SlIndustryAccepted : public VectorSaveLoadHandler<SlIndustryAccepted, Industry, Industry::AcceptedCargo, INDUSTRY_NUM_INPUTS> {
public:
	static inline const SaveLoad description[] = {
		 SLE_VAR(Industry::AcceptedCargo, cargo, SLE_UINT8),
		 SLE_VAR(Industry::AcceptedCargo, waiting, SLE_UINT16),
		 SLE_VAR(Industry::AcceptedCargo, last_accepted, SLE_INT32),
		SLE_CONDVAR(Industry::AcceptedCargo, accumulated_waiting, SLE_UINT32, SLV_INDUSTRY_ACCEPTED_HISTORY, SL_MAX_VERSION),
		SLEG_CONDSTRUCTLIST("history", SlIndustryAcceptedHistory, SLV_INDUSTRY_ACCEPTED_HISTORY, SL_MAX_VERSION),
	};
	static inline const SaveLoadCompatTable compat_description = _industry_accepts_sl_compat;

	std::vector<Industry::AcceptedCargo> &GetVector(Industry *i) const override { return i->accepted; }

	/* Old array structure used by INDYChunkHandler for savegames before SLV_INDUSTRY_CARGO_REORGANISE. */
	static inline std::array<CargoType, INDUSTRY_NUM_INPUTS> old_cargo;
	static inline std::array<uint16_t, INDUSTRY_NUM_INPUTS> old_waiting;
	static inline std::array<TimerGameEconomy::Date, INDUSTRY_NUM_INPUTS> old_last_accepted;

	static void ResetOldStructure()
	{
		SlIndustryAccepted::old_cargo.fill(INVALID_CARGO);
		SlIndustryAccepted::old_waiting.fill(0);
		SlIndustryAccepted::old_last_accepted.fill(TimerGameEconomy::Date{});
	}
};

class SlIndustryProducedHistory : public DefaultSaveLoadHandler<SlIndustryProducedHistory, Industry::ProducedCargo> {
public:
	static inline const SaveLoad description[] = {
		 SLE_VAR(Industry::ProducedHistory, production, SLE_UINT16),
		 SLE_VAR(Industry::ProducedHistory, transported, SLE_UINT16),
	};
	static inline const SaveLoadCompatTable compat_description = _industry_produced_history_sl_compat;

	void Save(Industry::ProducedCargo *p) const override
	{
		if (!IsValidCargoType(p->cargo)) {
			/* Don't save any history if cargo slot isn't used. */
			SlSetStructListLength(0);
			return;
		}

		SlSetStructListLength(p->history.size());

		for (auto &h : p->history) {
			SlObject(&h, this->GetDescription());
		}
	}

	void Load(Industry::ProducedCargo *p) const override
	{
		size_t len = SlGetStructListLength(p->history.size());

		for (auto &h : p->history) {
			if (--len > p->history.size()) break; // unsigned so wraps after hitting zero.
			SlObject(&h, this->GetLoadDescription());
		}
	}
};

class SlIndustryProduced : public VectorSaveLoadHandler<SlIndustryProduced, Industry, Industry::ProducedCargo, INDUSTRY_NUM_OUTPUTS> {
public:
	static inline const SaveLoad description[] = {
		 SLE_VAR(Industry::ProducedCargo, cargo, SLE_UINT8),
		 SLE_VAR(Industry::ProducedCargo, waiting, SLE_UINT16),
		 SLE_VAR(Industry::ProducedCargo, rate, SLE_UINT8),
		SLEG_STRUCTLIST("history", SlIndustryProducedHistory),
	};
	static inline const SaveLoadCompatTable compat_description = _industry_produced_sl_compat;

	std::vector<Industry::ProducedCargo> &GetVector(Industry *i) const override { return i->produced; }

	/* Old array structure used by INDYChunkHandler for savegames before SLV_INDUSTRY_CARGO_REORGANISE. */
	static inline std::array<CargoType, INDUSTRY_NUM_OUTPUTS> old_cargo;
	static inline std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> old_waiting;
	static inline std::array<uint8_t, INDUSTRY_NUM_OUTPUTS> old_rate;
	static inline std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> old_this_month_production;
	static inline std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> old_this_month_transported;
	static inline std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> old_last_month_production;
	static inline std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> old_last_month_transported;

	static void ResetOldStructure()
	{
		SlIndustryProduced::old_cargo.fill(INVALID_CARGO);
		SlIndustryProduced::old_waiting.fill(0);
		SlIndustryProduced::old_rate.fill(0);
		SlIndustryProduced::old_this_month_production.fill(0);
		SlIndustryProduced::old_this_month_transported.fill(0);
		SlIndustryProduced::old_last_month_production.fill(0);
		SlIndustryProduced::old_this_month_production.fill(0);
	}
};

static const SaveLoad _industry_desc[] = {
	SLE_CONDVAR(Industry, location.tile,              SLE_FILE_U16 | SLE_VAR_U32,  SL_MIN_VERSION, SLV_6),
	SLE_CONDVAR(Industry, location.tile,              SLE_UINT32,                  SLV_6, SL_MAX_VERSION),
	    SLE_VAR(Industry, location.w,                 SLE_FILE_U8 | SLE_VAR_U16),
	    SLE_VAR(Industry, location.h,                 SLE_FILE_U8 | SLE_VAR_U16),
	    SLE_REF(Industry, town,                       REF_TOWN),
	SLE_CONDREF(Industry, neutral_station,            REF_STATION,                SLV_SERVE_NEUTRAL_INDUSTRIES, SL_MAX_VERSION),
	SLEG_CONDARR("produced_cargo",             SlIndustryProduced::old_cargo,                  SLE_UINT8,  INDUSTRY_ORIGINAL_NUM_OUTPUTS, SLV_78, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLEG_CONDARR("produced_cargo",             SlIndustryProduced::old_cargo,                  SLE_UINT8,  INDUSTRY_NUM_OUTPUTS, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),
	SLEG_CONDARR("incoming_cargo_waiting",     SlIndustryAccepted::old_waiting,                SLE_UINT16, INDUSTRY_ORIGINAL_NUM_INPUTS, SLV_70, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLEG_CONDARR("incoming_cargo_waiting",     SlIndustryAccepted::old_waiting,                SLE_UINT16, INDUSTRY_NUM_INPUTS, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),
	SLEG_CONDARR("produced_cargo_waiting",     SlIndustryProduced::old_waiting,                SLE_UINT16, INDUSTRY_ORIGINAL_NUM_OUTPUTS, SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLEG_CONDARR("produced_cargo_waiting",     SlIndustryProduced::old_waiting,                SLE_UINT16, INDUSTRY_NUM_OUTPUTS, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),
	SLEG_CONDARR("production_rate",            SlIndustryProduced::old_rate,                   SLE_UINT8,  INDUSTRY_ORIGINAL_NUM_OUTPUTS, SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLEG_CONDARR("production_rate",            SlIndustryProduced::old_rate,                   SLE_UINT8,  INDUSTRY_NUM_OUTPUTS, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),
	SLEG_CONDARR("accepts_cargo",              SlIndustryAccepted::old_cargo,                  SLE_UINT8,  INDUSTRY_ORIGINAL_NUM_INPUTS, SLV_78, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLEG_CONDARR("accepts_cargo",              SlIndustryAccepted::old_cargo,                  SLE_UINT8,  INDUSTRY_NUM_INPUTS, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),
	    SLE_VAR(Industry, prod_level,                 SLE_UINT8),
	SLEG_CONDARR("this_month_production",      SlIndustryProduced::old_this_month_production,  SLE_UINT16, INDUSTRY_ORIGINAL_NUM_OUTPUTS, SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLEG_CONDARR("this_month_production",      SlIndustryProduced::old_this_month_production,  SLE_UINT16, INDUSTRY_NUM_OUTPUTS, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),
	SLEG_CONDARR("this_month_transported",     SlIndustryProduced::old_this_month_transported, SLE_UINT16, INDUSTRY_ORIGINAL_NUM_OUTPUTS, SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLEG_CONDARR("this_month_transported",     SlIndustryProduced::old_this_month_transported, SLE_UINT16, INDUSTRY_NUM_OUTPUTS, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),
	SLEG_CONDARR("last_month_production",      SlIndustryProduced::old_last_month_production,  SLE_UINT16, INDUSTRY_ORIGINAL_NUM_OUTPUTS, SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLEG_CONDARR("last_month_production",      SlIndustryProduced::old_last_month_production,  SLE_UINT16, INDUSTRY_NUM_OUTPUTS, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),
	SLEG_CONDARR("last_month_transported",     SlIndustryProduced::old_last_month_transported, SLE_UINT16, INDUSTRY_ORIGINAL_NUM_OUTPUTS, SL_MIN_VERSION, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLEG_CONDARR("last_month_transported",     SlIndustryProduced::old_last_month_transported, SLE_UINT16, INDUSTRY_NUM_OUTPUTS, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),

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
	SLEG_CONDVAR("last_cargo_accepted_at[0]",  SlIndustryAccepted::old_last_accepted[0], SLE_INT32,     SLV_70, SLV_EXTEND_INDUSTRY_CARGO_SLOTS),
	SLEG_CONDARR("last_cargo_accepted_at",     SlIndustryAccepted::old_last_accepted,    SLE_INT32, 16, SLV_EXTEND_INDUSTRY_CARGO_SLOTS, SLV_INDUSTRY_CARGO_REORGANISE),
	SLE_CONDVAR(Industry, selected_layout,            SLE_UINT8,                 SLV_73, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, exclusive_supplier,         SLE_UINT8,                 SLV_GS_INDUSTRY_CONTROL, SL_MAX_VERSION),
	SLE_CONDVAR(Industry, exclusive_consumer,         SLE_UINT8,                 SLV_GS_INDUSTRY_CONTROL, SL_MAX_VERSION),

	SLEG_CONDARR("storage", _old_ind_persistent_storage.storage, SLE_UINT32, 16, SLV_76, SLV_161),
	SLE_CONDREF(Industry, psa,                        REF_STORAGE,              SLV_161, SL_MAX_VERSION),

	SLE_CONDVAR(Industry, random,                     SLE_UINT16,                SLV_82, SL_MAX_VERSION),
	SLE_CONDSSTR(Industry, text,     SLE_STR | SLF_ALLOW_CONTROL,     SLV_INDUSTRY_TEXT, SL_MAX_VERSION),

	SLE_CONDVAR(Industry, valid_history, SLE_UINT64, SLV_INDUSTRY_NUM_VALID_HISTORY, SL_MAX_VERSION),

	SLEG_CONDSTRUCTLIST("accepted", SlIndustryAccepted,                          SLV_INDUSTRY_CARGO_REORGANISE, SL_MAX_VERSION),
	SLEG_CONDSTRUCTLIST("produced", SlIndustryProduced,                          SLV_INDUSTRY_CARGO_REORGANISE, SL_MAX_VERSION),
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

	void LoadMoveAcceptsProduced(Industry *i, uint inputs, uint outputs) const
	{
		i->accepted.reserve(inputs);
		for (uint j = 0; j != inputs; ++j) {
			auto &a = i->accepted.emplace_back();
			a.cargo = SlIndustryAccepted::old_cargo[j];
			a.waiting = SlIndustryAccepted::old_waiting[j];
			a.last_accepted = SlIndustryAccepted::old_last_accepted[j];
		}

		i->produced.reserve(outputs);
		for (uint j = 0; j != outputs; ++j) {
			auto &p = i->produced.emplace_back();
			p.cargo = SlIndustryProduced::old_cargo[j];
			p.waiting = SlIndustryProduced::old_waiting[j];
			p.rate = SlIndustryProduced::old_rate[j];
			p.history[THIS_MONTH].production = SlIndustryProduced::old_this_month_production[j];
			p.history[THIS_MONTH].transported = SlIndustryProduced::old_this_month_transported[j];
			p.history[LAST_MONTH].production = SlIndustryProduced::old_last_month_production[j];
			p.history[LAST_MONTH].transported = SlIndustryProduced::old_last_month_transported[j];
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_industry_desc, _industry_sl_compat);

		int index;

		SlIndustryAccepted::ResetOldStructure();
		SlIndustryProduced::ResetOldStructure();

		while ((index = SlIterateArray()) != -1) {
			Industry *i = new (IndustryID(index)) Industry();
			SlObject(i, slt);

			/* Before savegame version 161, persistent storages were not stored in a pool. */
			if (IsSavegameVersionBefore(SLV_161) && !IsSavegameVersionBefore(SLV_76)) {
				/* Store the old persistent storage. The GRFID will be added later. */
				assert(PersistentStorage::CanAllocateItem());
				i->psa = new PersistentStorage(0, GSF_INVALID, TileIndex{});
				std::copy(std::begin(_old_ind_persistent_storage.storage), std::end(_old_ind_persistent_storage.storage), std::begin(i->psa->storage));
			}
			if (IsSavegameVersionBefore(SLV_EXTEND_INDUSTRY_CARGO_SLOTS)) {
				LoadMoveAcceptsProduced(i, INDUSTRY_ORIGINAL_NUM_INPUTS, INDUSTRY_ORIGINAL_NUM_OUTPUTS);
			} else if (IsSavegameVersionBefore(SLV_INDUSTRY_CARGO_REORGANISE)) {
				LoadMoveAcceptsProduced(i, INDUSTRY_NUM_INPUTS, INDUSTRY_NUM_OUTPUTS);
			}

			if (IsSavegameVersionBefore(SLV_INDUSTRY_NUM_VALID_HISTORY)) {
				/* The last month has always been recorded. */
				size_t oldest_valid = LAST_MONTH;
				if (!IsSavegameVersionBefore(SLV_PRODUCTION_HISTORY)) {
					/* History was extended but we did not keep track of valid history, so assume it from the oldest non-zero value. */
					for (const auto &p : i->produced) {
						if (!IsValidCargoType(p.cargo)) continue;
						for (size_t n = LAST_MONTH; n < std::size(p.history); ++n) {
							if (p.history[n].production == 0 && p.history[n].transported == 0) continue;
							oldest_valid = std::max(oldest_valid, n);
						}
					}
				}
				/* Set mask bits up to and including the oldest valid record. */
				i->valid_history = (std::numeric_limits<uint64_t>::max() >> (std::numeric_limits<uint64_t>::digits - (oldest_valid + 1 - LAST_MONTH))) << LAST_MONTH;
			}

			Industry::industries[i->type].insert(i->index);
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
