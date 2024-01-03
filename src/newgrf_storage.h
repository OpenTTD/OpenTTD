/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_storage.h Functionality related to the temporary and persistent storage arrays for NewGRFs. */

#ifndef NEWGRF_STORAGE_H
#define NEWGRF_STORAGE_H

#include "core/alloc_func.hpp"
#include "core/pool_type.hpp"
#include "tile_type.h"

/**
 * Mode switches to the behaviour of persistent storage array.
 */
enum PersistentStorageMode {
	PSM_ENTER_GAMELOOP,   ///< Enter the gameloop, changes will be permanent.
	PSM_LEAVE_GAMELOOP,   ///< Leave the gameloop, changes will be temporary.
	PSM_ENTER_COMMAND,    ///< Enter command scope, changes will be permanent.
	PSM_LEAVE_COMMAND,    ///< Leave command scope, revert to previous mode.
	PSM_ENTER_TESTMODE,   ///< Enter command test mode, changes will be temporary.
	PSM_LEAVE_TESTMODE,   ///< Leave command test mode, revert to previous mode.
};

/**
 * Base class for all persistent NewGRF storage arrays. Nothing fancy, only here
 * so we have a generalised access to the virtual methods.
 */
struct BasePersistentStorageArray {
	uint32_t grfid;    ///< GRFID associated to this persistent storage. A value of zero means "default".
	byte feature;    ///< NOSAVE: Used to identify in the owner of the array in debug output.
	TileIndex tile;  ///< NOSAVE: Used to identify in the owner of the array in debug output.

	virtual ~BasePersistentStorageArray();

	static void SwitchMode(PersistentStorageMode mode, bool ignore_prev_mode = false);

protected:
	/**
	 * Discard temporary changes.
	 */
	virtual void ClearChanges() = 0;

	/**
	 * Check whether currently changes to the storage shall be persistent or
	 * temporary till the next call to ClearChanges().
	 */
	static bool AreChangesPersistent() { return (gameloop || command) && !testmode; }

private:
	static bool gameloop;
	static bool command;
	static bool testmode;
};

/**
 * Class for persistent storage of data.
 * On #ClearChanges that data is either reverted or saved.
 * @tparam TYPE the type of variable to store.
 * @tparam SIZE the size of the array.
 */
template <typename TYPE, uint SIZE>
struct PersistentStorageArray : BasePersistentStorageArray {
	using StorageType = std::array<TYPE, SIZE>;

	StorageType storage{}; ///< Memory for the storage array
	std::unique_ptr<StorageType> prev_storage{}; ///< Temporary memory to store previous state so it can be reverted, e.g. for command tests.

	/**
	 * Stores some value at a given position.
	 * If there is no backup of the data that backup is made and then
	 * we write the data.
	 * @param pos   the position to write at
	 * @param value the value to write
	 */
	void StoreValue(uint pos, int32_t value)
	{
		/* Out of the scope of the array */
		if (pos >= SIZE) return;

		/* The value hasn't changed, so we pretend nothing happened.
		 * Saves a few cycles and such and it's pretty easy to check. */
		if (this->storage[pos] == value) return;

		/* We do not have made a backup; lets do so */
		if (AreChangesPersistent()) {
			assert(!this->prev_storage);
		} else if (!this->prev_storage) {
			this->prev_storage = std::make_unique<StorageType>(this->storage);

			/* We only need to register ourselves when we made the backup
			 * as that is the only time something will have changed */
			AddChangedPersistentStorage(this);
		}

		this->storage[pos] = value;
	}

	/**
	 * Gets the value from a given position.
	 * @param pos the position to get the data from
	 * @return the data from that position
	 */
	TYPE GetValue(uint pos) const
	{
		/* Out of the scope of the array */
		if (pos >= SIZE) return 0;

		return this->storage[pos];
	}

	void ClearChanges() override
	{
		if (this->prev_storage) {
			this->storage = *this->prev_storage;
			this->prev_storage.reset();
		}
	}
};


/**
 * Class for temporary storage of data.
 * On #ClearChanges that data is always zero-ed.
 * @tparam TYPE the type of variable to store.
 * @tparam SIZE the size of the array.
 */
template <typename TYPE, uint SIZE>
struct TemporaryStorageArray {
	using StorageType = std::array<TYPE, SIZE>;
	using StorageInitType = std::array<uint16_t, SIZE>;

	StorageType storage{}; ///< Memory for the storage array
	StorageInitType init{}; ///< Storage has been assigned, if this equals 'init_key'.
	uint16_t init_key{1}; ///< Magic key to 'init'.

	/**
	 * Stores some value at a given position.
	 * @param pos   the position to write at
	 * @param value the value to write
	 */
	void StoreValue(uint pos, int32_t value)
	{
		/* Out of the scope of the array */
		if (pos >= SIZE) return;

		this->storage[pos] = value;
		this->init[pos] = this->init_key;
	}

	/**
	 * Gets the value from a given position.
	 * @param pos the position to get the data from
	 * @return the data from that position
	 */
	TYPE GetValue(uint pos) const
	{
		/* Out of the scope of the array */
		if (pos >= SIZE) return 0;

		if (this->init[pos] != this->init_key) {
			/* Unassigned since last call to ClearChanges */
			return 0;
		}

		return this->storage[pos];
	}

	void ClearChanges()
	{
		/* Increment init_key to invalidate all storage */
		this->init_key++;
		if (this->init_key == 0) {
			/* When init_key wraps around, we need to reset everything */
			this->init = {};
			this->init_key = 1;
		}
	}
};

void AddChangedPersistentStorage(BasePersistentStorageArray *storage);

typedef PersistentStorageArray<int32_t, 16> OldPersistentStorage;

typedef uint32_t PersistentStorageID;

struct PersistentStorage;
typedef Pool<PersistentStorage, PersistentStorageID, 1, 0xFF000> PersistentStoragePool;

extern PersistentStoragePool _persistent_storage_pool;

/**
 * Class for pooled persistent storage of data.
 */
struct PersistentStorage : PersistentStorageArray<int32_t, 256>, PersistentStoragePool::PoolItem<&_persistent_storage_pool> {
	/** We don't want GCC to zero our struct! It already is zeroed and has an index! */
	PersistentStorage(const uint32_t new_grfid, byte feature, TileIndex tile)
	{
		this->grfid = new_grfid;
		this->feature = feature;
		this->tile = tile;
	}
};

static_assert(cpp_lengthof(OldPersistentStorage, storage) <= cpp_lengthof(PersistentStorage, storage));

#endif /* NEWGRF_STORAGE_H */
