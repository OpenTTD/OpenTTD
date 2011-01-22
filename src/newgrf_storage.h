/* $Id$ */

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

/**
 * Base class for all NewGRF storage arrays. Nothing fancy, only here
 * so we have a generalised class to use.
 */
struct BaseStorageArray
{
	/** The needed destructor */
	virtual ~BaseStorageArray() {}

	/**
	 * Clear the changes made since the last ClearChanges.
	 * This can be done in two ways:
	 *  - saving the changes permanently
	 *  - reverting to the previous version
	 * @param keep_changes do we save or revert the changes since the last ClearChanges?
	 */
	virtual void ClearChanges(bool keep_changes) = 0;

	/**
	 * Stores some value at a given position.
	 * @param pos   the position to write at
	 * @param value the value to write
	 */
	virtual void Store(uint pos, int32 value) = 0;
};

/**
 * Class for persistent storage of data.
 * On ClearChanges that data is either reverted or saved.
 * @param TYPE the type of variable to store.
 * @param SIZE the size of the array.
 */
template <typename TYPE, uint SIZE>
struct PersistentStorageArray : BaseStorageArray {
	TYPE storage[SIZE]; ///< Memory to for the storage array
	TYPE *prev_storage; ///< Memory to store "old" states so we can revert them on the performance of test cases for commands etc.

	/** Simply construct the array */
	PersistentStorageArray() : prev_storage(NULL)
	{
		memset(this->storage, 0, sizeof(this->storage));
	}

	/** And free all data related to it */
	~PersistentStorageArray()
	{
		free(this->prev_storage);
	}

	/**
	 * Stores some value at a given position.
	 * If there is no backup of the data that backup is made and then
	 * we write the data.
	 * @param pos   the position to write at
	 * @param value the value to write
	 */
	void Store(uint pos, int32 value)
	{
		/* Out of the scope of the array */
		if (pos >= SIZE) return;

		/* The value hasn't changed, so we pretend nothing happened.
		 * Saves a few cycles and such and it's pretty easy to check. */
		if (this->storage[pos] == value) return;

		/* We do not have made a backup; lets do so */
		if (this->prev_storage != NULL) {
			this->prev_storage = MallocT<TYPE>(SIZE);
			memcpy(this->prev_storage, this->storage, sizeof(this->storage));

			/* We only need to register ourselves when we made the backup
			 * as that is the only time something will have changed */
			AddChangedStorage(this);
		}

		this->storage[pos] = value;
	}

	/**
	 * Gets the value from a given position.
	 * @param pos the position to get the data from
	 * @return the data from that position
	 */
	TYPE Get(uint pos) const
	{
		/* Out of the scope of the array */
		if (pos >= SIZE) return 0;

		return this->storage[pos];
	}

	/**
	 * Clear the changes, or assign them permanently to the storage.
	 * @param keep_changes Whether to assign or ditch the changes.
	 */
	void ClearChanges(bool keep_changes)
	{
		assert(this->prev_storage != NULL);

		if (!keep_changes) {
			memcpy(this->storage, this->prev_storage, sizeof(this->storage));
		}
		free(this->prev_storage);
	}
};


/**
 * Class for temporary storage of data.
 * On ClearChanges that data is always zero-ed.
 * @param TYPE the type of variable to store.
 * @param SIZE the size of the array.
 */
template <typename TYPE, uint SIZE>
struct TemporaryStorageArray : BaseStorageArray {
	TYPE storage[SIZE]; ///< Memory to for the storage array

	/** Simply construct the array */
	TemporaryStorageArray()
	{
		memset(this->storage, 0, sizeof(this->storage));
	}

	/**
	 * Stores some value at a given position.
	 * @param pos   the position to write at
	 * @param value the value to write
	 */
	void Store(uint pos, int32 value)
	{
		/* Out of the scope of the array */
		if (pos >= SIZE) return;

		this->storage[pos] = value;
		AddChangedStorage(this);
	}

	/**
	 * Gets the value from a given position.
	 * @param pos the position to get the data from
	 * @return the data from that position
	 */
	TYPE Get(uint pos) const
	{
		/* Out of the scope of the array */
		if (pos >= SIZE) return 0;

		return this->storage[pos];
	}

	void ClearChanges(bool keep_changes)
	{
		memset(this->storage, 0, sizeof(this->storage));
	}
};

void AddChangedStorage(BaseStorageArray *storage);
void ClearStorageChanges(bool keep_changes);

#endif /* NEWGRF_STORAGE_H */
