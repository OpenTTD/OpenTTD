/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object_sl.cpp Code handling saving and loading of objects */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/object_sl_compat.h"

#include "../object_base.h"
#include "../object_map.h"
#include "newgrf_sl.h"

#include "../safeguards.h"

static const SaveLoad _object_desc[] = {
	    SLE_VAR(Object, location.tile,              SLE_UINT32),
	    SLE_VAR(Object, location.w,                 SLE_FILE_U8 | SLE_VAR_U16),
	    SLE_VAR(Object, location.h,                 SLE_FILE_U8 | SLE_VAR_U16),
	    SLE_REF(Object, town,                       REF_TOWN),
	    SLE_VAR(Object, build_date,                 SLE_UINT32),
	SLE_CONDVAR(Object, colour,                     SLE_UINT8,                  SLV_148, SL_MAX_VERSION),
	SLE_CONDVAR(Object, view,                       SLE_UINT8,                  SLV_155, SL_MAX_VERSION),
	SLE_CONDVAR(Object, type,                       SLE_UINT16,                 SLV_186, SL_MAX_VERSION),
};

static void Save_OBJS()
{
	SlTableHeader(_object_desc);

	/* Write the objects */
	for (Object *o : Object::Iterate()) {
		SlSetArrayIndex(o->index);
		SlObject(o, _object_desc);
	}
}

static void Load_OBJS()
{
	const std::vector<SaveLoad> slt = SlCompatTableHeader(_object_desc, _object_sl_compat);

	int index;
	while ((index = SlIterateArray()) != -1) {
		Object *o = new (index) Object();
		SlObject(o, slt);
	}
}

static void Ptrs_OBJS()
{
	for (Object *o : Object::Iterate()) {
		SlObject(o, _object_desc);
		if (IsSavegameVersionBefore(SLV_148) && !IsTileType(o->location.tile, MP_OBJECT)) {
			/* Due to a small bug stale objects could remain. */
			delete o;
		}
	}
}

static void Save_OBID()
{
	Save_NewGRFMapping(_object_mngr);
}

static void Load_OBID()
{
	Load_NewGRFMapping(_object_mngr);
}

static const ChunkHandler object_chunk_handlers[] = {
	{ 'OBID', Save_OBID, Load_OBID, nullptr,   nullptr, CH_TABLE },
	{ 'OBJS', Save_OBJS, Load_OBJS, Ptrs_OBJS, nullptr, CH_TABLE },
};

extern const ChunkHandlerTable _object_chunk_handlers(object_chunk_handlers);
