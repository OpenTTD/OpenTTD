/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gamelog_sl.cpp Code handling saving and loading of gamelog data */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/gamelog_sl_compat.h"

#include "../gamelog_internal.h"
#include "../fios.h"
#include "../string_func.h"

#include "../safeguards.h"


class SlGamelogMode : public DefaultSaveLoadHandler<SlGamelogMode, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VARNAME(LoggedChangeMode, mode,      "mode.mode",      SLE_UINT8),
		SLE_VARNAME(LoggedChangeMode, landscape, "mode.landscape", SLE_UINT8),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_mode_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_MODE) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_MODE) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogRevision : public DefaultSaveLoadHandler<SlGamelogRevision, LoggedChange> {
public:
	static const size_t GAMELOG_REVISION_LENGTH = 15;
	static char revision_text[GAMELOG_REVISION_LENGTH];

	inline static const SaveLoad description[] = {
		    SLEG_CONDARR("revision.text", SlGamelogRevision::revision_text,   SLE_UINT8, GAMELOG_REVISION_LENGTH, SL_MIN_VERSION,     SLV_STRING_GAMELOG),
		SLE_CONDSSTRNAME(LoggedChangeRevision, text,     "revision.text",     SLE_STR,                            SLV_STRING_GAMELOG, SL_MAX_VERSION),
		     SLE_VARNAME(LoggedChangeRevision, newgrf,   "revision.newgrf",   SLE_UINT32),
		     SLE_VARNAME(LoggedChangeRevision, slver,    "revision.slver",    SLE_UINT16),
		     SLE_VARNAME(LoggedChangeRevision, modified, "revision.modified", SLE_UINT8),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_revision_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_REVISION) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_REVISION) return;
		SlObject(lc, this->GetLoadDescription());

		if (IsSavegameVersionBefore(SLV_STRING_GAMELOG)) {
			static_cast<LoggedChangeRevision *>(lc)->text = StrMakeValid(std::string_view(SlGamelogRevision::revision_text, std::size(SlGamelogRevision::revision_text)));
		}
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

/* static */ char SlGamelogRevision::revision_text[GAMELOG_REVISION_LENGTH];

class SlGamelogOldver : public DefaultSaveLoadHandler<SlGamelogOldver, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VARNAME(LoggedChangeOldVersion, type,    "oldver.type",    SLE_UINT32),
		SLE_VARNAME(LoggedChangeOldVersion, version, "oldver.version", SLE_UINT32),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_oldver_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_OLDVER) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_OLDVER) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogSetting : public DefaultSaveLoadHandler<SlGamelogSetting, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_SSTRNAME(LoggedChangeSettingChanged, name,   "setting.name",   SLE_STR),
		 SLE_VARNAME(LoggedChangeSettingChanged, oldval, "setting.oldval", SLE_INT32),
		 SLE_VARNAME(LoggedChangeSettingChanged, newval, "setting.newval", SLE_INT32),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_setting_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_SETTING) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_SETTING) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfadd : public DefaultSaveLoadHandler<SlGamelogGrfadd, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VARNAME(LoggedChangeGRFAdd, grfid,  "grfadd.grfid",  SLE_UINT32    ),
		SLE_ARRNAME(LoggedChangeGRFAdd, md5sum, "grfadd.md5sum", SLE_UINT8,  16),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfadd_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFADD) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFADD) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfrem : public DefaultSaveLoadHandler<SlGamelogGrfrem, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VARNAME(LoggedChangeGRFRemoved, grfid, "grfrem.grfid", SLE_UINT32),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfrem_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFREM) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFREM) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfcompat : public DefaultSaveLoadHandler<SlGamelogGrfcompat, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VARNAME(LoggedChangeGRFChanged, grfid,  "grfcompat.grfid",  SLE_UINT32    ),
		SLE_ARRNAME(LoggedChangeGRFChanged, md5sum, "grfcompat.md5sum", SLE_UINT8,  16),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfcompat_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFCOMPAT) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFCOMPAT) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfparam : public DefaultSaveLoadHandler<SlGamelogGrfparam, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VARNAME(LoggedChangeGRFParameterChanged, grfid, "grfparam.grfid", SLE_UINT32),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfparam_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFPARAM) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFPARAM) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfmove : public DefaultSaveLoadHandler<SlGamelogGrfmove, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VARNAME(LoggedChangeGRFMoved, grfid,  "grfmove.grfid",  SLE_UINT32),
		SLE_VARNAME(LoggedChangeGRFMoved, offset, "grfmove.offset", SLE_INT32),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfmove_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFMOVE) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFMOVE) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfbug : public DefaultSaveLoadHandler<SlGamelogGrfbug, LoggedChange> {
public:
	inline static const SaveLoad description[] = {
		SLE_VARNAME(LoggedChangeGRFBug, data,  "grfbug.data",  SLE_UINT64),
		SLE_VARNAME(LoggedChangeGRFBug, grfid, "grfbug.grfid", SLE_UINT32),
		SLE_VARNAME(LoggedChangeGRFBug, bug,   "grfbug.bug",   SLE_UINT8),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_grfbug_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFBUG) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_GRFBUG) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

static bool _is_emergency_save = true;

class SlGamelogEmergency : public DefaultSaveLoadHandler<SlGamelogEmergency, LoggedChange> {
public:
	/* We need to store something, so store a "true" value. */
	inline static const SaveLoad description[] = {
		SLEG_CONDVAR("is_emergency_save", _is_emergency_save, SLE_BOOL, SLV_RIFF_TO_ARRAY, SL_MAX_VERSION),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_emergency_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_EMERGENCY) return;

		_is_emergency_save = true;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GLCT_EMERGENCY) return;

		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

static std::unique_ptr<LoggedChange> MakeLoggedChange(GamelogChangeType type)
{
	switch (type) {
		case GLCT_MODE:      return std::make_unique<LoggedChangeMode>();
		case GLCT_REVISION:  return std::make_unique<LoggedChangeRevision>();
		case GLCT_OLDVER:    return std::make_unique<LoggedChangeOldVersion>();
		case GLCT_SETTING:   return std::make_unique<LoggedChangeSettingChanged>();
		case GLCT_GRFADD:    return std::make_unique<LoggedChangeGRFAdd>();
		case GLCT_GRFREM:    return std::make_unique<LoggedChangeGRFRemoved>();
		case GLCT_GRFCOMPAT: return std::make_unique<LoggedChangeGRFChanged>();
		case GLCT_GRFPARAM:  return std::make_unique<LoggedChangeGRFParameterChanged>();
		case GLCT_GRFMOVE:   return std::make_unique<LoggedChangeGRFMoved>();
		case GLCT_GRFBUG:    return std::make_unique<LoggedChangeGRFBug>();
		case GLCT_EMERGENCY: return std::make_unique<LoggedChangeEmergencySave>();
		case GLCT_END:
		case GLCT_NONE:
		default:
			SlErrorCorrupt("Invalid gamelog action type");
	}
}

class SlGamelogAction : public DefaultSaveLoadHandler<SlGamelogAction, LoggedAction> {
public:
	inline static const SaveLoad description[] = {
		SLE_SAVEBYTE(LoggedChange, ct),
		SLEG_STRUCT("mode", SlGamelogMode),
		SLEG_STRUCT("revision", SlGamelogRevision),
		SLEG_STRUCT("oldver", SlGamelogOldver),
		SLEG_STRUCT("setting", SlGamelogSetting),
		SLEG_STRUCT("grfadd", SlGamelogGrfadd),
		SLEG_STRUCT("grfrem", SlGamelogGrfrem),
		SLEG_STRUCT("grfcompat", SlGamelogGrfcompat),
		SLEG_STRUCT("grfparam", SlGamelogGrfparam),
		SLEG_STRUCT("grfmove", SlGamelogGrfmove),
		SLEG_STRUCT("grfbug", SlGamelogGrfbug),
		SLEG_STRUCT("emergency", SlGamelogEmergency),
	};
	inline const static SaveLoadCompatTable compat_description = _gamelog_action_sl_compat;

	void Save(LoggedAction *la) const override
	{
		SlSetStructListLength(la->change.size());

		for (auto &lc : la->change) {
			assert(lc->ct < GLCT_END);
			SlObject(lc.get(), this->GetDescription());
		}
	}

	void LoadChange(LoggedAction *la, GamelogChangeType type) const
	{
		std::unique_ptr<LoggedChange> lc = MakeLoggedChange(type);
		SlObject(lc.get(), this->GetLoadDescription());
		la->change.push_back(std::move(lc));
	}

	void Load(LoggedAction *la) const override
	{
		if (IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY)) {
			uint8_t type;
			while ((type = SlReadByte()) != GLCT_NONE) {
				if (type >= GLCT_END) SlErrorCorrupt("Invalid gamelog change type");
				LoadChange(la, (GamelogChangeType)type);
			}
			return;
		}

		size_t length = SlGetStructListLength(UINT32_MAX);
		la->change.reserve(length);

		for (size_t i = 0; i < length; i++) {
			LoadChange(la, (GamelogChangeType)SlReadByte());
		}
	}

	void LoadCheck(LoggedAction *la) const override { this->Load(la); }
};

static const SaveLoad _gamelog_desc[] = {
	SLE_CONDVAR(LoggedAction, at,            SLE_UINT8,   SLV_RIFF_TO_ARRAY, SL_MAX_VERSION),
	SLE_CONDVAR(LoggedAction, tick, SLE_FILE_U16 | SLE_VAR_U64, SL_MIN_VERSION, SLV_U64_TICK_COUNTER),
	SLE_CONDVAR(LoggedAction, tick, SLE_UINT64,                 SLV_U64_TICK_COUNTER, SL_MAX_VERSION),
	SLEG_STRUCTLIST("action", SlGamelogAction),
};

struct GLOGChunkHandler : ChunkHandler {
	GLOGChunkHandler() : ChunkHandler('GLOG', CH_TABLE) {}

	void LoadCommon(Gamelog &gamelog) const
	{
		assert(gamelog.data->action.empty());

		const std::vector<SaveLoad> slt = SlCompatTableHeader(_gamelog_desc, _gamelog_sl_compat);

		if (IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY)) {
			uint8_t type;
			while ((type = SlReadByte()) != GLAT_NONE) {
				if (type >= GLAT_END) SlErrorCorrupt("Invalid gamelog action type");

				LoggedAction &la = gamelog.data->action.emplace_back();
				la.at = (GamelogActionType)type;
				SlObject(&la, slt);
			}
			return;
		}

		while (SlIterateArray() != -1) {
			LoggedAction &la = gamelog.data->action.emplace_back();
			SlObject(&la, slt);
		}
	}

	void Save() const override
	{
		SlTableHeader(_gamelog_desc);

		uint i = 0;
		for (LoggedAction &la : _gamelog.data->action) {
			SlSetArrayIndex(i++);
			SlObject(&la, _gamelog_desc);
		}
	}

	void Load() const override
	{
		this->LoadCommon(_gamelog);
	}

	void LoadCheck(size_t) const override
	{
		this->LoadCommon(_load_check_data.gamelog);
	}
};

static const GLOGChunkHandler GLOG;
static const ChunkHandlerRef gamelog_chunk_handlers[] = {
	GLOG,
};

extern const ChunkHandlerTable _gamelog_chunk_handlers(gamelog_chunk_handlers);
