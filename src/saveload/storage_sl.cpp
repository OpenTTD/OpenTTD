/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file storage_sl.cpp Code handling saving and loading of persistent storages. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/storage_sl_compat.h"

#include "../newgrf_storage.h"

#include "../safeguards.h"

/**
 * Convert old fixed-sized array of persistent storage.
 * @param old_storage Span containing range of old persistent storage.
 * @returns Pointer to pool-allocated PersistentStorage object.
 */
PersistentStorage *ConvertOldPersistentStorage(std::span<const PersistentStorage::ElementType> old_storage)
{
	auto last = std::find_if(std::rbegin(old_storage), std::rend(old_storage), [](uint32_t value) { return value != 0; }).base();
	if (last == std::begin(old_storage)) return nullptr;

	assert(PersistentStorage::CanAllocateItem());
	PersistentStorage *psa = new PersistentStorage(0, 0, 0);

	for (auto it = std::begin(old_storage); it != last; ++it) {
		if (*it == 0) continue;
		psa->storage.emplace(std::distance(std::begin(old_storage), it), *it);
	}

	return psa;
}

class SlPersistentStorage : public DefaultSaveLoadHandler<SlPersistentStorage, PersistentStorage> {
public:
	using PairType = std::pair<PersistentStorage::KeyType, PersistentStorage::ElementType>;

	inline static const SaveLoad description[] = {
		SLE_VAR(PairType, first, SLE_UINT16),
		SLE_VAR(PairType, second, SLE_INT32),
	};
	inline const static SaveLoadCompatTable compat_description = {};

	void Save(PersistentStorage *psa) const override
	{
		SlSetStructListLength(psa->storage.size());
		for (auto &pair : psa->storage) {
			SlObject(&pair, this->GetDescription());
		}
	}

	void Load(PersistentStorage *psa) const override
	{
		size_t size = SlGetStructListLength(UINT32_MAX);
		for (size_t i = 0; i < size; ++i) {
			PairType pair;
			SlObject(&pair, this->GetDescription());
			if (pair.second == 0) continue;
			psa->storage.emplace(std::move(pair));
		}
	}
};

/** Old persistent storage was a fixed array of up to 256 elements. */
static std::array<PersistentStorage::ElementType, 256> _old_persistent_storage;

/** Description of the data to save and load in #PersistentStorage. */
static const SaveLoad _storage_desc[] = {
	 SLE_CONDVAR(PersistentStorage, grfid,    SLE_UINT32,                  SLV_6, SL_MAX_VERSION),
	 SLEG_CONDARR("storage", _old_persistent_storage, SLE_UINT32,  16, SLV_161, SLV_EXTEND_PERSISTENT_STORAGE),
	 SLEG_CONDARR("storage", _old_persistent_storage, SLE_UINT32, 256, SLV_EXTEND_PERSISTENT_STORAGE, SLV_VARIABLE_PERSISTENT_STORAGE),
	SLEG_CONDSTRUCTLIST("storage", SlPersistentStorage, SLV_VARIABLE_PERSISTENT_STORAGE, SL_MAX_VERSION),
};

/** Persistent storage data. */
struct PSACChunkHandler : ChunkHandler {
	PSACChunkHandler() : ChunkHandler('PSAC', CH_TABLE) {}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_storage_desc, _storage_sl_compat);

		_old_persistent_storage.fill(0);
		int index;

		while ((index = SlIterateArray()) != -1) {
			assert(PersistentStorage::CanAllocateItem());
			PersistentStorage *ps = new (index) PersistentStorage(0, 0, 0);
			SlObject(ps, slt);

			if (IsSavegameVersionBefore(SLV_VARIABLE_PERSISTENT_STORAGE)) {
				auto last = std::find_if(std::rbegin(_old_persistent_storage), std::rend(_old_persistent_storage), [](uint32_t value) { return value != 0; }).base();
				for (auto it = std::begin(_old_persistent_storage); it != last; ++it) {
					if (*it == 0) continue;
					ps->storage.emplace(std::distance(std::begin(_old_persistent_storage), it), *it);
				}
			}
		}
	}

	void Save() const override
	{
		SlTableHeader(_storage_desc);

		/* Write the industries */
		for (PersistentStorage *ps : PersistentStorage::Iterate()) {
			ps->ClearChanges();
			SlSetArrayIndex(ps->index);
			SlObject(ps, _storage_desc);
		}
	}
};

static const PSACChunkHandler PSAC;
static const ChunkHandlerRef persistent_storage_chunk_handlers[] = {
	PSAC,
};

extern const ChunkHandlerTable _persistent_storage_chunk_handlers(persistent_storage_chunk_handlers);
