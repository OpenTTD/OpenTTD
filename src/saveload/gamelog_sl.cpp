/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gamelog_sl.cpp Code handling saving and loading of gamelog data. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/gamelog_sl_compat.h"

#include "../gamelog_internal.h"
#include "../fios.h"
#include "../string_func.h"

#include "../safeguards.h"


class SlGamelogMode : public DefaultSaveLoadHandler<SlGamelogMode, LoggedChange> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::U8>("mode.mode", SLE_OBJECT_ADDRESS(LoggedChangeMode, mode)),
		SaveLoad::Variable<VarFileType::U8>("mode.landscape", SLE_OBJECT_ADDRESS(LoggedChangeMode, landscape)),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_mode_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::Mode) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::Mode) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogRevision : public DefaultSaveLoadHandler<SlGamelogRevision, LoggedChange> {
public:
	static const size_t GAMELOG_REVISION_LENGTH = 15; ///< Length of the old revision text length.
	static inline std::array<char, GAMELOG_REVISION_LENGTH> revision_text; ///< Temporary location to store the old revision text.

	static inline const SaveLoad description[] = {
		SaveLoad::Array<VarFileType::U8, GAMELOG_REVISION_LENGTH>("revision.text", SLE_GLOBAL_ADDRESS(SlGamelogRevision::revision_text), SaveLoadVersion::MinVersion, SaveLoadVersion::StringGamelog),
		SaveLoad::String("revision.text", SLE_OBJECT_ADDRESS(LoggedChangeRevision, text), {}, SaveLoadVersion::StringGamelog),
		SaveLoad::Variable<VarFileType::U32>("revision.newgrf", SLE_OBJECT_ADDRESS(LoggedChangeRevision, newgrf)),
		SaveLoad::Variable<VarFileType::U16>("revision.slver", SLE_OBJECT_ADDRESS(LoggedChangeRevision, slver)),
		SaveLoad::Variable<VarFileType::U8>("revision.modified", SLE_OBJECT_ADDRESS(LoggedChangeRevision, modified)),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_revision_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::Revision) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::Revision) return;
		SlObject(lc, this->GetLoadDescription());

		if (IsSavegameVersionBefore(SaveLoadVersion::StringGamelog)) {
			static_cast<LoggedChangeRevision *>(lc)->text = StrMakeValid(std::string_view(std::begin(SlGamelogRevision::revision_text), std::end(SlGamelogRevision::revision_text)));
		}
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogOldver : public DefaultSaveLoadHandler<SlGamelogOldver, LoggedChange> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::U32>("oldver.type", SLE_OBJECT_ADDRESS(LoggedChangeOldVersion, type)),
		SaveLoad::Variable<VarFileType::U32>("oldver.version", SLE_OBJECT_ADDRESS(LoggedChangeOldVersion, version)),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_oldver_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::OldVer) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::OldVer) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogSetting : public DefaultSaveLoadHandler<SlGamelogSetting, LoggedChange> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::String("setting.name", SLE_OBJECT_ADDRESS(LoggedChangeSettingChanged, name)),
		SaveLoad::Variable<VarFileType::I32>("setting.oldval", SLE_OBJECT_ADDRESS(LoggedChangeSettingChanged, oldval)),
		SaveLoad::Variable<VarFileType::I32>("setting.newval", SLE_OBJECT_ADDRESS(LoggedChangeSettingChanged, newval)),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_setting_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::Setting) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::Setting) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfadd : public DefaultSaveLoadHandler<SlGamelogGrfadd, LoggedChange> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::Label>("grfadd.grfid", SLE_OBJECT_ADDRESS(LoggedChangeGRFAdd, grfid)),
		SaveLoad::Array<VarFileType::U8, MD5_HASH_BYTES>("grfadd.md5sum", SLE_OBJECT_ADDRESS(LoggedChangeGRFAdd, md5sum)),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_grfadd_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFAdd) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFAdd) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfrem : public DefaultSaveLoadHandler<SlGamelogGrfrem, LoggedChange> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::Label>("grfrem.grfid", SLE_OBJECT_ADDRESS(LoggedChangeGRFRemoved, grfid)),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_grfrem_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFRem) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFRem) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfcompat : public DefaultSaveLoadHandler<SlGamelogGrfcompat, LoggedChange> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::Label>("grfcompat.grfid", SLE_OBJECT_ADDRESS(LoggedChangeGRFChanged, grfid)),
		SaveLoad::Array<VarFileType::U8, MD5_HASH_BYTES>("grfcompat.md5sum", SLE_OBJECT_ADDRESS(LoggedChangeGRFChanged, md5sum)),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_grfcompat_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFCompat) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFCompat) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfparam : public DefaultSaveLoadHandler<SlGamelogGrfparam, LoggedChange> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::Label>("grfparam.grfid", SLE_OBJECT_ADDRESS(LoggedChangeGRFParameterChanged, grfid)),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_grfparam_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFParam) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFParam) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfmove : public DefaultSaveLoadHandler<SlGamelogGrfmove, LoggedChange> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::Label>("grfmove.grfid", SLE_OBJECT_ADDRESS(LoggedChangeGRFMoved, grfid)),
		SaveLoad::Variable<VarFileType::I32>("grfmove.offset", SLE_OBJECT_ADDRESS(LoggedChangeGRFMoved, offset)),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_grfmove_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFMove) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFMove) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

class SlGamelogGrfbug : public DefaultSaveLoadHandler<SlGamelogGrfbug, LoggedChange> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::U64>("grfbug.data", SLE_OBJECT_ADDRESS(LoggedChangeGRFBug, data)),
		SaveLoad::Variable<VarFileType::Label>("grfbug.grfid", SLE_OBJECT_ADDRESS(LoggedChangeGRFBug, grfid)),
		SaveLoad::Variable<VarFileType::U8>("grfbug.bug", SLE_OBJECT_ADDRESS(LoggedChangeGRFBug, bug)),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_grfbug_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFBug) return;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::GRFBug) return;
		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

static bool _is_emergency_save = true;

class SlGamelogEmergency : public DefaultSaveLoadHandler<SlGamelogEmergency, LoggedChange> {
public:
	/** We need to store something, so store a "true" value. */
	static inline const SaveLoad description[] = {
		SaveLoad::Variable<VarFileType::Bool>("is_emergency_save", SLE_GLOBAL_ADDRESS(_is_emergency_save), SaveLoadVersion::RiffToArray),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_emergency_sl_compat;

	void Save(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::Emergency) return;

		_is_emergency_save = true;
		SlObject(lc, this->GetDescription());
	}

	void Load(LoggedChange *lc) const override
	{
		if (lc->ct != GamelogChangeType::Emergency) return;

		SlObject(lc, this->GetLoadDescription());
	}

	void LoadCheck(LoggedChange *lc) const override { this->Load(lc); }
};

static std::unique_ptr<LoggedChange> MakeLoggedChange(GamelogChangeType type)
{
	switch (type) {
		case GamelogChangeType::Mode:      return std::make_unique<LoggedChangeMode>();
		case GamelogChangeType::Revision:  return std::make_unique<LoggedChangeRevision>();
		case GamelogChangeType::OldVer:    return std::make_unique<LoggedChangeOldVersion>();
		case GamelogChangeType::Setting:   return std::make_unique<LoggedChangeSettingChanged>();
		case GamelogChangeType::GRFAdd:    return std::make_unique<LoggedChangeGRFAdd>();
		case GamelogChangeType::GRFRem:    return std::make_unique<LoggedChangeGRFRemoved>();
		case GamelogChangeType::GRFCompat: return std::make_unique<LoggedChangeGRFChanged>();
		case GamelogChangeType::GRFParam:  return std::make_unique<LoggedChangeGRFParameterChanged>();
		case GamelogChangeType::GRFMove:   return std::make_unique<LoggedChangeGRFMoved>();
		case GamelogChangeType::GRFBug:    return std::make_unique<LoggedChangeGRFBug>();
		case GamelogChangeType::Emergency: return std::make_unique<LoggedChangeEmergencySave>();
		case GamelogChangeType::End:
		case GamelogChangeType::None:
		default:
			SlErrorCorrupt("Invalid gamelog action type");
	}
}

class SlGamelogAction : public DefaultSaveLoadHandler<SlGamelogAction, LoggedAction> {
public:
	static inline const SaveLoad description[] = {
		SaveLoad::SaveByte(SLE_NAME_AND_OBJECT_ADDRESS(LoggedChange, ct)),
		SaveLoad::Struct<SlGamelogMode>("mode"),
		SaveLoad::Struct<SlGamelogRevision>("revision"),
		SaveLoad::Struct<SlGamelogOldver>("oldver"),
		SaveLoad::Struct<SlGamelogSetting>("setting"),
		SaveLoad::Struct<SlGamelogGrfadd>("grfadd"),
		SaveLoad::Struct<SlGamelogGrfrem>("grfrem"),
		SaveLoad::Struct<SlGamelogGrfcompat>("grfcompat"),
		SaveLoad::Struct<SlGamelogGrfparam>("grfparam"),
		SaveLoad::Struct<SlGamelogGrfmove>("grfmove"),
		SaveLoad::Struct<SlGamelogGrfbug>("grfbug"),
		SaveLoad::Struct<SlGamelogEmergency>("emergency"),
	};
	static inline const SaveLoadCompatTable compat_description = _gamelog_action_sl_compat;

	void Save(LoggedAction *la) const override
	{
		SlSetStructListLength(la->change.size());

		for (auto &lc : la->change) {
			assert(lc->ct < GamelogChangeType::End);
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
		if (IsSavegameVersionBefore(SaveLoadVersion::RiffToArray)) {
			GamelogChangeType type;
			while ((type = static_cast<GamelogChangeType>(SlReadByte())) != GamelogChangeType::None) {
				if (type >= GamelogChangeType::End) SlErrorCorrupt("Invalid gamelog change type");
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
	SaveLoad::Variable<VarFileType::U8>(SLE_NAME_AND_OBJECT_ADDRESS(LoggedAction, at), SaveLoadVersion::RiffToArray),
	SaveLoad::Variable<VarFileType::U16>(SLE_NAME_AND_OBJECT_ADDRESS(LoggedAction, tick), SaveLoadVersion::MinVersion, SaveLoadVersion::U64TickCounter),
	SaveLoad::Variable<VarFileType::U64>(SLE_NAME_AND_OBJECT_ADDRESS(LoggedAction, tick), SaveLoadVersion::U64TickCounter),
	SaveLoad::StructList<SlGamelogAction>("action"),
};

struct GLOGChunkHandler : ChunkHandler {
	GLOGChunkHandler() : ChunkHandler("GLOG", ChunkType::Table) {}

	void LoadCommon(Gamelog &gamelog) const
	{
		assert(gamelog.data->action.empty());

		const std::vector<SaveLoad> slt = SlCompatTableHeader(_gamelog_desc, _gamelog_sl_compat);

		if (IsSavegameVersionBefore(SaveLoadVersion::RiffToArray)) {
			GamelogActionType type;
			while ((type = static_cast<GamelogActionType>(SlReadByte())) != GamelogActionType::None) {
				if (type >= GamelogActionType::End) SlErrorCorrupt("Invalid gamelog action type");

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
