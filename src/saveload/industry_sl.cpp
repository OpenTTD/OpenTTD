/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_sl.cpp Code handling saving and loading of industries. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/industry_sl_compat.h"

#include "../industry.h"
#include "../town.h"
#include "newgrf_sl.h"

#include "../safeguards.h"

static OldPersistentStorage _old_ind_persistent_storage;

class SlIndustryAcceptedHistory : public DefaultSaveLoadHandler<SlIndustryAcceptedHistory, Industry::AcceptedCargo> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Industry::AcceptedHistory, accepted)),
		SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Industry::AcceptedHistory, waiting)),
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
		SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Industry::AcceptedCargo, cargo)),
		SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Industry::AcceptedCargo, waiting)),
		SaveLoad::Variable<VarFileType::I32>(SLE_NAME_AND_OBJECT_ADDRESS(Industry::AcceptedCargo, last_accepted)),
		SLE_CONDVAR(Industry::AcceptedCargo, accumulated_waiting, VarTypes::U32, SaveLoadVersion::IndustryAcceptedHistory, SaveLoadVersion::MaxVersion),
		SaveLoad::StructList<SlIndustryAcceptedHistory>("history", SaveLoadVersion::IndustryAcceptedHistory),
	};
	static inline const SaveLoadCompatTable compat_description = _industry_accepts_sl_compat;

	std::vector<Industry::AcceptedCargo> &GetVector(Industry *i) const override { return i->accepted; }

	/** @{
	 * Old array structures used by INDYChunkHandler for savegames before SaveLoadVersion::IndustryCargoReorganise. */
	static inline std::array<CargoType, INDUSTRY_NUM_INPUTS> old_cargo;
	static inline std::array<uint16_t, INDUSTRY_NUM_INPUTS> old_waiting;
	static inline std::array<TimerGameEconomy::Date, INDUSTRY_NUM_INPUTS> old_last_accepted;
	/** @} */

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
		SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Industry::ProducedHistory, production)),
		SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Industry::ProducedHistory, transported)),
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
		SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Industry::ProducedCargo, cargo)),
		SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Industry::ProducedCargo, waiting)),
		SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Industry::ProducedCargo, rate)),
		SaveLoad::StructList<SlIndustryProducedHistory>("history"),
	};
	static inline const SaveLoadCompatTable compat_description = _industry_produced_sl_compat;

	std::vector<Industry::ProducedCargo> &GetVector(Industry *i) const override { return i->produced; }

	/** @{
	 * Old array structures used by INDYChunkHandler for savegames before SaveLoadVersion::IndustryCargoReorganise. */
	static inline std::array<CargoType, INDUSTRY_NUM_OUTPUTS> old_cargo;
	static inline std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> old_waiting;
	static inline std::array<uint8_t, INDUSTRY_NUM_OUTPUTS> old_rate;
	static inline std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> old_this_month_production;
	static inline std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> old_this_month_transported;
	static inline std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> old_last_month_production;
	static inline std::array<uint16_t, INDUSTRY_NUM_OUTPUTS> old_last_month_transported;
	/** @} */

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
	SLE_CONDVAR(Industry, location.tile, VarFileType::U16 | VarMemType::U32, SaveLoadVersion::MinVersion, SaveLoadVersion::MultipleRoadStops),
	SLE_CONDVAR(Industry, location.tile, VarTypes::U32, SaveLoadVersion::MultipleRoadStops, SaveLoadVersion::MaxVersion),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, location.w)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, location.h)),
	SaveLoad::Reference<SLRefType::Town>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, town)),
	SaveLoad::Reference<SLRefType::Station>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, neutral_station), SaveLoadVersion::ServeNeutralIndustries),
	SaveLoad::Array<VarFileType::U8, INDUSTRY_ORIGINAL_NUM_OUTPUTS>("produced_cargo", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_cargo), SaveLoadVersion::StoreIndustryCargo, SaveLoadVersion::ExtendIndustryCargoSlots),
	SaveLoad::Array<VarFileType::U8, INDUSTRY_NUM_OUTPUTS>("produced_cargo", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_cargo), SaveLoadVersion::ExtendIndustryCargoSlots, SaveLoadVersion::IndustryCargoReorganise),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_ORIGINAL_NUM_INPUTS>("incoming_cargo_waiting", SLE_GLOBAL_ADDRESS(SlIndustryAccepted::old_waiting), SaveLoadVersion::CargoPaymentOverflow, SaveLoadVersion::ExtendIndustryCargoSlots),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_NUM_INPUTS>("incoming_cargo_waiting", SLE_GLOBAL_ADDRESS(SlIndustryAccepted::old_waiting), SaveLoadVersion::ExtendIndustryCargoSlots, SaveLoadVersion::IndustryCargoReorganise),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_ORIGINAL_NUM_OUTPUTS>("produced_cargo_waiting", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_waiting), SaveLoadVersion::MinVersion, SaveLoadVersion::ExtendIndustryCargoSlots),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_NUM_OUTPUTS>("produced_cargo_waiting", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_waiting), SaveLoadVersion::ExtendIndustryCargoSlots, SaveLoadVersion::IndustryCargoReorganise),
	SaveLoad::Array<VarFileType::U8, INDUSTRY_ORIGINAL_NUM_OUTPUTS>("production_rate", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_rate), SaveLoadVersion::MinVersion, SaveLoadVersion::ExtendIndustryCargoSlots),
	SaveLoad::Array<VarFileType::U8, INDUSTRY_NUM_OUTPUTS>("production_rate", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_rate), SaveLoadVersion::ExtendIndustryCargoSlots, SaveLoadVersion::IndustryCargoReorganise),
	SaveLoad::Array<VarFileType::U8, INDUSTRY_ORIGINAL_NUM_INPUTS>("accepts_cargo", SLE_GLOBAL_ADDRESS(SlIndustryAccepted::old_cargo), SaveLoadVersion::StoreIndustryCargo, SaveLoadVersion::ExtendIndustryCargoSlots),
	SaveLoad::Array<VarFileType::U8, INDUSTRY_NUM_INPUTS>("accepts_cargo", SLE_GLOBAL_ADDRESS(SlIndustryAccepted::old_cargo), SaveLoadVersion::ExtendIndustryCargoSlots, SaveLoadVersion::IndustryCargoReorganise),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, prod_level)),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_ORIGINAL_NUM_OUTPUTS>("this_month_production", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_this_month_production), SaveLoadVersion::MinVersion, SaveLoadVersion::ExtendIndustryCargoSlots),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_NUM_OUTPUTS>("this_month_production", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_this_month_production), SaveLoadVersion::ExtendIndustryCargoSlots, SaveLoadVersion::IndustryCargoReorganise),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_ORIGINAL_NUM_OUTPUTS>("this_month_transported", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_this_month_transported), SaveLoadVersion::MinVersion, SaveLoadVersion::ExtendIndustryCargoSlots),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_NUM_OUTPUTS>("this_month_transported", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_this_month_transported), SaveLoadVersion::ExtendIndustryCargoSlots, SaveLoadVersion::IndustryCargoReorganise),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_ORIGINAL_NUM_OUTPUTS>("last_month_production", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_last_month_production), SaveLoadVersion::MinVersion, SaveLoadVersion::ExtendIndustryCargoSlots),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_NUM_OUTPUTS>("last_month_production", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_last_month_production), SaveLoadVersion::ExtendIndustryCargoSlots, SaveLoadVersion::IndustryCargoReorganise),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_ORIGINAL_NUM_OUTPUTS>("last_month_transported", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_last_month_transported), SaveLoadVersion::MinVersion, SaveLoadVersion::ExtendIndustryCargoSlots),
	SaveLoad::Array<VarFileType::U16, INDUSTRY_NUM_OUTPUTS>("last_month_transported", SLE_GLOBAL_ADDRESS(SlIndustryProduced::old_last_month_transported), SaveLoadVersion::ExtendIndustryCargoSlots, SaveLoadVersion::IndustryCargoReorganise),

	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, counter)),

	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, type)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, owner)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, random_colour)),
	SLE_CONDVAR(Industry, last_prod_year, VarFileType::U8 | VarMemType::I32, SaveLoadVersion::MinVersion, SaveLoadVersion::BigDates),
	SLE_CONDVAR(Industry, last_prod_year, VarTypes::I32, SaveLoadVersion::BigDates, SaveLoadVersion::MaxVersion),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, was_cargo_delivered)),
	SLE_CONDVAR(Industry, ctlflags, VarTypes::U8, SaveLoadVersion::GSIndustryControl, SaveLoadVersion::MaxVersion),

	SLE_CONDVAR(Industry, founder, VarTypes::U8, SaveLoadVersion::CargoPaymentOverflow, SaveLoadVersion::MaxVersion),
	SLE_CONDVAR(Industry, construction_date, VarTypes::I32, SaveLoadVersion::CargoPaymentOverflow, SaveLoadVersion::MaxVersion),
	SLE_CONDVAR(Industry, construction_type, VarTypes::U8, SaveLoadVersion::CargoPaymentOverflow, SaveLoadVersion::MaxVersion),
	SaveLoad::Variable<VarFileType::I32>("last_cargo_accepted_at[0]", SLE_GLOBAL_ADDRESS(SlIndustryAccepted::old_last_accepted[0]), SaveLoadVersion::CargoPaymentOverflow, SaveLoadVersion::ExtendIndustryCargoSlots),
	SaveLoad::Array<VarFileType::I32, 16>("last_cargo_accepted_at", SLE_GLOBAL_ADDRESS(SlIndustryAccepted::old_last_accepted), SaveLoadVersion::ExtendIndustryCargoSlots, SaveLoadVersion::IndustryCargoReorganise),
	SLE_CONDVAR(Industry, selected_layout, VarTypes::U8, SaveLoadVersion::NewGRFIndustryLayout, SaveLoadVersion::MaxVersion),
	SLE_CONDVAR(Industry, exclusive_supplier, VarTypes::U8, SaveLoadVersion::GSIndustryControl, SaveLoadVersion::MaxVersion),
	SLE_CONDVAR(Industry, exclusive_consumer, VarTypes::U8, SaveLoadVersion::GSIndustryControl, SaveLoadVersion::MaxVersion),

	SaveLoad::Array<VarFileType::U32, 16>("storage", SLE_GLOBAL_ADDRESS(_old_ind_persistent_storage.storage), SaveLoadVersion::NewGRFPersistentStorage, SaveLoadVersion::PersistentStoragePool),
	SaveLoad::Reference<SLRefType::Storage>(SLE_NAME_AND_OBJECT_ADDRESS(Industry, psa), SaveLoadVersion::PersistentStoragePool),

	SLE_CONDVAR(Industry, random, VarTypes::U16, SaveLoadVersion::NewGRFIndustryRandomTriggers, SaveLoadVersion::MaxVersion),
	SaveLoad::String(SLE_NAME_AND_OBJECT_ADDRESS(Industry, text), StringValidationSetting::AllowControlCode, SaveLoadVersion::IndustryText),

	SLE_CONDVAR(Industry, valid_history, VarTypes::U64, SaveLoadVersion::IndustryNumValidHistory, SaveLoadVersion::MaxVersion),

	SaveLoad::StructList<SlIndustryAccepted>("accepted", SaveLoadVersion::IndustryCargoReorganise),
	SaveLoad::StructList<SlIndustryProduced>("produced", SaveLoadVersion::IndustryCargoReorganise),
};

struct INDYChunkHandler : ChunkHandler {
	INDYChunkHandler() : ChunkHandler("INDY", ChunkType::Table) {}

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
			Industry *i = Industry::CreateAtIndex(IndustryID(index));
			SlObject(i, slt);

			/* Before savegame version 161, persistent storages were not stored in a pool. */
			if (IsSavegameVersionBefore(SaveLoadVersion::PersistentStoragePool) && !IsSavegameVersionBefore(SaveLoadVersion::NewGRFPersistentStorage)) {
				/* Store the old persistent storage. The GRFID will be added later. */
				assert(PersistentStorage::CanAllocateItem());
				i->psa = PersistentStorage::Create(GrfID{}, GrfSpecFeature::Invalid, TileIndex{});
				std::copy(std::begin(_old_ind_persistent_storage.storage), std::end(_old_ind_persistent_storage.storage), std::begin(i->psa->storage));
			}
			if (IsSavegameVersionBefore(SaveLoadVersion::ExtendIndustryCargoSlots)) {
				LoadMoveAcceptsProduced(i, INDUSTRY_ORIGINAL_NUM_INPUTS, INDUSTRY_ORIGINAL_NUM_OUTPUTS);
			} else if (IsSavegameVersionBefore(SaveLoadVersion::IndustryCargoReorganise)) {
				LoadMoveAcceptsProduced(i, INDUSTRY_NUM_INPUTS, INDUSTRY_NUM_OUTPUTS);
			}

			if (IsSavegameVersionBefore(SaveLoadVersion::IndustryNumValidHistory)) {
				/* The last month has always been recorded. */
				size_t oldest_valid = LAST_MONTH;
				if (!IsSavegameVersionBefore(SaveLoadVersion::ProductionHistory)) {
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
	IIDSChunkHandler() : NewGRFMappingChunkHandler("IIDS", _industry_mngr) {}
};

struct TIDSChunkHandler : NewGRFMappingChunkHandler {
	TIDSChunkHandler() : NewGRFMappingChunkHandler("TIDS", _industile_mngr) {}
};

/** Description of the data to save and load in #IndustryBuildData. */
static const SaveLoad _industry_builder_desc[] = {
	SaveLoad::Variable<VarFileType::U32>("wanted_inds", SLE_GLOBAL_ADDRESS(_industry_builder.wanted_inds)),
};

/** Industry builder. */
struct IBLDChunkHandler : ChunkHandler {
	IBLDChunkHandler() : ChunkHandler("IBLD", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_industry_builder_desc);

		SlSetArrayIndex(0);
		SlGlobList(_industry_builder_desc);
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_industry_builder_desc, _industry_builder_sl_compat);

		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() == -1) return;
		SlGlobList(slt);
		if (!IsSavegameVersionBefore(SaveLoadVersion::RiffToArray) && SlIterateArray() != -1) SlErrorCorrupt("Too many IBLD entries");
	}
};

/** Description of the data to save and load in #IndustryTypeBuildData. */
static const SaveLoad _industrytype_builder_desc[] = {
	SaveLoad::Variable<VarFileType::U32>(SLE_NAME_AND_OBJECT_ADDRESS(IndustryTypeBuildData, probability)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(IndustryTypeBuildData, min_number)),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(IndustryTypeBuildData, target_count)),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(IndustryTypeBuildData, max_wait)),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(IndustryTypeBuildData, wait_count)),
};

/** Industry-type build data. */
struct ITBLChunkHandler : ChunkHandler {
	ITBLChunkHandler() : ChunkHandler("ITBL", ChunkType::Table) {}

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
