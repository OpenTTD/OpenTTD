/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file object_sl.cpp Code handling saving and loading of objects. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/object_sl_compat.h"

#include "../object_base.h"
#include "../object_map.h"
#include "../town.h"
#include "newgrf_sl.h"

#include "../safeguards.h"

static const SaveLoad _object_desc[] = {
	SaveLoad::Variable<VarFileType::U32>(SLE_NAME_AND_OBJECT_ADDRESS(Object, location.tile)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Object, location.w)),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Object, location.h)),
	SaveLoad::Reference<SLRefType::Town>(SLE_NAME_AND_OBJECT_ADDRESS(Object, town)),
	SaveLoad::Variable<VarFileType::U32>(SLE_NAME_AND_OBJECT_ADDRESS(Object, build_date)),
	SaveLoad::Variable<VarFileType::U8>("colour", SLE_OBJECT_ADDRESS(Object, recolour_offset), SaveLoadVersion::IndustryPlatform),
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(Object, view), SaveLoadVersion::NewGRFObjectView),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(Object, type), SaveLoadVersion::ObjectTypeToPool),
};

struct OBJSChunkHandler : ChunkHandler {
	OBJSChunkHandler() : ChunkHandler("OBJS", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_object_desc);

		/* Write the objects */
		for (Object *o : Object::Iterate()) {
			SlSetArrayIndex(o->index);
			SlObject(o, _object_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_object_desc, _object_sl_compat);

		int index;
		while ((index = SlIterateArray()) != -1) {
			Object *o = Object::CreateAtIndex(ObjectID(index));
			SlObject(o, slt);
		}
	}

	void FixPointers() const override
	{
		for (Object *o : Object::Iterate()) {
			SlObject(o, _object_desc);
			if (IsSavegameVersionBefore(SaveLoadVersion::IndustryPlatform) && !IsTileType(o->location.tile, TileType::Object)) {
				/* Due to a small bug stale objects could remain. */
				delete o;
			}
		}
	}
};

struct OBIDChunkHandler : NewGRFMappingChunkHandler {
	OBIDChunkHandler() : NewGRFMappingChunkHandler("OBID", _object_mngr) {}
};

static const OBIDChunkHandler OBID;
static const OBJSChunkHandler OBJS;
static const ChunkHandlerRef object_chunk_handlers[] = {
	OBID,
	OBJS,
};

extern const ChunkHandlerTable _object_chunk_handlers(object_chunk_handlers);
