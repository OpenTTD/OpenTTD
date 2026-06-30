/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file newgrf_sl.cpp Code handling saving and loading of newgrf config. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/newgrf_sl_compat.h"

#include "newgrf_sl.h"
#include "../fios.h"

#include "../safeguards.h"

/** Save and load the mapping between a spec and the NewGRF it came from. */
static const SaveLoad _newgrf_mapping_desc[] = {
	SLE_VAR(EntityIDMapping, grfid,         VarTypes::U32),
	SLE_CONDVAR(EntityIDMapping, entity_id, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::ExtendEntityMapping),
	SLE_CONDVAR(EntityIDMapping, entity_id, VarTypes::U16, SaveLoadVersion::ExtendEntityMapping, SaveLoadVersion::MaxVersion),
	SLE_CONDVAR(EntityIDMapping, substitute_id, VarFileType::U8 | VarMemType::U16, SaveLoadVersion::MinVersion, SaveLoadVersion::ExtendEntityMapping),
	SLE_CONDVAR(EntityIDMapping, substitute_id, VarTypes::U16, SaveLoadVersion::ExtendEntityMapping, SaveLoadVersion::MaxVersion),
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

struct NGRFChunkHandler : ChunkHandler {
	NGRFChunkHandler() : ChunkHandler("NGRF", ChunkType::Table) {}

	static inline std::array<uint32_t, GRFConfig::MAX_NUM_PARAMS> param;
	static inline uint8_t num_params;

	static inline const SaveLoad description[] = {
		   SLE_SSTR(GRFConfig, filename,         VarTypes::STR),
		    SLE_VAR(GRFConfig, ident.grfid,      VarTypes::U32),
		    SLE_ARR(GRFConfig, ident.md5sum,     VarTypes::U8,  16),
		SLE_CONDVAR(GRFConfig, version, VarTypes::U32, SaveLoadVersion::StoreNewGRFVersion, SaveLoadVersion::MaxVersion),
		   SLEG_ARR("param", param,              VarTypes::U32, std::size(param)),
		   SLEG_VAR("num_params", num_params,    VarTypes::U8),
		SLE_CONDVAR(GRFConfig, palette, VarTypes::U8, SaveLoadVersion::NewGRFPalette, SaveLoadVersion::MaxVersion),
	};

	void SaveParameters(const GRFConfig &config) const
	{
		/* Transfer config to fixed array, ensure unused entries are blanked. */
		param.fill(0);
		num_params = static_cast<uint8_t>(std::size(config.param));
		std::copy(std::begin(config.param), std::end(config.param), std::begin(param));
	}

	void Save() const override
	{
		SlTableHeader(description);

		int index = 0;

		for (const auto &c : _grfconfig) {
			if (c->flags.Any({GRFConfigFlag::Static, GRFConfigFlag::InitOnly})) continue;
			this->SaveParameters(*c);
			SlSetArrayIndex(index++);
			SlObject(c.get(), description);
		}
	}

	void LoadParameters(GRFConfig &config) const
	{
		/* Transfer used part of fixed array to config. */
		auto last = std::begin(param) + std::min<size_t>(std::size(param), num_params);
		config.param.assign(std::begin(param), last);
	}

	void LoadCommon(GRFConfigList &grfconfig) const
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(description, _grfconfig_sl_compat);

		ClearGRFConfigList(grfconfig);
		while (SlIterateArray() != -1) {
			auto c = std::make_unique<GRFConfig>();
			SlObject(c.get(), slt);
			if (IsSavegameVersionBefore(SaveLoadVersion::NewGRFPalette)) c->SetSuitablePalette();
			this->LoadParameters(*c);
			AppendToGRFConfigList(grfconfig, std::move(c));
		}
	}

	void Load() const override
	{
		this->LoadCommon(_grfconfig);

		if (_game_mode == GameMode::Menu) {
			/* Intro game must not have NewGRF. */
			if (!_grfconfig.empty()) SlErrorCorrupt("The intro game must not use NewGRF");

			/* Activate intro NewGRFs (townnames) */
			ResetGRFConfig(false);
		} else {
			/* Append static NewGRF configuration */
			AppendStaticGRFConfigs(_grfconfig);
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
