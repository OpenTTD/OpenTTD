/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_storage.cpp Functionality related to the temporary and persistent storage arrays for NewGRFs. */

#include "stdafx.h"
#include "newgrf_storage.h"
#include "core/pool_func.hpp"
#include "core/endian_func.hpp"
#include "debug.h"

#include "safeguards.h"

PersistentStoragePool _persistent_storage_pool("PersistentStorage");
INSTANTIATE_POOL_METHODS(PersistentStorage)

/** The changed storage arrays */
static std::set<BasePersistentStorageArray*> *_changed_storage_arrays = new std::set<BasePersistentStorageArray*>;

bool BasePersistentStorageArray::gameloop;
bool BasePersistentStorageArray::command;
bool BasePersistentStorageArray::testmode;

/**
 * Remove references to use.
 */
BasePersistentStorageArray::~BasePersistentStorageArray()
{
	_changed_storage_arrays->erase(this);
}

/**
 * Add the changed storage array to the list of changed arrays.
 * This is done so we only have to revert/save the changed
 * arrays, which saves quite a few clears, etc. after callbacks.
 * @param storage the array that has changed
 */
void AddChangedPersistentStorage(BasePersistentStorageArray *storage)
{
	_changed_storage_arrays->insert(storage);
}

/**
 * Clear temporary changes made since the last call to SwitchMode, and
 * set whether subsequent changes shall be persistent or temporary.
 *
 * @param mode Mode switch affecting temporary/persistent changes.
 * @param ignore_prev_mode Disable some sanity checks for exceptional call circumstances.
 */
/* static */ void BasePersistentStorageArray::SwitchMode(PersistentStorageMode mode, [[maybe_unused]] bool ignore_prev_mode)
{
	switch (mode) {
		case PSM_ENTER_GAMELOOP:
			assert(ignore_prev_mode || !gameloop);
			assert(!command && !testmode);
			gameloop = true;
			break;

		case PSM_LEAVE_GAMELOOP:
			assert(ignore_prev_mode || gameloop);
			assert(!command && !testmode);
			gameloop = false;
			break;

		case PSM_ENTER_COMMAND:
			assert((ignore_prev_mode || !command) && !testmode);
			command = true;
			break;

		case PSM_LEAVE_COMMAND:
			assert(ignore_prev_mode || command);
			command = false;
			break;

		case PSM_ENTER_TESTMODE:
			assert(!command && (ignore_prev_mode || !testmode));
			testmode = true;
			break;

		case PSM_LEAVE_TESTMODE:
			assert(ignore_prev_mode || testmode);
			testmode = false;
			break;

		default: NOT_REACHED();
	}

	/* Discard all temporary changes */
	for (auto &it : *_changed_storage_arrays) {
		Debug(desync, 1, "Discarding persistent storage changes: Feature {}, GrfID {:08X}, Tile {}", it->feature, BSWAP32(it->grfid), it->tile);
		it->ClearChanges();
	}
	_changed_storage_arrays->clear();
}
