/* $Id$ */

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
