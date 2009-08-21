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

void AddChangedStorage(BaseStorageArray *storage)
{
	_changed_storage_arrays.insert(storage);
}

void ClearStorageChanges(bool keep_changes)
{
	/* Loop over all changes arrays */
	for (std::set<BaseStorageArray*>::iterator it = _changed_storage_arrays.begin(); it != _changed_storage_arrays.end(); it++) {
		(*it)->ClearChanges(keep_changes);
	}

	/* And then clear that array */
	_changed_storage_arrays.clear();
}
