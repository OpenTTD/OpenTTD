/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_sl.cpp Code handling saving and loading of newgrf config */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/newgrf_sl_compat.h"

#include "newgrf_sl.h"
#include "../fios.h"

#include "../safeguards.h"

/** Save and load the mapping between a spec and the NewGRF it came from. */
static const SaveLoad _newgrf_mapping_desc[] = {
	SLE_VAR(EntityIDMapping, grfid,         SLE_UINT32),
	SLE_CONDVAR(EntityIDMapping, entity_id,     SLE_FILE_U8 | SLE_VAR_U16, SL_MIN_VERSION,            SLV_EXTEND_ENTITY_MAPPING),
	SLE_CONDVAR(EntityIDMapping, entity_id,     SLE_UINT16,                SLV_EXTEND_ENTITY_MAPPING, SL_MAX_VERSION),
	SLE_CONDVAR(EntityIDMapping, substitute_id, SLE_FILE_U8 | SLE_VAR_U16, SL_MIN_VERSION,            SLV_EXTEND_ENTITY_MAPPING),
	SLE_CONDVAR(EntityIDMapping, substitute_id, SLE_UINT16,                SLV_EXTEND_ENTITY_MAPPING, SL_MAX_VERSION),
};

/**
 * Save a GRF ID + local id -> OpenTTD's id mapping.
 */
void NewGRFMappingChunkHandler::Save() const
{
	SlTableHeader(_newgrf_mapping_desc);

	for (uint i = 0; i < this->mapping.GetMaxMapping(); i++) {
		if (this->mapping.mappings[i].grfid == 0 &&
			this->mapping.mappings[i].entity_id == 0) continue;
		SlSetArrayIndex(i);
		SlObject(&this->mapping.mappings[i], _newgrf_mapping_desc);
	}
}

/**
 * Load a GRF ID + local id -> OpenTTD's id mapping.
 */
void NewGRFMappingChunkHandler::Load() const
{
	const std::vector<SaveLoad> slt = SlCompatTableHeader(_newgrf_mapping_desc, _newgrf_mapping_sl_compat);

	/* Clear the current mapping stored.
	 * This will create the manager if ever it is not yet done */
	this->mapping.ResetMapping();

	uint max_id = this->mapping.GetMaxMapping();

	int index;
	while ((index = SlIterateArray()) != -1) {
		if ((uint)index >= max_id) SlErrorCorrupt("Too many NewGRF entity mappings");
		SlObject(&this->mapping.mappings[index], slt);
	}
}


static const SaveLoad _grfconfig_desc[] = {
	   SLE_SSTR(GRFConfig, filename,         SLE_STR),
	    SLE_VAR(GRFConfig, ident.grfid,      SLE_UINT32),
	    SLE_ARR(GRFConfig, ident.md5sum,     SLE_UINT8,  16),
	SLE_CONDVAR(GRFConfig, version,          SLE_UINT32, SLV_151, SL_MAX_VERSION),
	    SLE_ARR(GRFConfig, param,            SLE_UINT32, 0x80),
	    SLE_VAR(GRFConfig, num_params,       SLE_UINT8),
	SLE_CONDVAR(GRFConfig, palette,          SLE_UINT8,  SLV_101, SL_MAX_VERSION),
};


struct NGRFChunkHandler : ChunkHandler {
	NGRFChunkHandler() : ChunkHandler('NGRF', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_grfconfig_desc);

		int index = 0;

		for (GRFConfig *c = _grfconfig; c != nullptr; c = c->next) {
			if (HasBit(c->flags, GCF_STATIC) || HasBit(c->flags, GCF_INIT_ONLY)) continue;
			SlSetArrayIndex(index++);
			SlObject(c, _grfconfig_desc);
		}
	}


	void LoadCommon(GRFConfig *&grfconfig) const
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_grfconfig_desc, _grfconfig_sl_compat);

		ClearGRFConfigList(&grfconfig);
		while (SlIterateArray() != -1) {
			GRFConfig *c = new GRFConfig();
			SlObject(c, slt);
			if (IsSavegameVersionBefore(SLV_101)) c->SetSuitablePalette();
			AppendToGRFConfigList(&grfconfig, c);
		}
	}

	void Load() const override
	{
		this->LoadCommon(_grfconfig);

		if (_game_mode == GM_MENU) {
			/* Intro game must not have NewGRF. */
			if (_grfconfig != nullptr) SlErrorCorrupt("The intro game must not use NewGRF");

			/* Activate intro NewGRFs (townnames) */
			ResetGRFConfig(false);
		} else {
			/* Append static NewGRF configuration */
			AppendStaticGRFConfigs(&_grfconfig);
		}
	}

	void LoadCheck(size_t) const override
	{
		this->LoadCommon(_load_check_data.grfconfig);
	}
};

static const NGRFChunkHandler NGRF;
static const ChunkHandlerRef newgrf_chunk_handlers[] = {
	NGRF,
};

extern const ChunkHandlerTable _newgrf_chunk_handlers(newgrf_chunk_handlers);
