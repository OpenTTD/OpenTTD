/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_storage.cpp Functionality related to the temporary and persistent storage arrays for NewGRFs. */

#include "stdafx.h"
#include "newgrf_storage.h"
#include <set>

/** The changed storage arrays */
static std::set<BaseStorageArray*> _changed_storage_arrays;

/**
 * Add the changed storage array to the list of changed arrays.
 * This is done so we only have to revert/save the changed
 * arrays, which saves quite a few clears, etc. after callbacks.
 * @param storage the array that has changed
 */
void AddChangedStorage(BaseStorageArray *storage)
{
	_changed_storage_arrays.insert(storage);
}

/**
 * Clear the changes made since the last ClearStorageChanges.
 * This is done for *all* storages that have been registered to with
 * AddChangedStorage since the previous ClearStorageChanges.
 *
 * This can be done in two ways:
 *  - saving the changes permanently
 *  - reverting to the previous version
 * @param keep_changes do we save or revert the changes since the last ClearChanges?
 */
void ClearStorageChanges(bool keep_changes)
{
	/* Loop over all changes arrays */
	for (std::set<BaseStorageArray*>::iterator it = _changed_storage_arrays.begin(); it != _changed_storage_arrays.end(); it++) {
		(*it)->ClearChanges(keep_changes);
	}

	/* And then clear that array */
	_changed_storage_arrays.clear();
}
