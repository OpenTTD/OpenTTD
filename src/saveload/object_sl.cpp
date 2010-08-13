/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object_sl.cpp Code handling saving and loading of objects */

#include "../stdafx.h"
#include "../object_base.h"

#include "saveload.h"

static const SaveLoad _object_desc[] = {
	    SLE_VAR(Object, location.tile,              SLE_UINT32),
	    SLE_VAR(Object, location.w,                 SLE_FILE_U8 | SLE_VAR_U16),
	    SLE_VAR(Object, location.h,                 SLE_FILE_U8 | SLE_VAR_U16),
	    SLE_REF(Object, town,                       REF_TOWN),
	    SLE_VAR(Object, build_date,                 SLE_UINT32),

	SLE_END()
};

static void Save_OBJS()
{
	Object *o;

	/* Write the objects */
	FOR_ALL_OBJECTS(o) {
		SlSetArrayIndex(o->index);
		SlObject(o, _object_desc);
	}
}

static void Load_OBJS()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Object *o = new (index) Object();
		SlObject(o, _object_desc);
	}
}

static void Ptrs_OBJS()
{
	Object *o;
	FOR_ALL_OBJECTS(o) {
		SlObject(o, _object_desc);
	}
}

extern const ChunkHandler _object_chunk_handlers[] = {
	{ 'OBJS', Save_OBJS, Load_OBJS, Ptrs_OBJS, NULL, CH_ARRAY | CH_LAST},
};
